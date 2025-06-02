/*
** comptodsk - compressed data to disk, directio version
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

#include <dmedia/vl.h>
#include <vl/dev_mgv.h>

#include <dmedia/cl.h>
#include <dmedia/cl_impactcomp.h>

#include "mjfif.h"

#define CL_DATA_RING	    (20 * 1024 * 1024)	    /* 20 Megabytes */
#define	CHUNK_SIZE	    (128 * 1024)	    /* stripe unit */

int    imageCount = MAXINT;	/* number of images to capture */

/*
** usage message
*/
void
usage( char * progname )
{
    fprintf(stderr, "%s: [-c n_images] [-v vin_node] "
			"[-q quality] [-b bitrate] -f filename\n"
		    	"\t-f\toutput file name\n"
		    	"\t-c\tnumber of fields to capture\n"
		    	"\t-v\tvideo source node number\n"
		    	"\t-q\tjpeg quality factor\n"
		    	"\t-b\tbitrate, bits per second (overrides quality)\n",
			progname );
}

/*
** signal handler
*/
void
sighand( void )
{
    imageCount = 0;
}

/*
** application guts
*/
main( int argc, char * argv[] )
{
    char  * progname = argv[0]; 
    int	    c;		
    int	    vin_node = VL_ANY;		/* video input node */
    char  * outfile = NULL;		/* output file name */
    int	    rc;				/* return code */
    int	    ncaptured;			/* fields actually captured */
    struct  sigaction sigact;		/* sigaction */

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
    int		quality = 75;		/* default to quality 75 */
    int		bitrate = 0;		/* bits per second option */
    int		codec_cookie;		/* magic cl->vl codec cookie */
    int		ringsize;		/* size of the compressed ring */
    CLimageInfo imageinfo;		/* image info */
    int		preWrap;		/* unwrapped portion of ring buffer */
    int		ignored;		/* wrapped portion of ring */
    char      * dataPtr;		/* available valid data */
    char      * tmpPtr;			/* temp valid data */
    int		toWrite;		/* how many bytes to write */
    int		prev_imagecount = 0;	/* previous imagecount */
    long long	accum_size = 0;		/* bytes of compressed data */
    float	comp_ratio;		/* informational compression ratio */
    int		ndropped = 0;		/* number of fields dropped */
    int		nap = 0;
    int		bpfield = 0;		/* bytes per field */

    /* mjfif file */
    int		fd;	   		/* data file descriptor */
    struct dioattr dio_info;		/* directio info */
    mjfif_header_t  * file_header;	/* mjfif header file */
    int		remainder;		/* leftover data in ring buffer */
    int		chunkSize = CHUNK_SIZE;	/* initial cut at chunk size */

    while ((c = getopt(argc, argv, "hf:c:v:b:q:")) != EOF) {
	switch (c) {
	    case 'f':
		outfile = optarg;
		break;

	    case 'c':
		imageCount = atoi( optarg );
		break;

	    case 'v':
		vin_node = atoi( optarg );
		break;

	    case 'b':
		bitrate = atoi( optarg );
		break;

	    case 'q':
		quality = atoi( optarg );
		break;

	    case 'h':
		usage( progname );
		exit( 0 );
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
    if (outfile == NULL) {
	usage( progname );
	exit(1);
    }

    /*
    ** attempt to get the file opened and setup for directio
    */
    fd = open( outfile, O_WRONLY|O_CREAT|O_TRUNC|O_DIRECT, 0666 );
    if (fd < 0) {
	perror( outfile );
	exit(1);
    }

    rc = fcntl( fd, F_DIOINFO, &dio_info );
    if (rc) {
	perror( "fcntl( F_DIOINFO )" );
	exit(1);
    }

    /* allocate on restricted boundary ... */
    file_header = (mjfif_header_t *)memalign( 
			dio_info.d_mem, dio_info.d_miniosz );
    bzero( file_header, dio_info.d_miniosz );

    file_header->dio_size = dio_info.d_miniosz;

    if (chunkSize > dio_info.d_maxiosz) chunkSize = dio_info.d_maxiosz;

    /*
    ** the mjfif file format is defined to have the header in the
    ** first block ... so we'll skip one block here and fill it in
    ** later
    */
    lseek( fd, file_header->dio_size, SEEK_SET );

    /*
    ** first step is to setup the compression library
    */
    scheme = clQuerySchemeFromName( CL_ALG_VIDEO, "impact" );
    if (scheme < 0) {
	fprintf(stderr,"didn't find impact compression scheme\n");
	exit(1);
    }

    rc = clOpenCompressor( scheme, &clhandle );
    if (rc != SUCCESS) {
	fprintf(stderr,"failed to open the compressor\n");
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
    src = vlGetNode( svr, VL_SRC, VL_VIDEO, vin_node );
    drn = vlGetNode( svr, VL_DRN, VL_CODEC, codec_cookie );
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
    ** now setup the size of the codec node to meet the restrictions
    ** imposed by the jpeg compression algorithm
    */
    rc = vlGetControl( svr, path, drn, VL_SIZE, &val );
    if (rc) {
	vlPerror("vlGetControl VL_SIZE on CODEC");
	exit(1);
    }

    /*
    ** the compressor works on fields, and the codec drain node gives
    ** us field sizes.  happy happy, joy joy
    */
    file_header->image_width = (val.xyVal.x / 16) * 16;
    file_header->image_height = ((val.xyVal.y + 7) / 8) * 8;

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
    paramBuf[n++] = CL_ENABLE_IMAGEINFO;
    paramBuf[n++] = 1;
    paramBuf[n++] = CL_JPEG_QUALITY_FACTOR;
    paramBuf[n++] = quality;
    if (bitrate != 0) {
	paramBuf[n++] = CL_BITRATE;
	paramBuf[n++] = bitrate;
    }

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
    ** install a signal handler, so that if a count isn't provided
    ** on the command line, that there's a way to get a valid output file
    */
    sigemptyset( &sigact.sa_mask );
    sigact.sa_handler = sighand;
    sigact.sa_flags = 0;
    if (sigaction(SIGINT, &sigact, NULL) != 0) {
	perror( progname );
	fprintf(stderr,"unable to install signal handler\n");
	exit(1);
    }


    /*
    ** everything is setup, and data is landing at the feet of the codec.
    ** turn the codec on, and then start dumping data to disk.
    */
    rc = clCompress( clhandle, CL_CONTINUOUS_NONBLOCK,
					CL_EXTERNAL_DEVICE, 0, NULL);
    if (rc != SUCCESS) {
	fprintf(stderr,"clCompress failed\n");
	exit(1);
    }

    /*
    **
    */
    ncaptured = 0;
    remainder = 0;

    while ( ncaptured < imageCount ) {
	rc = clGetNextImageInfo( clhandle, &imageinfo, sizeof(CLimageInfo) );
	if (rc == SUCCESS) {
	    nap = 0;
	    ncaptured++;
	    remainder += imageinfo.size;
	    accum_size += imageinfo.size;

	    if (imageinfo.imagecount != (prev_imagecount + 1)) {
		fprintf(stderr,"dropped field[s] %d - %d\n",prev_imagecount+1,
		    imageinfo.imagecount-1);
		ndropped += (imageinfo.imagecount - prev_imagecount) - 1;
	    }
	    prev_imagecount = imageinfo.imagecount;
	}
	else if (rc == CL_NEXT_NOT_AVAILABLE) {
	    if (nap) sginap(1); else sginap(0);
	    nap = 1;
	}

	/*
	** and if there's more than a blocksize, push the data out to disk
	*/
	if (remainder >= chunkSize) {
	    nap = 0;
	    preWrap = clQueryValid( dataRB, 0, (void**)&dataPtr, &ignored );
	    toWrite = (preWrap / file_header->dio_size) *
						 file_header->dio_size;
	    if (toWrite > dio_info.d_maxiosz) toWrite = dio_info.d_maxiosz;
	    rc = write( fd, dataPtr, toWrite );
	    if (rc != toWrite) {
		perror( progname );
		fprintf(stderr, "write to file %s failed, errno %d\n",
			outfile, errno);
		exit(1);
	    }
	    remainder -= toWrite;
	    clUpdateTail( dataRB, toWrite );
	}
    }

    /*
    ** we've exited the main loop.  we've captured enough images, now
    ** we just need to make sure they all get to disk
    */
    while (remainder >= file_header->dio_size) {
    	preWrap = clQueryValid( dataRB, 0, (void **)&dataPtr, &ignored);
	if (preWrap > remainder) preWrap = remainder;
	toWrite = (preWrap / file_header->dio_size) *
						 file_header->dio_size;
	if (toWrite > dio_info.d_maxiosz) toWrite = dio_info.d_maxiosz;
	rc = write( fd, dataPtr, toWrite );
	if (rc != toWrite) {
	    perror( progname );
	    fprintf(stderr, "write to file %s failed, errno %d\n",
		outfile, errno);
	    exit(1);
	}
	remainder -= toWrite;
	clUpdateTail( dataRB, toWrite );
    }

    /*
    ** and for the last chunk, allocate outselves a dio_size chunk,
    ** clear it, copy the last chunk of data into it, and then write
    ** it out to the file
    */
    if (remainder > 0) {
    	preWrap = clQueryValid( dataRB, 0, (void **)&dataPtr, &ignored);
	tmpPtr = (char *) memalign( dio_info.d_mem, file_header->dio_size );
	bzero(tmpPtr, file_header->dio_size);
	bcopy(dataPtr, tmpPtr, remainder);
	rc = write( fd, tmpPtr, file_header->dio_size );
	if (rc != file_header->dio_size) {
	    perror( progname );
	    fprintf(stderr, "write to file %s failed, errno %d\n",
		outfile, errno);
	    exit(1);
	}
    }

    /*
    ** and last, but not least, write out the file header 
    */
    strncpy(file_header->magic, "mjfif-01", 8);
    file_header->field_rate = 60;
    file_header->n_images = ncaptured;
    file_header->data_size = accum_size;

    lseek( fd, 0, SEEK_SET );
    rc = write( fd, file_header, file_header->dio_size );
    if (rc != file_header->dio_size) {
	perror( progname );
	fprintf(stderr, "write to file %s failed, errno %d\n", outfile, errno);
	exit(1);
    }

    close( fd );
    clCloseDecompressor( clhandle );

    bpfield = accum_size / ncaptured;
    comp_ratio = (float)(720 * 248 * 2) / (float)(bpfield);

    printf("captured %d fields, dropped %d\n"
	"\tavg size %d bytes per field\n\tbitrate %d bits/sec\n"
	"\tcompression ratio %.1f:1\n",
	ncaptured, ndropped,
	bpfield,
	(bpfield * 8 * 60), comp_ratio);
}
