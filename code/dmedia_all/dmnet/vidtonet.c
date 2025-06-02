/*
 * File:          vidtonet.c 
 *
 * Description:   vidtonet captures a video stream using vl into
 *                dmbuffers, and sends these over dmnet
 *
 * Functions:     SGI Video Library functions used (see other modules)
 *
 *                vlOpenVideo()
 *                vlGetDeviceList()
 *                vlGetNode  ()
 *                vlCreatePath()
 *                vlGetDevice()
 *                vlSetupPaths()
 *                vlSelectEvents()
 *                vlSetControl()
 *                vlGetControl()
 *                vlCreateBuffer()
 *                vlDMPoolRegister()
 *	              vlDMGetParams();
 *                vlGetTransferSize()
 *                vlBeginTransfer()
 *                vlEndTransfer()
 *                vlDeregisterBuffer()
 *                vlDestroyPath()
 *	              vlDestroyBuffer()
 *                vlCloseVideo()
 *                vlPerror()
 *                vlStrError()
 *
 *                Irix 6.3 specific Video Library Functions used
 *
 *                vlPathGetFD()
 *                vlEventRecv()
 *                vlEventToDMBuffer()
 *
 *                Irix 6.4/6.5 specific Video Library Functions used
 *
 *                vlNodeGetFd()
 *                vlNextEvent()
 *                vlDMBufferGetValid()
 *
 *
 *                SGI DMBuffer Library Functions used
 *
 *                dmParamsCreate()
 *                dmParamsSetType()
 *	              dmParamsDestroy()
 *	              dmBufferCreatePool()
 *	              dmBufferSetPoolDefaults()
 *	              dmBufferFree()
 *
 *
 *                SGI dmNet Library Functions used
 * 
 *                dmNetOpen()                
 *                dmNetConnect()
 *                dmNetGetParams()
 *                dmNetRegisterPool()
 *                dmNetSend()
 */

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gl/image.h>
#include <dmedia/vl.h>

#include <dmedia/dmedia.h>
#include <dmedia/dm_buffer.h>
#include <dmedia/dm_params.h>
#include <sys/dmcommon.h>
#include <dmedia/dmnet.h>

char *_progName;
int DEBUG=0;

VLServer svr;
VLPath path;
VLNode src, drn;
DMbufferpool pool;
DMnetconnection cnp;
int xsize = 0, ysize =0;
int sentFrames=0;
struct timeval startTime;
int blockSize=0;

void docleanup(int);
void ProcessEvent(VLServer, VLEvent *, void *);
void usage(void);
void fixEndian(DMbuffer);
void printStats(void);

