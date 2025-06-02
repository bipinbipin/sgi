
/*
** mtov.c - memory to video.
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
int load_frame(char * data, int xsize, int ysize, int i);

char *prognm;

int Debug_f = 0;
char * Filenm_f = "vidcap";
int Nframes_f = 1;
int Viddev_f = VL_ANY;
int Vidnode_f = VL_ANY;
int Loop_f = 0;

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
    fd_set fdset;
    int	maxfd;
    int pathfd;
    DMbuffer *buffers;
    int i;
    int nready;

    prognm = av[0];

    while( (c = getopt(ac,av,"Df:c:v:n:l")) != -1 ) {
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
	case 'l':
	    Loop_f++;
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
	printf("%s: dev 0x%x:0x%x, file %s, %d frames\n", prognm, 
	    Viddev_f, Vidnode_f, Filenm_f, Nframes_f);
    }

    if( (svr = vlOpenVideo("")) == NULL ) {
	fprintf(stderr,"%s: vlOpenVideo failed\n",prognm);
	return 1;
    }

    if( (src = vlGetNode(svr, VL_SRC, VL_MEM, VL_ANY)) == -1 ) {
	vlPerror("get mem source node");
	return 1;
    }

    if( (drn = vlGetNode(svr, VL_DRN, VL_VIDEO, Vidnode_f)) == -1 ) {
	vlPerror("get video drain node");
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
    vlSetControl(svr, path, src, VL_PACKING, &val);

    val.intVal = VL_FORMAT_RGB;
    vlSetControl(svr, path, src, VL_FORMAT, &val);

    vlGetControl(svr, path, src, VL_SIZE, &val);
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

    if( vlDMPoolGetParams(svr, path, src, plist) == DM_FAILURE ) {
	fprintf(stderr, "%s: get pool reqs\n",prognm);
	return 1;
    }

    /* create the buffer pool */
    if( dmBufferCreatePool(plist, &pool) == DM_FAILURE ) {
	fprintf(stderr, "%s: create pool\n", prognm);
	return 1;
    }

    if( (buffers = (DMbuffer *)calloc(Nframes_f,sizeof(DMbuffer))) == NULL ) {
	fprintf(stderr, "%s: calloc\n");
	return 1;
    }

    for( i = 0 ; i < Nframes_f ; i++ ) {
	if( dmBufferAllocate(pool, &buffers[i]) != DM_SUCCESS ) {
	    fprintf(stderr, "buffer allocate %d\n",i);
	    return 1;
	}

	if( load_frame(dmBufferMapData(buffers[i]), xsize, ysize, i) ) {
	    fprintf(stderr, "load frame %d\n", i);
	    return 1;
	}

	if( vlDMBufferSend(svr, path, buffers[i]) ) {
	    fprintf(stderr, "buffer send %d\n", i);
	    return 1;
	}
    }

    if( vlSelectEvents(svr, path, VLTransferCompleteMask) == -1 ) {
	vlPerror("select events");
	return 1;
    }

    if( !Loop_f ) {
	xferdesc.mode = VL_TRANSFER_MODE_DISCRETE;
	xferdesc.count = Nframes_f;
	xferdesc.delay = 0;
	xferdesc.trigger = VLTriggerImmediate;
	if( Debug_f ) {
	    printf("Starting discrete transfer for %d frames\n", Nframes_f);
	}

	/* Begin the data transfer */
	if( vlBeginTransfer(svr, path, 1, &xferdesc) == -1 ) {
	    vlPerror("begin transfer");
	    return 1;
	}
    }
    else {
	if( Debug_f ) {
	    printf("Starting continuous transfer for %d frames\n", Nframes_f);
	}
	/* Begin the data transfer */
	if( vlBeginTransfer(svr, path, 0, NULL) == -1 ) {
	    vlPerror("begin transfer");
	    return 1;
	}
    }

    /* spin in the get event loop until we've reached nframes */
    if( vlPathGetFD(svr, path, &pathfd) ) {
	vlPerror("getpathfd");
	return 1;
    }
    maxfd = pathfd;

    for( i = 0 ; i < Nframes_f || Loop_f ;  ) {
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
		if( Loop_f ) {
		    if( vlDMBufferSend(svr, path, buffers[i%Nframes_f]) ) {
			fprintf(stderr, "buffer send %d\n", i);
			return 1;
		    }
		}
		i++;
		break;
	    default:
		if( Debug_f ) {
		    fprintf(stderr, "Received unexpect event %s\n", 
			vlEventToName(event.reason));
		}
		break;
	    }
	}
    }

    /* free up resources. */
    (void)vlEndTransfer(svr, path);
    (void)vlDMPoolDeregister(svr, path, src, pool);

    for( i = 0 ; i < Nframes_f ; i++ ) {
	(void)dmBufferFree(buffers[i]);
    }

    (void)vlDestroyPath(svr, path);

    (void)vlCloseVideo(svr);

    return 0;
}

/*
** dump_frame - save an SGI .rgb image file.
*/

int
load_frame(char * data, int xsize, int ysize, int i)
{
    char fname[PATH_MAX];
    IMAGE *image;
    unsigned short rbuf[1024];
    unsigned short gbuf[1024];
    unsigned short bbuf[1024];
    int x, y;

    sprintf(fname,"%s-%05d.rgb", Filenm_f, i);
    if( (image = iopen(fname, "r", RLE(1), 3, xsize, ysize, 3)) == NULL ) {
	return 1;
    }
    if( Debug_f ) {
	printf("%s: reading file %s size %d x %d\n",
	    prognm, fname, xsize, ysize);
    }


    for( y = 0 ; y < ysize ; y++ ) {
	(void)getrow(image, rbuf, ysize - y - 1, 0);
	(void)getrow(image, gbuf, ysize - y - 1, 1);
	(void)getrow(image, bbuf, ysize - y - 1, 2);
	for( x = 0 ; x < xsize ; x++ ) {
	    *data++ = 0;
	    *data++ = bbuf[x];
	    *data++ = gbuf[x];
	    *data++ = rbuf[x];
	}
    }
    (void)iclose(image);

    return 0;
}

void
usage()
{
    fprintf(stderr,"usage: [options]\n",prognm);
    fprintf(stderr,"    -f <filename base>\n");
    fprintf(stderr,"    -c <frame count>\n");
    fprintf(stderr,"    -v <video device>\n");
    fprintf(stderr,"    -n <video node>\n");
    fprintf(stderr,"    -l loop on output\n");
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
