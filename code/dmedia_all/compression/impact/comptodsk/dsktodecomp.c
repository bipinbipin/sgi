/*
** dsktodecomp - decompress data from disk to video
*/

#include <errno.h>
#include <sys/lock.h>
#include <sys/schedctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <getopt.h>
#include <bstring.h>
#include <values.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>
#include <sgidefs.h>

#include <dmedia/vl.h>
#include <vl/dev_mgv.h>

#include <dmedia/cl.h>
#include <dmedia/cl_impactcomp.h>

#include "mjfif.h"

#define CL_DATA_RING	    (20 * 1024 * 1024)	    /* 20 Megabytes */
#define	CHUNK_SIZE	    (256 * 1024)	    /* stripe unit */

/*
** usage message
*/
void
usage( char * progname )
{
    fprintf(stderr, "%s: [-v videonode] "
		    "-f filename\n"
		    "\t-f\tinput file name\n"
		    "\t-v\tvideo drain node number\n", progname );
}

/*
** application guts
*/
main( int argc, char * argv[] )
{
    char  * progname = argv[0]; 
    int	    c;		
    int	    vout_node = VL_ANY;		/* video input node */
    int	    imageCount = 0;		/* number of images to played */
    char  * infile = NULL;		/* input file name */
    int	    rc;				/* return code */
    int	    ncaptured;			/* fields actually captured */

    /* video library stuff */
    VLServer	svr;			/* video server connection */
    VLNode	src, drn;		/* source, drain nodes */
    VLPath	path;			/* path */
    VLControlValue  val;		/* vl get/set control value */

    /* compression library stuff */
    CLhandle	clhandle;		/* cl handle */
    int		scheme;			/* compression library scheme */
    int		n, paramBuf[50];	/* cl parameters */
    CLbufferHdl dataRB;			/* compressed data ring buffer */
    int		codec_cookie;		/* magic cl->vl codec cookie */
    int		ringsize;		/* size of the compressed ring */
    int		preWrap;		/* unwrapped portion of ring buffer */
    int		ignored;		/* wrapped portion of ring */
    char      * dataPtr;		/* available valid data */
    char      * offset;			/* offset in rb to read into */
    int		toRead;			/* how many bytes to read */
    __int64_t	nRead = 0;		/* number of bytes read from file */
    int		done = 0;		/* done reading the file? */
    CLimageInfo imageinfo;		/* imageinfo */
    int		autoend = 0;		/* exit or wait for user? */


    /* mjfif file */
    int		fd;	   		/* data file descriptor */
    struct dioattr dio_info;		/* directio info */
    mjfif_header_t  * file_header;	/* mjfif header file */
    int		chunkSize = CHUNK_SIZE;	/* initial cut at chunk size */


    while ((c = getopt(argc, argv, "hf:v:E")) != EOF) {
	switch (c) {
	    case 'f':
		infile = optarg;
		break;

	    case 'v':
		vout_node = atoi( optarg );
		break;

	    case 'h':
		usage( progname );
		exit( 0 );
		break;

	    case 'E':
		autoend = 1;
		break;

	    default:
		usage( progname );
		exit( 1 );
		break;
	}
    }

    /*
    ** check for valid parameters
    */
    if (infile == NULL) {
	usage( progname );
	exit(1);
    }

    /*
    ** attempt to get the file opened and setup for directio
    */
    fd = open( infile, O_RDONLY|O_DIRECT, 0666 );
    if (fd < 0) {
	perror( infile );
	exit(1);
    }

    rc = fcntl( fd, F_DIOINFO, &dio_info );
    if (rc) {
	perror( "fcntl( F_DIOINFO )" );
	exit(1);
    }
    if (chunkSize > dio_info.d_maxiosz) chunkSize = dio_info.d_maxiosz;

    /* allocate on restricted boundary ... */
    file_header = (mjfif_header_t *)memalign( 
			dio_info.d_mem, dio_info.d_miniosz );
    bzero( file_header, dio_info.d_miniosz );

    /* and read the file header into memory */
    read( fd, file_header, dio_info.d_miniosz );

    /*
    ** make sure the device directio transfer characteristics
    ** are compatible this those used to create the file.
    */
    if ( (file_header->dio_size < dio_info.d_miniosz) ||
		((dio_info.d_miniosz % file_header->dio_size) != 0) ) {
	fprintf(stderr,"directio characteristics are incompatible\n");
	exit(1);
    }

    /*
    ** the mjfif file format is defined to have the header in the
    ** first block ... so we'll skip one block here and fill it in
    ** later
    */
    lseek( fd, file_header->dio_size, SEEK_SET );

    /*
    ** setup the compression library
    */
    scheme = clQuerySchemeFromName( CL_ALG_VIDEO, "impact" );
    if (scheme < 0) {
	fprintf(stderr,"didn't find impact compression scheme\n");
	exit(1);
    }

    rc = clOpenDecompressor( scheme, &clhandle );
    if (rc != SUCCESS) {
	fprintf(stderr,"failed to open the decompressor\n");
	exit(1);
    }


    /*
    ** at this point we need to query the cl and figure out which
    ** of the codec nodes we're on.  after we setup the video path,
    ** we'll query the size of the video we're about to capture and
    ** come back and finish setting up the cl.
    */
    codec_cookie = clGetParam( clhandle, CL_IMPACT_VIDEO_INPUT_CONTROL );


    /*
    ** setup the video library
    */
    svr = vlOpenVideo( "" );
    src = vlGetNode( svr, VL_SRC, VL_CODEC, codec_cookie );
    drn = vlGetNode( svr, VL_DRN, VL_VIDEO, vout_node );
    if ((src == -1) || (drn == -1)) {
	fprintf(stderr,"getnode failed\n");
	exit(1);
    }

    path = vlCreatePath( svr, VL_ANY, src, drn );
    if (path == -1) {
	vlPerror("vlCreatePath");
	exit(1);
    }

    rc = vlSetupPaths( svr, &path, 1, VL_SHARE, VL_SHARE );
    if (rc) {
	vlPerror("vlSetupPaths");
	exit(1);
    }

    /*
    ** now setup the size of the codec node to meet the size of
    ** the captured data
    */
    val.xyVal.x = file_header->image_width;
    val.xyVal.y = file_header->image_height;
    rc = vlSetControl( svr, path, drn, VL_SIZE, &val );

    /*
    ** and start the flow of data from the vl perspective
    */
    rc = vlBeginTransfer( svr, path, 0, NULL );
    if (rc) {
	vlPerror("vlBeginTransfer");
	exit(1);
    }


    /*
    ** now finish setting up the cl
    */
    n = 0;
    paramBuf[n++] = CL_IMAGE_WIDTH;
    paramBuf[n++] = file_header->image_width;
    paramBuf[n++] = CL_IMAGE_HEIGHT;
    paramBuf[n++] = file_header->image_height;

    rc = clSetParams( clhandle, paramBuf, n );
    if (rc != SUCCESS) {
	fprintf(stderr,"clSetParams failed\n");
	exit(1);
    }

    /* 
    ** make sure that the ring is sized as a multiple of the block size
    ** (rounding up as necessary).  the ring beginning will be alligned
    ** on a 16KB boundary (cuz I made it do that :)
    */
    ringsize = ((CL_DATA_RING + chunkSize - 1) / chunkSize) * chunkSize;
    dataRB = clCreateBuf( clhandle, CL_DATA, 1, ringsize, 0 );
    if (dataRB == NULL) {
	fprintf(stderr,"clCreateBuf failed\n");
	exit(1);
    }

    /*
    ** setup high priority, and locked pages
    */
    if (plock(PROCLOCK) == -1) {
	perror("Warning: failed to lock process in memory");
    }
    if (schedctl(NDPRI, getpid(), NDPHIMIN) == -1) {
	perror("Warning: failed to set non-degrading priority");
    }

    /*
    ** pre-read a large chunk of the data into ring buffer and mark
    ** it as available before calling clDecompress ... that way we
    ** will (hopefully) avoid underflowing the decompressor.  
    ** (and since we don't know the individual field sizes, there's
    ** no way we can determine if we do infact underflow  :(
    */
    fprintf(stderr,"preloading the ring buffer ... ");
    preWrap = clQueryFree( dataRB, 0, (void**)&dataPtr, &ignored );
    while ((nRead < (file_header->data_size - chunkSize)) && 
			(preWrap > chunkSize)) {
	rc = read(fd, dataPtr, chunkSize);
	if (rc != chunkSize) {
	    fprintf(stderr,"read returned %d instead of %d, errno %d\n",
		   rc, chunkSize, errno );
	    exit(1);
	}
	nRead += chunkSize;
        clUpdateHead( dataRB, chunkSize );
        preWrap = clQueryFree( dataRB, 0, (void**)&dataPtr, &ignored );
    }
    fprintf(stderr,"done\n");

    /*
    ** everything is setup, and the video is pumping black out to video.
    ** let's give it something to chew on ...
    */
    rc = clDecompress( clhandle, CL_CONTINUOUS_NONBLOCK,
					0, NULL, CL_EXTERNAL_DEVICE);
    if (rc != SUCCESS) {
	fprintf(stderr,"clDecompress failed\n");
	exit(1);
    }

    /*
    ** and drop into a loop to write the rest of the data into the
    ** ring buffer.  mark the ring done when we put the last of
    ** the data into it.
    */
    done = 0;
    while ( !done ) {
	preWrap = clQueryFree( dataRB, chunkSize, (void**)&dataPtr, &ignored );
	toRead = (preWrap < chunkSize) ? preWrap : chunkSize;
	if ((nRead+toRead) > file_header->data_size) { 
		toRead -= ((nRead+toRead) - file_header->data_size);
		toRead = ((toRead + file_header->dio_size - 1) / 
				file_header->dio_size) * file_header->dio_size;
		done = 1;
	}
	rc = read( fd, dataPtr, toRead );
	if (rc != toRead) {
	    fprintf(stderr,"read attept of %d, returned %d errno %d, "
		"nRead %lld\n", toRead, rc, errno, nRead);
	    exit(1);
	}
	nRead += toRead;
	clUpdateHead( dataRB, toRead );
    }
    clDoneUpdatingHead( dataRB );

    /*
    ** now just hang out and loop while the data in the ring buffer
    ** is played.
    */
    rc = clGetNextImageInfo(clhandle, &imageinfo, sizeof(CLimageInfo));
    while ((rc == SUCCESS ) || (rc == CL_NEXT_NOT_AVAILABLE)) {
	sginap(1);
        rc = clGetNextImageInfo(clhandle, &imageinfo, sizeof(CLimageInfo));
    }

    fprintf(stderr,"%s playback complete.\n",infile);
    if (!autoend) {
    	fprintf(stderr,"Hit enter to exit");
    	getchar();
    }

    /*
    ** cleanup 
    */
    close( fd );
    clCloseDecompressor( clhandle );
}