#define USAGE \
"%s: [-s wxh] [-d <device>] [-v <video-node> ] [-o <memory-node> ]\n\
\t\t[-i] [-t] [-n <num>] [-I] [-m <format>] [-r <packing>]\n\
\t\t[-z n/d] [-b <poolsize>] [-q <port>] [-c <conntype>]\n\
\t\t[-N <plugin>] [-l] [-L] [-W] [-h] -T <hostname>\n\
\n\
\t-s wxh          frame dimensions\n\
\t-d device       device number to use\n\
\t-v video-node   video source node number\n\
\t-o memory-node  memory drain node number\n\
\t-i              nonInterlaced (fields instead of default frames)\n\
\t-t              wait for external trigger\n\
\t-n num          number of frames to capture (default 1000)\n\
\t-I              print node and path IDs\n\
\t-m format       VL format images are in\n\
\t-r packing      packing of images\n\
\t-z n/d          zoom the image by n/d\n\
\t-b poolsize     number of buffers in pool (default 5)\n\
\t-q port         port number\n\
\t-c conntype     DmNet connection type\n\
\t-N plugin       DmNet plugin tag\n\
\t-l              local video server (uses DMNET_LOCAL)\n\
\t-L              toggle loop mode\n\
\t-W              transfer to WinNT\n\
\t-h              this help message\n\
\t-T hostname     video server host\n"

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
	char* hostname=NULL;
	int nframes = 1000, frame =0;
	VLEvent event;
    VLDevList devlist;
    VLDev     deviceId;
    VLControlValue val;
    VLTransferDescriptor xferDesc;
	int frameSize;
    DMparams* plist;
	int poolsize = 5;
    int c;
    int vin = VL_ANY;
    int memnode = VL_ANY;
    int devicenum = -1;
	char *deviceName;
    int zoom_num = 1;
    int zoom_denom = 1;
    int packing = VL_PACKING_YVYU_422_8;
	int format = VL_FORMAT_SMPTE_YUV;
	int capType = VL_CAPTURE_NONINTERLEAVED;
    int ext_trig = 0;
    int print_ids = 0;
    int port = 5555;
	int pathfd;
	fd_set fdset;
	int nready;
	int bFields=0;
	int conntype=DMNET_TCP;
	char* pluginName=NULL;
	int bNt=0, bLoop=1;

    _progName = argv[0];
	DEBUG = ( int ) getenv( "DEBUG_NETTOVID" );

    while ((c = getopt(argc, argv, "o:n:b:s:d:r:m:v:tIz:liT:q:hc:N:WL")) != EOF)
    {
	switch (c)
	{
	    case 'o':
		memnode = atoi(optarg);
	    break;

	    case 'n':
		nframes = atoi(optarg);
	    break;
	    
	    case 'b' :
		poolsize = atoi(optarg);
		break;

		case 'i' :
		bFields = 1;
		break;

		case 'c' :
		conntype = atoi(optarg);
		break;

		case 'N' :
		pluginName = optarg;
		if (!pluginName) {
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
			if (!xloc) {
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

	    case 'd':
		devicenum = atoi(optarg);
	    break;

	    case 'r':
		packing = atoi(optarg);
		break;

	    case 'm':
		format = atoi(optarg);
		break;

	    case 'v':
		vin = atoi(optarg);
	    break;

		case 'L':
		bLoop=1;
		break;

	    case 't':
		ext_trig = 1;
	    break;

	    case 'I':
		print_ids = 1;
	    break;

	    case 'z':
		if (sscanf(optarg,"%d/%d",&zoom_num,&zoom_denom) != 2 ||
		!zoom_num || !zoom_denom)
		{
		    fprintf(stderr, "%s: ERROR zoom format is <num>/<denom>\n",
			_progName);
		    exit(1);
		}
	    break;

		case 'l' :
		hostname = strdup(getenv("HOST"));
		conntype=DMNET_LOCAL;
		break;
	    
		case 'T' :
		hostname = strdup(optarg);
		break;

	    case 'q' :
		port = atoi(optarg);
		break;

		case 'W' :
		bNt =1;
		break;

	    case 'h':
	 	usage();
		exit(0);

	    default:
		usage();
		exit(1);
	    break;
	}
    }

    if (!hostname)
		err("Must provide -T hostname (or -l for local)\n");

	/* packing = (packing == VL_PACKING_YVYU_422_8)?VL_PACKING_RGBA_8:packing;
	format = (format == VL_FORMAT_SMPTE_YUV) ? VL_FORMAT_RGB : format; */

	if(bNt) {
		/* packing=(packing==VL_PACKING_YVYU_422_8 )?VL_PACKING_RGBA_8:packing;
		xsize = 320; ysize = 240; zoom_num = 1; zoom_denom = 2; */
		packing=( packing==VL_PACKING_YVYU_422_8) ? VL_PACKING_X4444_5551:packing;
		format = (format == VL_FORMAT_SMPTE_YUV ) ? VL_FORMAT_RGB : format;
	}

    if (dmNetOpen(&cnp) != DM_SUCCESS)
		err("dmNetOpen failed\n");
    
    if (dmParamsCreate(&plist) == DM_FAILURE)
		err("Create params failed\n");

	if (pluginName) {
		char fcName[128];

		conntype = DMNET_PLUGIN;
		if (dmParamsSetString(plist, DMNET_PLUGIN_NAME, pluginName)
				== DM_FAILURE)
			err("Couldn't set DMNET_PLUGIN_NAME");
			
		sprintf(fcName, "fc-%s", hostname);
		if (dmParamsSetString(plist, "DMNET_TRANSPORTER_REMOTE_HOSTNAME",
				fcName) == DM_FAILURE)
			err("Couldn't set DMNET_TRANSPORTER_REMOTE_HOSTNAME\n");
	}
	
	if (dmParamsSetInt(plist, DMNET_CONNECTION_TYPE, conntype)
			== DM_FAILURE)
		err("Couldn't set connection type\n");
	
    if (dmParamsSetInt(plist, DMNET_PORT, port) == DM_FAILURE)
		err("Couldn't set port number\n");

    if (dmParamsSetString(plist, DMNET_REMOTE_HOSTNAME, hostname)
		== DM_FAILURE)
		err("Couldn't set host name\n");
    
    if (dmNetConnect(cnp, plist) != DM_SUCCESS)
		err("dmNetConnect failed\n");
	if (DEBUG)
    	fprintf(stderr, "%s: connect\n", _progName);
    

    /* Connect to the daemon */
    if (!(svr = vlOpenVideo("")))
		err("Couldn't open video\n");

    /* Get the list of devices that the daemon supports */
    if (vlGetDeviceList(svr, &devlist) < 0) {
		fprintf(stderr,"%s: getting device list: %s\n",_progName,vlStrError(vlErrno));
		exit(1);
    }

    /* Make sure the user specified device (if any) is in the list */
    if ((devicenum >= (int)devlist.numDevices) || (devicenum < -1)) {
		if (devlist.numDevices == 1)
	    	fprintf(stderr,"%s: The device number must be 0\n",_progName);
		else
	    	fprintf(stderr,"%s: The device number must be between 0 and %d\n",
				_progName, devlist.numDevices-1);
		exit(1);
    }

    /* Set up a drain node in memory */
    if ((drn = vlGetNode(svr, VL_DRN, VL_MEM, memnode)) == -1) {
		vlPerror("vlGetNode drain");
		exit(1);
	}

    /* Set up a source node from video */
    if ((src = vlGetNode(svr, VL_SRC, VL_VIDEO, vin)) == -1) {
		vlPerror("vlGetNode drain");
		exit(1);
	}

    /*
     * If the user didn't specify a device create a path using the first
     * device that will support it
     */
    if (devicenum == -1) {
		if ((path = vlCreatePath(svr, VL_ANY, src, drn)) < 0) {
	    	vlPerror("vlCreatePath");
	    	exit(1);
		}
		/* Get the device number and name */
		devicenum = vlGetDevice(svr, path);
		deviceName = devlist.devices[devicenum].name;
    } else {
    /* Create a path using the user specified device */
		deviceName = devlist.devices[devicenum].name;
		deviceId   = devlist.devices[devicenum].dev;
		if ((path = vlCreatePath(svr, deviceId, src, drn)) < 0) {
	    	vlPerror("vlCreatePath user-specified");
	    	exit(1);
		}
    }

    /* Print the node and path IDs for cmd line users */
    if (print_ids) {
		printf("Device Name = %s\n", deviceName);
		printf("VIDTOMEM NODE IDs:\n");
		printf("video source = %d\n", src);
		printf("memory drain = %d\n", drn);
		printf("VIDEO TO MEMORY PATH ID = %d\n", path);
    }

    /* Set up the hardware for and define the usage of the path */
    if (vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE) < 0) {
		vlPerror("setup paths");
		exit(1);
    }

    /*
     * Set the memory node's timing based upon the video source's timing
     */
    if (vlGetControl(svr, path, src, VL_TIMING, &val) == VLSuccess) {

	/* Ignore BadControl error when setting memory timing,
	 * perhaps the memory node doesn't have a timing control;
	 */
		if ((vlSetControl(svr, path, drn, VL_TIMING, &val) < 0)
	    	&& (vlErrno != VLBadControl)) {
	    	vlPerror("SetControl Failed");
	    	exit(1);
		}
    }

    /* Set the zoom ratio */
    val.fractVal.numerator = zoom_num;
    val.fractVal.denominator = zoom_denom;
    if (vlSetControl(svr, path, drn, VL_ZOOM, &val) <0)
		vlPerror("vlSetControl(VL_ZOOM)");

    /* Did the user get the desired zoom ratio? */
    vlGetControl(svr, path, drn, VL_ZOOM, &val);
    if (val.fractVal.numerator != zoom_num ||
	val.fractVal.denominator != zoom_denom)
	fprintf(stderr, 
		"%s: Unable to set requested zoom. Using zoom factor of %d/%d\n", 
		_progName, val.fractVal.numerator, val.fractVal.denominator);

    /* This is fixed here - ensure same at receiver */
    val.intVal = packing;
    if (vlSetControl(svr, path, drn, VL_PACKING, &val) < 0) {
		vlPerror("vlSetControl(VL_PACKING)");
		exit(1);
    }
    
    /* This is fixed here - ensure same at receiver */
    val.intVal = format;
    if (vlSetControl(svr, path, drn, VL_FORMAT, &val) < 0) { 
		vlPerror("vlSetControl(VL_FORMAT)");
		exit(1);
    }


	if (bFields) {
		val.intVal = capType;
		if(vlSetControl(svr, path, drn, VL_CAP_TYPE, &val) <0) {
			vlPerror("vlSetControl(VL_CAP_TYPE)");
			exit(1);
		}
	}

    /* Set the video size to what the user requested */
    if (!xsize)
		vlGetControl(svr, path, drn, VL_SIZE, &val);
    else
    {
		val.xyVal.x = xsize;
		val.xyVal.y = ysize;
		if (vlSetControl(svr, path, drn, VL_SIZE, &val) <0)
			vlPerror("vlSetControl(VL_SIZE)");

		/* Make sure the hardware supports this size */
		vlGetControl(svr, path, drn, VL_SIZE, &val);
    }

    xsize = val.xyVal.x;
    ysize = val.xyVal.y;

    frameSize = vlGetTransferSize(svr, path);
    fprintf(stderr, 
		"%s: frameSize=%d, xsize=%d, ysize=%d, format=%d, packing=%d\n", 
	    _progName, frameSize, xsize, ysize, format, packing);

    /* Create and register a buffer of poolsize frames... */
    if (dmBufferSetPoolDefaults(plist, poolsize, frameSize,
                                DM_TRUE, DM_FALSE))
		err("dmBufferSetPoolDefaults\n");

    if (dmNetGetParams(cnp, plist) == DM_FAILURE)
		err("dmNetGetParams failed\n");

	/* vlDMPoolGetParams on 6.3 O2 */
    if (vlDMGetParams(svr, path, drn, plist) == DM_FAILURE) {
		vlPerror("vlDMGetParams");
		exit(1);
	}

    if (dmBufferCreatePool(plist, &pool) != DM_SUCCESS)
		err("Create Pool failed\n");

    if(!pool)
      err("No Pool\n");

    if (dmNetRegisterPool(cnp, pool) != DM_SUCCESS)
		err("dmNetRegisterPool failed\n");

     if (vlDMPoolRegister(svr, path, drn, pool) == DM_FAILURE) {
        vlPerror("vlDMPoolRegister");
        dmBufferDestroyPool(pool);
        exit(1);
    }

	blockSize = dmParamsGetInt(plist, DM_BUFFER_SIZE);
	if (DEBUG)
    	fprintf(stderr, "%s: Pool blk size = %d\n", _progName, blockSize);

    dmParamsDestroy(plist);

    /*
     * Fill the transfer descriptor for a discrete transfer of nframes
     * frames. If the external trigger option is set, we use VLDeviceEvent
     * for a trigger, otherwise trigger immediately. Note that this may not
     * work well on all devices, as VLDeviceEvent it not well-defined.
     */
    xferDesc.mode = VL_TRANSFER_MODE_DISCRETE;
    xferDesc.count = nframes;
    xferDesc.delay = 0;
    if (ext_trig)
        xferDesc.trigger = VLDeviceEvent;
    else
        xferDesc.trigger = VLTriggerImmediate;

	/* Select what events we're interested in */
	if (vlSelectEvents(svr, path, VLTransferCompleteMask|VLTransferFailedMask
		|VLSequenceLostMask) == -1) {
		vlPerror("vlSelectEvents");
		exit(1);
	}

    /* Begin the data transfer */
    if (vlBeginTransfer(svr, path, 1, &xferDesc)) {
		vlPerror("vlBeginTransfer");
		exit(1);
    }

#ifdef DMEDIA_6_3
	if (vlPathGetFD(svr, path, &pathfd)) {
#else
	if ((pathfd = vlNodeGetFd(svr, path, drn)) == -1) {
#endif
		vlPerror("vlNodeGetFd");
		exit(1);
	}

	gettimeofday(&startTime, 0);

	for (;;) {
		FD_ZERO(&fdset);
		FD_SET(pathfd, &fdset);

		/* wait for video events */
		if ((nready = select(pathfd+1, &fdset, 0, 0, 0)) == -1) {
			perror("select on video fd");
			exit(1);
		}

		if(nready ==0) continue;

#ifdef DMEDIA_6_3
		while(vlEventRecv(svr, path, &event) == DM_SUCCESS) {
#else
		while(vlNextEvent(svr, &event) == DM_SUCCESS) {
#endif
			switch(event.reason) {
    		DMbuffer dmbuf;
			case VLTransferComplete:
			/* Get the dmBuffer, and send it over dmnet */
#ifdef DMEDIA_6_3
				if (vlEventToDMBuffer(&event, &dmbuf) == VLSuccess) {
#else
				if (vlDMBufferGetValid(svr, path, drn, &dmbuf) == DM_SUCCESS){
#endif
					if (bLoop || frame < nframes) {
						frame++;
						if (DEBUG)
							fprintf(stderr, "%s: Sending frame %d of %d\n", 
							_progName, frame, nframes);
						if (bNt) fixEndian(dmbuf);
						/* if (frame>20 && frame <27) {
							printf("Waiting\n");
							getchar();
						} */
						if (dmNetSend(cnp, dmbuf) != DM_SUCCESS) {
							if (errno == EBUSY) {
								fprintf(stderr, 
									"%s: dmNetSend napping on frame %d\n", 
									_progName, frame);
								sginap(1);
							} else {
								fprintf(stderr, "%s: Could not send frame %d\n",
									_progName, frame);
								perror("dmNetSend: ");
								docleanup(1);
							}
						}
					}
					sentFrames++;
					dmBufferFree(dmbuf);
				}
				if (!bLoop && frame >= nframes) docleanup(0);
				break;
	
				case VLSequenceLost: 
				frame++;
				if (DEBUG)
					fprintf(stderr, "%s: Lost frame %d\n", _progName, frame);
				if (!bLoop && frame >= nframes) docleanup(0);
				break;
	
				case VLTransferFailed:
				fprintf(stderr,"%s: Transfer failed.\n",_progName);
				docleanup(1);
				break;
	
				default:
				break;
			}
		}
	}
}

/*
 * Do general cleanup before exiting. All of our resources should
 * be freed.
 */
void
docleanup(int ret)
{
	printStats();
	dmNetClose(cnp);
    vlEndTransfer(svr, path);
    vlDMPoolDeregister(svr, path, drn, pool);
    dmBufferDestroyPool(pool);
    vlDestroyPath(svr, path);
    vlCloseVideo(svr);
    exit(ret);
}

void
printStats(void)
{
	struct timeval stopTime;
	long sec, usec;
	double diff;

	gettimeofday(&stopTime, 0);
	sec = stopTime.tv_sec - startTime.tv_sec;
	usec = stopTime.tv_usec - startTime.tv_usec;
	diff = sec + usec/1000000.0;

	printf("\n%d frames sent in %.3f real seconds @ %.3f fps, %.3f KB/sec\n",
		sentFrames, diff, sentFrames/diff, sentFrames/diff*(blockSize/1000.0));
}

void
fixEndian(DMbuffer dmbuf)
{
	register unsigned short *head;
	int i;

	head = (unsigned short*) dmBufferMapData(dmbuf);
	for (i=0; i<xsize*ysize; i++, head++) {
		/* RGBA -> BGRA
		register unsigned r1, r2, r3, r4;
		r1 = (*head & 0xFF000000) >>16;
		r2 = (*head & 0x00FF0000) >>0;
		r3 = (*head & 0x0000FF00) <<16;
		r4 = (*head & 0x000000FF) <<0;
		*head = r1 | r2 | r3 | r4; */

		/* ABGR -> BGRA
		*head = *head<<8; */

		/* 555 -> 555 */
		register unsigned short r1, r2;
		r1 = (*head & 0xFF00)>>8;
		r2 = (*head & 0x00FF)<<8;
		*head = r1 | r2;
	}
}
