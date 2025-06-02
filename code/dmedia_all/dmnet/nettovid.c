/*
 * File:         nettovid.c
 *
 * Description:   nettovid receives frames in dmBuffers from dmnet 
 *                and sends these to video out
 *
 * Functions:     SGI Video Library functions used (see other modules)
 *
 *                vlOpenVideo()
 *                vlGetNode()
 *                vlCreatePath()
 *                vlSetupPaths()
 *                vlSetControl()
 *                vlGetControl()
 *                vlDMGetParams()
 *                vlDMPoolRegister()
 *                vlGetTransferSize()
 *                vlSelectEvents()
 *                vlBeginTransfer()
 *                vlEndTransfer()
 *                vlDMPoolDeregister()
 *                vlDestroyPath()
 *                vlCloseVideo()
 *                vlPerror()
 *                vlStrError()
 *
 *
 *                Irix 6.3 specific Video Library functions used
 *
 *                vlDMBufferSend()
 *                vlEventRecv()
 *
 *
 *                Irix 6.4/6.5 specific Video Library functions used
 *
 *                vlDMBufferPutValid()
 *                vlNextEvent()
 *                vlCheckEvent()
 *                vlPending()
 *
 *
 *                SGI DMBuffer Library Functions used
 *
 *                dmParamsCreate()
 *                dmParamsSetType()
 *                dmParamsGetType()
 *                dmParamsDestroy()
 *                dmBufferSetPoolDefaults()
 *		          dmBufferCreatePool()
 *		          dmBufferDestroyPool()
 *		          dmBufferFree()
 *                 
 *                SGI DMNet Library functions used 
 *
 *                dmNetOpen()
 *                dmNetListen()
 *                dmNetAccept()
 *                dmNetGetParams()
 *                dmNetRegisterPool()
 *                dmNetRecv()
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <malloc.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <vl/vl.h>
#include <gl/image.h>

#include <dmedia/dmedia.h>
#include <dmedia/dm_buffer.h>
#include <dmedia/dm_params.h>
#include <sys/dmcommon.h>
#include <dmedia/dmnet.h>

#define DMNET_SELECT

extern int errno;
int DEBUG=0;

char *_progName;
VLServer svr;
VLPath MEMtoVIDPath;
VLNode src, drn;
DMbufferpool pool;
DMnetconnection cnp;
int frame = 0;

void process_event(int block);
void docleanup(int exit_type);
void usage(void);

#define USAGE \
"%s: [-s wxh] [-o <video-node>] [-i] [-v <memory-node>] [-L]\n\
\t\t[-x] [-p] [-n <numframes>] [-I] [-m <format>] [-r <packing>]\n\
\t\t[-b <poolsize>] [-q <port>] [-c <conkntype>] [-N <plugin>] [-h]\n\
\n\
\t-s wxh          frame dimensions\n\
\t-o video-node   Output node number\n\
\t-i              NonInterlaced (fields instead of default frames)\n\
\t-v memory-node  Memory node number\n\
\t-L              Toggle loop mode (default on)\n\
\t-x              Use external trigger\n\
\t-p              Pause after each frame\n\
\t-n numframes    Number of frames to output (default 1000)\n\
\t-I              Print node and path IDs\n\
\t-m              format/colorspace of images\n\
\t-r packing      packing of images\n\
\t-b poolsize     Number of buffers in pool (default 5)\n\
\t-q port         Port number, default 5555\n\
\t-c conntype     DmNet connection type\n\
\t-N plugin       DmNet plugin tag\n\
\t-h              This message\n"

void 
err(char* str) 
{
	fprintf(stderr, "%s: %s", _progName, str);
	exit(1);
}

void
usage(void)
{
    fprintf(stderr, USAGE, _progName);
}

void
main(int argc, char **argv)
{
    VLControlValue val;
    VLTransferDescriptor xferDesc;
    DMparams* plist;
    DMbuffer* bufArray;
	int voutnode = VL_ANY;
	int memnode = VL_ANY;
    int transferSize;
    int trigger = 0;
    int nframes = 1000;
    int print_ids = 0;
    int framepause = 0;
    int c;
    int  packing = VL_PACKING_YVYU_422_8;
	int format = VL_FORMAT_SMPTE_YUV;
	int capType = VL_CAPTURE_INTERLEAVED;
    int port = 5555;
	int poolsize = 5;
	int xsize=0, ysize=0;
	int bFields=0;
	int conntype=DMNET_TCP;
	char *pluginName=NULL;
	int bLoop=1;
    /*
    char * dataPtr;
    FILE* fp;
    char userData[DM_BUFFER_MAX_AUX_DATA_SIZE];
    dmspoolinfo_t* pip; 
    */

    _progName = argv[0];
	DEBUG = ( int ) getenv( "DEBUG_NETTOVID" );

    while ((c = getopt(argc, argv, "s:r:v:m:Ln:po:xIliq:b:c:N:h")) != EOF) {
	switch(c) {
	case 'r':
	    packing = atoi(optarg);
	    break;
	    
	case 'i':
		bFields =1;
		capType = VL_CAPTURE_NONINTERLEAVED;
		break;

	case 'c':
		conntype = atoi(optarg);
		break;

	case 'N':
		pluginName = strdup(optarg);
		if(!pluginName) {
			fprintf(stderr, "Must supply plugin tag with -N\n");
			usage(); exit(1);
		}
		break;

	case 's':
	{
	    int len, xlen, ylen;
	    char *numbuf;
	    char *xloc;
	    
	    len = strlen(optarg);
	    xloc = strchr(optarg, 'x');
	    if (!xloc)
	    {
			fprintf(stderr, "Error: invalid geometry format, using default size\n");
			break;
	    }
	    xlen = len - strlen(xloc);
	    if (xlen < 1 || xlen > 4)
	    {
			fprintf(stderr, "Error: invalid x size, using default size\n");
			break;
	    }
	    numbuf = strtok(optarg,"x");
	    xsize = atoi(numbuf);
	    if ( xsize < 0 ) {
			fprintf (stderr, "Error: negative w size not allowed, using default w and h sizes\n");
			xsize = 0;	/* use default size */
			break;
	    }
	    
	    ylen = len - xlen -1;
	    if (ylen < 1 || ylen > 4)
	    {
			fprintf(stderr, "Error: invalid y size, using default size\n");
			break;
	    }
	    numbuf = strtok(NULL,"");
	    ysize = atoi(numbuf);
	    if ( ysize < 0 ) {
			fprintf (stderr, "Error: negative h size not allowed, using default w and h sizes\n");
			xsize = 0;	/* use default size */
			break;
	    }
	}
	break;	
	
	case 'v':
	    memnode = atoi(optarg);
	    break;
	    
	case 'm':
	    format = atoi(optarg);
	    break;
	    
	case 'n':
	    nframes = atoi(optarg);
	    break;
	    
	case 'L':
		bLoop =~bLoop;
		break;

	case 'p':
	    framepause = 1;
	    break;
	    
	case 'o':
	    voutnode = atoi(optarg);
	    break;
	    
	case 'x':
	    trigger = 1;
	    break;
	    
	case 'I':
	    print_ids = 1;
	    break;
	    
	case 'l' :
		conntype = DMNET_LOCAL;
		break;

	case 'q' :
	    port = atoi(optarg);
	    break;
	    
	case 'b' :
	    poolsize = atoi(optarg);
	    break;
	    
	case 'h':
	    usage();
	    exit(0);
	    break;
	    
	default:
	    usage();
	    exit(1);
	    break;
	}
    }
    
    if (bFields)
      nframes *= 2;

	/* DmNet calls. Very similar to BSD socket calls */
    if (dmNetOpen(&cnp) != DM_SUCCESS)
		err("dmNetOpen failed\n");
    
    if (dmParamsCreate(&plist) == DM_FAILURE)
		err("Create params failed\n");
	
    if (dmParamsSetInt(plist, DMNET_PORT, port) == DM_FAILURE)
		err("Couldn't set port number\n");

	if(pluginName) {
		conntype = DMNET_PLUGIN;
		if (dmParamsSetString(plist, DMNET_PLUGIN_NAME, pluginName)
				== DM_FAILURE)
			err("Couldn't set DMNET_PLUGIN_NAME");
	}
    
	/* DMNET_LOCAL uses a dms fifo and bypasses the network altogether */
    if (dmParamsSetInt(plist, DMNET_CONNECTION_TYPE, conntype)
			== DM_FAILURE) 
		err("Couldn't set connection type\n");

    if (dmNetListen(cnp, plist) != DM_SUCCESS)
		err("dmNetListen failed\n");

    if (dmNetAccept(cnp, plist) != DM_SUCCESS)
		err("dmNetAccept failed\n");
	if(DEBUG) 
    	fprintf(stderr, "%s: accept\n", _progName);
    
	/* VL Calls */

    /* Connect to the daemon */
    if (!(svr = vlOpenVideo(""))) 
		err("Couldn't open video\n");
    
    /* 
     * Create the memory to video path (just grab the first device 
     * that supports it) 
     */
    
    /* Set up a source node in memory */
    src = vlGetNode(svr, VL_SRC, VL_MEM, memnode);
    
    /* Set up a video drain node on the first device that has one */
    drn = vlGetNode(svr, VL_DRN, VL_VIDEO, voutnode);
    
    /* Create a path using the selected devices */
    MEMtoVIDPath = vlCreatePath(svr, VL_ANY, src, drn);
    
    if (MEMtoVIDPath == -1) {
	fprintf(stderr, "%s: can't create path: %s\n", 
		_progName, vlStrError(vlErrno));
	exit(1);
    }
    
    /* Print the node and path IDs for cmd line users */
    if (print_ids) {
		printf("MEMTOVID NODE IDs:\n");
		printf("memory source = %d\n", src);
		printf("video drain = %d\n", drn);
		printf("MEMORY TO VIDEO PATH ID = %d\n", MEMtoVIDPath);
    }
    
    /* Set up the hardware for and define the usage of the path */
    if (vlSetupPaths(svr, (VLPathList)&MEMtoVIDPath, 1, VL_SHARE, VL_SHARE)<0) {
		printf("%s: can't setup path: %s\n", _progName, vlStrError(vlErrno));
		exit(1);
    }
    
	/* Specify packing, format and capture type */
    val.intVal = packing;
    if (vlSetControl(svr, MEMtoVIDPath, src, VL_PACKING, &val) < 0) {
		vlPerror("vlSetControl(VL_PACKING)");
		exit(1);
    }
    
    val.intVal = format;
    if (vlSetControl(svr, MEMtoVIDPath, src, VL_FORMAT, &val) < 0) { 
		vlPerror("vlSetControl(VL_FORMAT)");
		exit(1);
    }

	val.intVal = capType;
	if (vlSetControl(svr, MEMtoVIDPath, src, VL_CAP_TYPE, &val) <0) {
		vlPerror("vlSetControl(VL_CAP_TYPE)");
		exit(1);
	}
    
    /* Get the size of the video frame */
    if (!xsize)
		vlGetControl(svr, MEMtoVIDPath, drn, VL_SIZE, &val);
    else {
		val.xyVal.x = xsize;
		val.xyVal.y = ysize;
		vlSetControl(svr, MEMtoVIDPath, drn, VL_SIZE, &val);
    }
    
    xsize = val.xyVal.x;
    ysize = val.xyVal.y;

    /* Make sure the path supports this size */
    transferSize = vlGetTransferSize(svr,MEMtoVIDPath);

    /* take in account if the data is sent as frames or fields */
    if (bFields)
      ysize /= 2;

    fprintf(stderr,"%s: transferSize=%d, xsize=%d, ysize=%d, format=%d, packing=%d\n", 
	    _progName, transferSize, xsize, ysize, format, packing);
    
	/* Allocate a pool of DMBuffers, with poolsize # of buffers */
    if(dmBufferSetPoolDefaults(plist, poolsize, transferSize,
			    DM_TRUE, DM_FALSE) == DM_FAILURE)
		err("dmBufferSetPoolDefaults\n");
    
	/* Give DmNet a chance to add its own parameters */
    if(dmNetGetParams(cnp, plist) == DM_FAILURE)
		err("dmNetGetParams\n");
    
	/* Give VL a chance to add its own parameters */
	/* vlDMPoolGetParams for 6.3 O2 */
    if (vlDMGetParams(svr, MEMtoVIDPath, src, plist) == DM_FAILURE) {
		vlPerror("vlDMGetParams");
		exit(1);
	}
    
	if (DEBUG) 
    	fprintf(stderr, 
		"%s: Pool blk size (dmNetGetParams->vlDMGetParams) = %d\n", 
	    _progName, dmParamsGetInt(plist, DM_BUFFER_SIZE));
    
    if (dmBufferCreatePool(plist, &pool) != DM_SUCCESS)
		err("Create Pool failed\n");

    if(!pool)
      err("No pool\n");

	/* Don't need to call vlDMPoolRegister */
    if (vlDMPoolRegister(svr, MEMtoVIDPath, src, pool)) {
		vlPerror("vlDMPoolRegister");
		dmBufferDestroyPool(pool);
		exit(1);
    }

    if (dmNetRegisterPool(cnp, pool) != DM_SUCCESS)
		err("dmNetRegisterPool failed\n");
    
    /*
    pip = (dmspoolinfo_t*)_dmBufferGetDmsPool(pool);
    fprintf(stderr, "Pool block size = %d\n", pip->block_size);
    */

    /* fprintf(stderr, "%s: Create Pool, Register Pool worked\n", _progName); */
    
    dmParamsDestroy(plist);
    
    bufArray = (DMbuffer*)malloc(poolsize * sizeof(DMbuffer));
    
    /* Events to look for */
    vlSelectEvents(svr, MEMtoVIDPath,  (VLTransferCompleteMask |
					VLSequenceLostMask|
					VLStreamPreemptedMask	|
					VLStreamStartedMask	|
					VLStreamStoppedMask	|
					VLTransferFailedMask));
    /*
     * Fill the transfer descriptor for a discrete transfer of one frame.
     * If the external trigger option is set, we use VLDeviceEvent for a
     * trigger, otherwise trigger immediately. Note that this may not
     * work well on all devices, as VLDeviceEvent it not well-defined.
     */
    xferDesc.delay = 0;
    if (trigger)
		xferDesc.trigger = VLDeviceEvent;
    else
		xferDesc.trigger = VLTriggerImmediate;
    
    xferDesc.count = 0;
    xferDesc.mode = VL_TRANSFER_MODE_CONTINUOUS;
    
    if (vlBeginTransfer(svr, MEMtoVIDPath, 1, &xferDesc) < 0) {
		vlPerror("vlBeginTransfer");
		goto outahere;
    }
    
    for (frame = 0; bLoop || frame < nframes; frame++) {
		/* Illustrate use of dmNetDataFd - to be used
		 * if we don't want to block on dmnetRecv(). 
		 */
		fd_set fdset;
		int nfd;
		int ret=0;
		struct timeval tv;
		nfd = dmNetDataFd(cnp);
#ifdef DMNET_SELECT
		while (!ret) { /* timeout returns 0 */
			FD_ZERO(&fdset);
			FD_SET(nfd, &fdset);
			tv.tv_sec = 1; tv.tv_usec = 0;
			ret = select(nfd+1, &fdset, 0, 0, &tv);
			if (ret == -1) perror("network select");
			else if (ret ==0) {
				fprintf(stderr, "%s: select on dmNetRecv timed out on frame %d\n", 
					_progName, frame);
				/* if (errno == EBUSY) fprintf(stderr, "errno == EBUSY\n"); */
			}
		}
#endif
		while (dmNetRecv(cnp, &bufArray[frame%poolsize]) != DM_SUCCESS) {
	    	if (errno == EBUSY){
				/* FIFO is busy - block until it's ready */
				FD_ZERO(&fdset);
				FD_SET(nfd, &fdset);
				if (select(nfd+1, &fdset, 0, 0, 0) == -1)
					perror("fifo select");
			} else if (errno == ENOMEM) { /* all buffers are full - wait */
				process_event(TRUE);
	    	} else {
				perror(_progName);
				fprintf(stderr, "%s: Could not receive buffer %d\n", 
					_progName, frame+1);
				docleanup(1);
	    	}
			errno = 0;
		}
		if (DEBUG)
			fprintf(stderr, "%s: Received frame %d\n", _progName, frame+1);
		/* Ignore - for testing only
	  	dataPtr = dmBufferMapData(bufArray[frame]);
	  	shortPtr = (unsigned short *) dataPtr;
	  	for (i=0; i<transferSize/2; i++) {
	      	fprintf(stdout, "%x", *shortPtr++);
	  	}
	  	fp = fopen("output.rgb", "w");
	  	fwrite(dataPtr, transferSize, 1, fp);
	  	fclose(fp);
		process_event(FALSE); 
		*/
		
#ifdef DMEDIA_6_3
		vlDMBufferSend(svr, MEMtoVIDPath, bufArray[frame%poolsize]);
#else
		if(vlDMBufferPutValid(svr, MEMtoVIDPath, src, bufArray[frame%poolsize])
			!= VLSuccess) {
			if(vlGetErrno() == VLAgain) {
				process_event(TRUE);
			} else {
				vlPerror("vlDMBufferPutValid");
				docleanup(1);
			}
		}
#endif
		if (framepause) {
	    	printf("[%d] Hit return for next frame\n.", frame);
	    	c = getchar();
		}
		dmBufferFree(bufArray[frame%poolsize]);
    } /* for loop */
   	 
    	/* Wait for a return if -P option used,
     	* otherwise wait until a StreamStopped event;
     	*/
    
