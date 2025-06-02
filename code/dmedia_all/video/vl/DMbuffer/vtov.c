
/*
** vtov.c - video to video.
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

#define MAX(a,b)	(((a)>(b))?(a):(b))

char *prognm;

int Debug_f = 0;
int Nframes_f = 1;
int Viddev_f = VL_ANY;
int Vidnode_f = VL_ANY;

int
main(int ac, char *av[])
{
    int	c, errflg = 0;
    VLServer svr;
    VLNode viddrn;
    VLNode vidsrc;
    VLNode memdrn;
    VLNode memsrc;
    VLPath pathin;
    VLPath pathout;
    VLControlValue val;
    DMparams * plist;
    VLEvent event;
    int xsize, ysize;
    int xfersize;
    DMbufferpool pool;
    fd_set fdset;
    int	maxfd;
    int pathinfd;
    int i;
    int nready;
    DMbuffer buffer;

    prognm = av[0];

    while( (c = getopt(ac,av,"Dc:v:n:")) != -1 ) {
	switch( c ) {
	case 'D':
	    Debug_f++;
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
	printf("%s: dev %d:%d, %d frames\n", prognm, 
	    Viddev_f, Vidnode_f, Nframes_f);
    }

    if( (svr = vlOpenVideo("")) == NULL ) {
	fprintf(stderr,"%s: vlOpenVideo failed\n",prognm);
	return 1;
    }

    /* set up the path in */
    if( (memdrn = vlGetNode(svr, VL_DRN, VL_MEM, VL_ANY)) == -1 ) {
	vlPerror("get mem drain node");
	return 1;
    }

    if( (vidsrc = vlGetNode(svr, VL_SRC, VL_VIDEO, Vidnode_f)) == -1 ) {
	vlPerror("get video source node");
	return 1;
    }

    if( (pathin = vlCreatePath(svr, Viddev_f, vidsrc, memdrn)) == -1 ) {
	vlPerror("create path in");
	return 1;
    }

    /* set up the path out */
    if( (memsrc = vlGetNode(svr, VL_SRC, VL_MEM, VL_ANY)) == -1 ) {
	vlPerror("get mem source node");
	return 1;
    }

    if( (viddrn = vlGetNode(svr, VL_DRN, VL_VIDEO, VL_ANY)) == -1 ) {
	vlPerror("get video drain node");
	return 1;
    }

    if( (pathout = vlCreatePath(svr, VL_ANY, memsrc, viddrn)) == -1 ) {
	vlPerror("create path out");
	return 1;
    }

    if( vlSetupPaths(svr, (VLPathList)&pathin, 1, VL_SHARE, VL_SHARE) == -1 ) {
	vlPerror("setup path in");
	return 1;
    }

    if( vlSetupPaths(svr, (VLPathList)&pathout, 1, VL_SHARE, VL_SHARE) == -1 ) {
	vlPerror("setup path out");
	return 1;
    }

    /* Set the output drain's timing based upon the input source's timing */
    if( vlGetControl(svr, pathin, vidsrc, VL_TIMING, &val) == 0 ) {
	if ((vlSetControl(svr, pathout, viddrn, VL_TIMING, &val) < 0)
	    && (vlErrno != VLBadControl)) {
	    vlPerror("set control timing vid drain");
	    return 1;
	}
    }

    /* Set packing mode to RGB */
    val.intVal = VL_PACKING_RGB_8;
    vlSetControl(svr, pathin, memdrn, VL_PACKING, &val);
    vlSetControl(svr, pathout, memsrc, VL_PACKING, &val);

    val.intVal = VL_FORMAT_RGB;
    vlSetControl(svr, pathin, memdrn, VL_FORMAT, &val);
    vlSetControl(svr, pathout, memsrc, VL_FORMAT, &val);

    vlGetControl(svr, pathin, memdrn, VL_SIZE, &val);
    xsize = val.xyVal.x;
    ysize = val.xyVal.y;

    if( dmParamsCreate(&plist) == DM_FAILURE ) {
	fprintf(stderr, "%s: create params\n");
	return 1;
    }

    xfersize = vlGetTransferSize(svr, pathin);

    if( dmBufferSetPoolDefaults(plist, Nframes_f, xfersize, DM_FALSE, DM_FALSE)
	== DM_FAILURE ) {
	fprintf(stderr, "%s: set pool defs\n",prognm);
	return 1;
    }

    if( vlDMPoolGetParams(svr, pathin, memsrc, plist) == DM_FAILURE ) {
	fprintf(stderr, "%s: get pool reqs\n",prognm);
	return 1;
    }

    /* create the buffer pool */
    if( dmBufferCreatePool(plist, &pool) == DM_FAILURE ) {
	fprintf(stderr, "%s: create pool\n", prognm);
	return 1;
    }

    if( vlDMPoolRegister(svr, pathin, memdrn, pool) == DM_FAILURE ) {
	fprintf(stderr, "%s: register pool\n", prognm);
	return 1;
    }

    if( vlSelectEvents(svr, pathin, VLTransferCompleteMask) == -1 ) {
	vlPerror("select events");
	return 1;
    }

    /* Begin the data transfer */
    if( vlBeginTransfer(svr, pathout, 0, NULL) == -1 ) {
	vlPerror("begin transfer out");
	return 1;
    }

    if( vlBeginTransfer(svr, pathin, 0, NULL) == -1 ) {
	vlPerror("begin transfer in");
	return 1;
    }

    /* spin in the get event loop until we've reached nframes */
    if( vlPathGetFD(svr, pathin, &pathinfd) ) {
	vlPerror("getpathfd");
	return 1;
    }
    maxfd = pathinfd + 1;

    for( ; ; ) {
	FD_ZERO(&fdset);
	FD_SET(pathinfd, &fdset);

	if( (nready = select(maxfd, &fdset, 0, 0, 0)) == -1 ) {
	    perror("select");
	    return 1;
	}

	/* for now, we really only care about path in events... */
	while( vlEventRecv(svr, pathin, &event) == DM_SUCCESS ) {
	    switch( event.reason ) {
	    case VLTransferComplete:
		if( vlEventToDMBuffer(&event, &buffer) ) {
		    vlPerror("event to dmbuffer");
		    return 1;
		}
		if( vlDMBufferSend(svr, pathout, buffer) ) {
		    vlPerror("buffer send");
		    return 1;
		}
		(void)dmBufferFree(buffer);
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

    (void)vlCloseVideo(svr);

    return 0;
}

void
usage()
{
    fprintf(stderr,"usage: [options]\n",prognm);
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
