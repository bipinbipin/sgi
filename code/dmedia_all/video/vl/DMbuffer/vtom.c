
/*
** vtom.c - video to memory.
*/

#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <bstring.h>
#include <sys/time.h>
#include <limits.h>
#include <dmedia/dmedia.h>
#include <dmedia/vl.h>
#include <dmedia/dm_buffer.h>
#include <dmedia/dm_params.h>
#include <gl/image.h>

int ntoi(char *str, int *val);
void usage(void);
void dump_frame(char * data, int xsize, int ysize, int i);

char *prognm;

int Debug_f = 0;
char * Filenm_f = "vidcap";
int Nframes_f = 1;
int Viddev_f = VL_ANY;
int Vidnode_f = VL_ANY;
int mask = VLTransferCompleteMask | VLTransferFailedMask;

int
main(int ac, char *av[])
{
    int	c, errflg = 0;
    VLServer svr;
    VLNode drn;
    VLNode src;
    VLPath path;
    VLControlValue val;
    DMparams * plist;
    VLTransferDescriptor xferdesc;
    VLEvent event;
    int xsize, ysize;
    int xfersize;
    DMbufferpool pool;
    DMbuffer *dmbuffers;
    fd_set fdset;
    int	maxfd;
    int pathfd;
    int i;
    int nready;

    prognm = av[0];

    while( (c = getopt(ac,av,"Df:c:v:n:")) != -1 ) {
	switch( c ) {
	case 'D':
	    Debug_f++;
	    break;
	case 'f':
	    Filenm_f = optarg;
	    break;
	case 'c':
	    if( ntoi(optarg, &Nframes_f) )
		errflg++;
	    break;
	case 'v':
	    if( ntoi(optarg, &Viddev_f) )
		errflg++;
	    break;
	case 'n':
	    if( ntoi(optarg, &Vidnode_f) )
		errflg++;
	    break;
	case '?':
	    errflg++;
	    break;
	}
    }

    if( errflg ) {
	usage();
	return 1;
    }

    if( Debug_f ) {
	printf("%s: dev %d:%d, file %s, %d frames\n", prognm, 
	    Viddev_f, Vidnode_f, Filenm_f, Nframes_f);
    }

    if( (svr = vlOpenVideo("")) == NULL ) {
	fprintf(stderr,"%s: vlOpenVideo failed\n",prognm);
	return 1;
    }

    if( (drn = vlGetNode(svr, VL_DRN, VL_MEM, VL_ANY)) == -1 ) {
	vlPerror("get mem drain node");
	return 1;
    }

    if( (src = vlGetNode(svr, VL_SRC, VL_VIDEO, Vidnode_f)) == -1 ) {
	vlPerror("get video source node");
	return 1;
    }

    if( (path = vlCreatePath(svr, Viddev_f, src, drn)) == -1 ) {
	vlPerror("get path");
	return 1;
    }

    if( vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE) == -1 ) {
	vlPerror("setup paths");
	return 1;
    }

    /* Set packing mode to RGB */
    val.intVal = VL_PACKING_RGB_8;
    vlSetControl(svr, path, drn, VL_PACKING, &val);

    val.intVal = VL_FORMAT_RGB;
    vlSetControl(svr, path, drn, VL_FORMAT, &val);

    vlGetControl(svr, path, drn, VL_SIZE, &val);
    xsize = val.xyVal.x;
    ysize = val.xyVal.y;

    if( dmParamsCreate(&plist) == DM_FAILURE ) {
	fprintf(stderr, "%s: create params\n");
	return 1;
    }

    xfersize = vlGetTransferSize(svr, path);

    if( dmBufferSetPoolDefaults(plist, Nframes_f, xfersize, DM_TRUE, DM_TRUE)
	== DM_FAILURE ) {
	fprintf(stderr, "%s: set pool defs\n",prognm);
	return 1;
    }

    if( vlDMPoolGetParams(svr, path, drn, plist) == DM_FAILURE ) {
	fprintf(stderr, "%s: get pool reqs\n",prognm);
	return 1;
    }

    /* create the buffer pool */
    if( dmBufferCreatePool(plist, &pool) == DM_FAILURE ) {
	fprintf(stderr, "%s: create pool\n", prognm);
	return 1;
    }

    if( vlDMPoolRegister(svr, path, drn, pool) == DM_FAILURE ) {
	fprintf(stderr, "%s: register pool\n", prognm);
	return 1;
    }

    if( (dmbuffers = (DMbuffer *)calloc(Nframes_f,sizeof(DMbuffer *))) 
	== NULL ) {
	fprintf(stderr, "%s: calloc\n");
	return 1;
    }

    xferdesc.mode = VL_TRANSFER_MODE_DISCRETE;
    xferdesc.count = Nframes_f;
    xferdesc.delay = 0;
    xferdesc.trigger = VLTriggerImmediate;

    if( vlSelectEvents(svr, path, mask) == -1 ) {
	vlPerror("select events");
	return 1;
    }

    /* Begin the data transfer */
    if( vlBeginTransfer(svr, path, 1, &xferdesc) == -1 ) {
	vlPerror("begin transfer");
	return 1;
    }

    /* spin in the get event loop until we've reached nframes */
    if( vlPathGetFD(svr, path, &pathfd) ) {
	vlPerror("getpathfd");
	return 1;
    }
    maxfd = pathfd;

    for( i = 0 ; i < Nframes_f ;  ) {
	FD_ZERO(&fdset);
	FD_SET(pathfd, &fdset);

	if( (nready = select(maxfd + 1, &fdset, 0, 0, 0)) == -1 ) {
	    perror("select");
	    return 1;
	}

	if( nready == 0 ) {
	    continue;
	}

	while( vlEventRecv(svr, path, &event) == DM_SUCCESS ) {
	    switch( event.reason ) {
	    case VLTransferComplete:
		vlEventToDMBuffer(&event, &dmbuffers[i++]);
		break;
	    case VLTransferFailed:
		fprintf(stderr, "Transfer failed!\n");
		return 1;
	    default:
		if( Debug_f ) {
		    fprintf(stderr, "Received unexpect event %s\n", 
			vlEventToName(event.reason));
		}
		break;
	    }
	}
    }

    (void)vlEndTransfer(svr, path);

    /* dump out the pictures we received */
    for( i = 0 ; i < Nframes_f ; i++ ) {
	dump_frame(dmBufferMapData(dmbuffers[i]), xsize, ysize, i);
	(void)dmBufferFree(dmbuffers[i]);
    }

    (void)vlDMPoolDeregister(svr, path, src, pool);

    (void)vlDestroyPath(svr, path);

    (void)vlCloseVideo(svr);

    return 0;
}