outahere:
    	/* Cleanup and exit with no error */
    	docleanup(0);
}

void
process_event(int block)
{
    int ret;
    VLEvent ev;
    
#ifdef DMEDIA_6_3
	ret = vlEventRecv(svr, MEMtoVIDPath, &ev);
#else
	if (block)
		ret = vlNextEvent(svr, &ev);
	else
		ret = vlCheckEvent(svr, VLAllEventsMask, &ev);
#endif

	if (ret == -1)
		return;

	switch (ev.reason) {
	case VLStreamPreempted :
		fprintf(stderr, "%s: Stream Preempted by another Program\n",
 		_progName);
		exit(1);
		break;
      
      /*
       * As of 8/31/94 ev1 still does not support the
       * VLStreamStopped and VLStreamStarted events.
       */
	case VLStreamStopped :
		fprintf(stderr, "%s: Stream stopped at frame %d\n", _progName, frame);
		break;
	case VLSequenceLost :
		fprintf(stderr, "%s: Dropped  frame %d\n", _progName, frame);
		break;
	case VLTransferFailed : 
		fprintf(stderr, "%s: Transfer failed\n", _progName);
		exit(1);
		break;
	case VLTransferComplete :
		if (DEBUG)
			fprintf(stderr, "%s: Frame %d transferred\n", _progName, frame-1);
		break;	
  
	default:
		break;
    }
#ifdef DMEDIA_6_3
	if (vlPending(svr)) process_event(FALSE);
#endif
}

void docleanup(int exit_type)
{
	dmNetClose(cnp);
    vlEndTransfer(svr, MEMtoVIDPath);
    vlDMPoolDeregister(svr, MEMtoVIDPath, src, pool);
    dmBufferDestroyPool(pool);
    /* Destroy the path, free its memory */
    vlDestroyPath(svr, MEMtoVIDPath);
    /* Disconnect from the daemon */
    vlCloseVideo(svr);
    exit(exit_type);
}