/*
** dump_frame - create an SGI .rgb image file.
*/

void
dump_frame(char * data, int xsize, int ysize, int i)
{
    char fname[PATH_MAX];
    IMAGE *image;
    unsigned short rbuf[1024];
    unsigned short gbuf[1024];
    unsigned short bbuf[1024];
    int x, y;

    sprintf(fname,"%s-%05d.rgb", Filenm_f, i);
    image = iopen(fname, "w", RLE(1), 3, xsize, ysize, 3);
    if( Debug_f ) {
	printf("%s: writing file %s size %d x %d\n",
	    prognm, fname, xsize, ysize);
    }

    for( y = 0 ; y < ysize ; y++ ) {
	for( x = 0 ; x < xsize ; x++ ) {
	    data++;
	    bbuf[x] = *data++;
	    gbuf[x] = *data++;
	    rbuf[x] = *data++;
	}
	(void)putrow(image, rbuf, ysize - y - 1, 0);
	(void)putrow(image, gbuf, ysize - y - 1, 1);
	(void)putrow(image, bbuf, ysize - y - 1, 2);
    }
    (void)iclose(image);
}

void
usage()
{
    fprintf(stderr,"usage: [options]\n",prognm);
    fprintf(stderr,"    -f <filename base>\n");
    fprintf(stderr,"    -c <frame count>\n");
    fprintf(stderr,"    -v <video device>\n");
    fprintf(stderr,"    -n <video node>\n");
}

int
ntoi(char *str, int *val )
{
    char *strp;	
    long ret;

    if( *str == '\'' ) {
	str++;
	return (*str)?*str:-1;
    }

    ret = strtol(str,&strp,0);

    if( ((ret == 0) && (strp == str)) ||
	(((ret == LONG_MIN) || ret == LONG_MAX) && (errno == ERANGE)) )
	return -1;

    if( (ret > INT_MAX) || (ret < INT_MIN) ) {
	return -1;
    }
    
    *val = (int)ret;
    
    return 0;
}
