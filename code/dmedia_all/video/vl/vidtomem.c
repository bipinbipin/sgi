/*
 * File:          vidtomem.c - vidtomem using the dmBuffer interface
 *
 * Usage:         vidtomem [-f filename] [-w] [-d] [-c numframes] [-n devicenum]
 *                         [-r] [-v videosource] [-z n/d] [-t] [-I] [-s wxh] [-h]
 *			   [-o memnode]
 * Description:   Vidtomem captures an incoming video stream to memory
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
 *		  vlAddCallback()
 *                vlSetControl()
 *                vlGetControl()
 *		  vlCreateBuffer()
#ifdef KEEP_DMRBCOMPAT
 *		  vlRegisterBuffer()
 *		  vlGetActiveRegion()
 *		  vlGetNextValid()
 *		  vlPutFree()
#else
 *                vlDMPoolRegister()
 *		  vlDMBufferGetValid()
 *		  vlDMPoolRegister()
 *		  dmParamsDestroy()
 *		  dmBufferCreatePool()
 *		  vlDMGetParams();
 *		  dmBufferSetPoolDefaults()
 *		  dmBufferMapData()
 *		  vlDMBufferGetValid()
 *		  dmBufferFree()
#endif
 *                vlGetTransferSize()
 *                vlBeginTransfer()
 *                vlEndTransfer()
 *                vlDeregisterBuffer()
 *                vlDestroyPath()
 *		  vlDestroyBuffer()
 *                vlCloseVideo()
 *                vlPerror()
 *                vlStrError()
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
#include "tables.h"


char *deviceName;
char *_progName;

#ifndef MAXNAMLEN
#define MAXNAMLEN 1024
#endif
int icount = 0;
char iname[MAXNAMLEN];
char iname_tmp[MAXNAMLEN];
char outfilename[MAXNAMLEN - 5] = "";
int imageCount = 1;
int writeFrame = 1;
int forcedWriteFrame = 0;
int rawMode = 0;
int pixelPacking = -1;
int displayFrame = 0;
VLServer svr;
VLBuffer buffer;
int frames = 0;
VLPath path;
VLNode src, drn;
int xsize = 0;
int ysize = 0;
int xsetsize = 0;
int ysetsize = 0;
int frameSize;
#ifndef KEEP_DMRBCOMPAT
DMbufferpool pool;
#endif

void docleanup(int);
void ProcessEvent(VLServer, VLEvent *, void *);
void usage(void);
char *get_packing(void);

#define GRABFILE    "out"

#define USAGE \
"%s: [-f <filename>] [-w] [-d] [-s wxh] [-n <device>] [-v <video-node> ]\n\
\t\t[-o <memory-node> ] [-c <num>] [-I] [-r [packing]] [-t] \n\
\t\t[-z n/d] [-h]\n\
\n\
\t-f filename     output filename\n\
\t-w              don't write frame to disk\n\
\t-d              display captured frame\n\
\t-s wxh          frame dimensions\n\
\t-c num          number of frames to capture\n\
\t-n device       device number to use\n\
\t-r [packing]    raw mode output, with optional packing specified\n\
\t                use with -r\n\
\t-v video-node   video source node number\n\
\t-o memory-node  memory drain node number\n\
\t-t              wait for external trigger\n\
\t-I              print node and path IDs\n\
\t-z n/d          zoom the image by n/d\n\
\t-h              this help message\n"

void
usage(void)
{
    fprintf(stderr, USAGE, _progName);
}

/* Dump data to image file */
void
DumpImage(char *data, int xinputsize, int xoutputsize, int yinputsize,
          int youtputsize, int transferSize)
{
    IMAGE *image;
    int fd, ret;
    unsigned short rbuf[1024];
    unsigned short gbuf[1024];
    unsigned short bbuf[1024];
    char *outdata, *temp;
    int fileSize;
    int x, y, z;

    sprintf(iname,"%s-%05d", outfilename, icount++);
    if (rawMode)
    {
	strcat(iname, ".");
	strcat(iname, get_packing());

	sprintf(iname_tmp, "%s.%d", iname, getpid());

	fd = open(iname_tmp, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (fd < 0)
	{
	    perror(iname_tmp);
	    exit(1);
	}
	/* Do clipping if requested */
	if ((xinputsize != xoutputsize)||(yinputsize != youtputsize))
	{
	    fileSize = youtputsize * xoutputsize * 2;
            outdata = (char *)malloc(fileSize*sizeof(char));
	    if (!outdata)
	    {
		fprintf(stderr,"DumpImage: can't malloc enough memory\n");
		exit(1);
	    }
	    temp = outdata;
	    for (y = 0; y < youtputsize; y++)
	    {
		for (x = 0; x < xoutputsize; x++)
		{
		   *(temp++) = *(data++);
		   *(temp++) = *(data++);
		}
		/* Clip data to user requested frame size */
		for (x = xoutputsize; x < xinputsize; x++)
		{
		   data++;
		   data++;
		}
	    }
	    ret = write(fd, outdata, fileSize);
	    free(outdata);
      	}
	else
	{
	    fileSize = transferSize;
	    ret = write(fd, data, fileSize);
	}
	if (ret < fileSize)
	{
	    perror(iname_tmp);
	    exit(1);
	}
	close(fd);

	if (rename(iname_tmp, iname) < 0) {
	    fprintf(stderr, "Cannot rename tmp file to %s, errno=%d\n",
		    iname, errno);
	    unlink(iname_tmp);
	}
    } else
    {
	if (writeFrame)
	{
	    strcat(iname, ".rgb");
	    sprintf(iname_tmp, "%s.%d", iname, getpid());
	    image = iopen(iname_tmp, "w", RLE(1), 3, xoutputsize, youtputsize, 3);
	}

	/* Write RGB data to buffers */
	for (y=0;y<youtputsize;y++)
	{
	/* One pixel at a time along scan line */
	    for(x=0;x<xoutputsize;x++)
	    {
		data++;
		bbuf[x] = *(data++);
		gbuf[x] = *(data++);
		rbuf[x] = *(data++);
	    }
	    /* Clip data to user requested frame size */
	    for (x=xoutputsize; x<xinputsize; x++)
	    {
	       data++;
	       data++;
	       data++;
	       data++;
	    }
	    if (writeFrame)
	    {
		/* Write RGB buffers to disk */
		putrow(image, rbuf, youtputsize - y - 1, 0);
		putrow(image, gbuf, youtputsize - y - 1, 1);
		putrow(image, bbuf, youtputsize - y - 1, 2);
	    }
	}
	if (writeFrame) {
	    iclose(image);

	    if (rename(iname_tmp, iname) < 0) {
		fprintf(stderr, "Cannot rename tmp file to %s, errno=%d\n",
			iname, errno);
		unlink(iname_tmp);
	    }
	}
    }
}

main(int argc, char **argv)
{
    VLDevList devlist;
    VLDev     deviceId;
    VLControlValue val;
    int c;
    int vin = VL_ANY;
    int devicenum = -1;
    int memnode = VL_ANY;
    int zoom_num = 1;
    int zoom_denom = 1;
    int ext_trig = 0;
    int print_ids = 0;
    VLTransferDescriptor xferDesc;
#ifndef KEEP_DMRBCOMPAT
    DMparams* plist;
#endif

    _progName = argv[0];

    /*
     * Parse command line options
     * -f   output filename
     * -c   capture n frames
     * -w   don't write frame to disk
     * -d   display captured frame
     * -s   set window size wxh
     * -n   use device number n
     * -r   raw mode output (no YUV->RGB or de-interlace)
     * -v   video source node number n
     * -o   video memory node number 
     * -t   wait for external trigger
     * -z   set zoom ratio n/d
     * -I   publish node and path IDs for command line interface users
     * -h   help message
     */
    while ((c = getopt(argc, argv, "f:wdn:c:rhs:v:Iz:to:")) != EOF)
    {
	switch (c)
	{
	    case 'o':
		memnode = atoi(optarg);
	    break;

	    case 'f':
		sprintf(outfilename,"%s",optarg);
	    break;

	    case 'c':
		imageCount = atoi(optarg);
	    break;

	    case 'w':
		writeFrame = 0;
	    break;

	    case 'd':
		displayFrame = 1;
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

	    case 'n':
		devicenum = atoi(optarg);
	    break;

	    case 'r':
		rawMode = 1;
		if (optind < argc && argv[optind][0] != '-') {
		    pixelPacking = atoi(argv[optind]);
		    optind++;
		}
	    break;

	    case 'v':
		vin = atoi(optarg);
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

	    case 'h':
	 	usage();
		exit(0);

	    default:
		usage();
		exit(1);
	    break;
	}
    }

    /* Ensure that writeFrame is enabled if -d option specified */
    if (displayFrame && !writeFrame)
    {
	writeFrame = 1;
	forcedWriteFrame = 1;
    }

    if (writeFrame)
    {
	if (outfilename[0] == '\0')
	    strcpy(outfilename, GRABFILE);
    }

    /* Connect to the daemon */
    if (!(svr = vlOpenVideo("")))
    {
	fprintf(stderr, "%s: couldn't open video\n", _progName);
	exit(1);
    }

    /* Get the list of devices that the daemon supports */
    if (vlGetDeviceList(svr, &devlist) < 0)
    {
	fprintf(stderr,"%s: getting device list: %s\n",_progName,vlStrError(vlErrno));
	exit(1);
    }

    /* Make sure the user specified device (if any) is in the list */
    if ((devicenum >= (int)devlist.numDevices) || (devicenum < -1))
    {
	if (devlist.numDevices == 1)
	    fprintf(stderr,"%s: The device number must be 0\n",_progName);
	else
	    fprintf(stderr,"%s: The device number must be between 0 and %d\n",
		_progName, devlist.numDevices-1);
	exit(1);
    }

    /* Set up a drain node in memory */
    drn = vlGetNode(svr, VL_DRN, VL_MEM, memnode);

    /* Set up a source node from video */
    src = vlGetNode(svr, VL_SRC, VL_VIDEO, vin);

    /*
     * If the user didn't specify a device create a path using the first
     * device that will support it
     */
    if (devicenum == -1)
    {
	if ((path = vlCreatePath(svr, VL_ANY, src, drn)) < 0)
	{
	    vlPerror(_progName);
	    exit(1);
	}
	/* Get the device number and name */
	devicenum = vlGetDevice(svr, path);
	deviceName = devlist.devices[devicenum].name;
    }
    else
    /* Create a path using the user specified device */
    {
	deviceName = devlist.devices[devicenum].name;
	deviceId   = devlist.devices[devicenum].dev;
	if ((path = vlCreatePath(svr, deviceId, src, drn)) < 0)
	{
	    vlPerror(_progName);
	    exit(1);
	}
    }

    /* Print the node and path IDs for cmd line users */
    if (print_ids)
    {
	printf("VIDTOMEM NODE IDs:\n");
	printf("video source = %d\n", src);
	printf("memory drain = %d\n", drn);
	printf("VIDEO TO MEMORY PATH ID = %d\n", path);
    }

    /* Set up the hardware for and define the usage of the path */
    if (vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE) < 0)
    {
	vlPerror(_progName);
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

    /*
     * Specify what path-related events we want to receive.
     * In this example we want transfer complete and transfer failed
     * events only.
     */
    vlSelectEvents(svr, path, VLTransferCompleteMask|VLTransferFailedMask);

    /* Set the zoom ratio */
    val.fractVal.numerator = zoom_num;
    val.fractVal.denominator = zoom_denom;
    vlSetControl(svr, path, drn, VL_ZOOM, &val);

    /* Did the user get the desired zoom ratio? */
    vlGetControl(svr, path, drn, VL_ZOOM, &val);
    if (val.fractVal.numerator != zoom_num ||
	val.fractVal.denominator != zoom_denom)
	fprintf(stderr, "%s: Unable to set requested zoom. Using zoom factor of %d/%d\n", _progName,
		val.fractVal.numerator, val.fractVal.denominator);

    if (pixelPacking != -1)
    { 
        val.intVal = pixelPacking;
        if (vlSetControl(svr, path, drn, VL_PACKING, &val) < 0)
        {
            vlPerror("vlSetControl(VL_PACKING)");
            exit(1);
        }
    }

     /* Set packing to RGB if user requested it */
    if (!rawMode)
    {
	val.intVal = VL_PACKING_RGB_8;
	vlSetControl(svr, path, drn, VL_PACKING, &val);
        val.intVal = VL_FORMAT_RGB;
        vlSetControl(svr, path, drn, VL_FORMAT, &val);
    }

    /* Set the video size to what the user requested */
    if (!xsize)
	vlGetControl(svr, path, drn, VL_SIZE, &val);
    else
    {
	val.xyVal.x = xsetsize = xsize;
	val.xyVal.y = ysetsize = ysize;
	vlSetControl(svr, path, drn, VL_SIZE, &val);

	/* Make sure the hardware supports this size */
	vlGetControl(svr, path, drn, VL_SIZE, &val);
    }

    xsize = val.xyVal.x;
    ysize = val.xyVal.y;

    if (!xsetsize)
    {
	xsetsize = xsize;
	ysetsize = ysize;
    }
    else
    {
	if (xsetsize > xsize || xsetsize <= 0)
	    xsetsize = xsize;
	if (ysetsize > ysize || ysetsize <= 0)
	    ysetsize = ysize;
    }

    frameSize = vlGetTransferSize(svr, path);

    /* Set ProcessEvent() as the callback for a transfer events */
    vlAddCallback(svr, path, VLTransferCompleteMask|VLTransferFailedMask,
			         ProcessEvent, NULL);

    /* Create and register a buffer of imageCount frames... */
#ifdef KEEP_DMRBCOMPAT
    buffer = vlCreateBuffer(svr, path, drn, imageCount);
    if (buffer == NULL)
    {
	vlPerror(_progName);
	exit(1);
    }
    if (vlRegisterBuffer(svr, path, drn, buffer))
    {
	vlPerror(_progName);
	exit(1);
    }
#else
    if (dmParamsCreate(&plist)) {
        printf("dmParamsCreate failed!\n");
        exit(1);
    }
    dmBufferSetPoolDefaults(plist, imageCount, frameSize,
                                DM_TRUE, DM_FALSE);
    vlDMGetParams(svr, path, drn, plist);
    if (dmBufferCreatePool(plist, &pool) != DM_SUCCESS) {
        printf("dmBufferCreatePool failed %s\n", dmGetError(0, 0));
    }
    dmParamsDestroy(plist);

    if (!pool) {
        vlPerror(_progName);
        exit(1);
    }

    /* Associate the pool with the path */
    if (vlDMPoolRegister(svr, path, drn, pool)) {
        vlPerror(argv[0]);
        dmBufferDestroyPool(pool);
        exit(1);
    }
#endif

    /*
     * Fill the transfer descriptor for a discrete transfer of imageCount
     * frames. If the external trigger option is set, we use VLDeviceEvent
     * for a trigger, otherwise trigger immediately. Note that this may not
     * work well on all devices, as VLDeviceEvent it not well-defined.
     */
    xferDesc.mode = VL_TRANSFER_MODE_DISCRETE;
    xferDesc.count = imageCount;
    xferDesc.delay = 0;
    if (ext_trig)
        xferDesc.trigger = VLDeviceEvent;
    else
        xferDesc.trigger = VLTriggerImmediate;

    /* Begin the data transfer */
    if (vlBeginTransfer(svr, path, 1, &xferDesc))
    {
	vlPerror(_progName);
	exit(1);
    }

    /* Loop until all requested frames are grabbed */
    vlMainLoop();
 }

/*
 * Handle video library events
 * (only transfer complete and transfer failed events in this example)
 */
void
ProcessEvent(VLServer svr, VLEvent *ev, void *data)
{
    VLInfoPtr info;
    char *dataPtr;
#ifndef KEEP_DMRBCOMPAT
    DMbuffer dmbuf;
#endif

    switch (ev->reason)
    {
	case VLTransferComplete:
	    /* Get the number and location of frames captured */
#ifdef KEEP_DMRBCOMPAT
	    while (info = vlGetNextValid(svr, buffer))
	    {
		/* Get a pointer to the frame(s) */
		dataPtr = vlGetActiveRegion(svr, buffer, info);
#else
	    while (vlDMBufferGetValid(svr, path, drn, &dmbuf) == VLSuccess) {
		/* Get a pointer to the frame(s) */
		if ((dataPtr = dmBufferMapData(dmbuf)) <= 0) {
		    printf("dmBufferMapData failed %s\n", dmGetError(0,0));
		    docleanup(1);
		}
#endif

		if (frames < imageCount)
		{
		    frames++;

		    /* Set the filename if user requested write to disk */
		    if (writeFrame)
		    {
			sprintf(iname,"%s-%05d", outfilename, frames);
			if (!rawMode)
			    strcat(iname, ".rgb");
			else
			{
			    strcat(iname, ".");
			    strcat(iname, get_packing());
			}
		    }

		    /* Write the frame(s) to memory (and to disk if requested) */
		    DumpImage(dataPtr, xsize, xsetsize, ysize, ysetsize, frameSize);

		    if (writeFrame && !forcedWriteFrame)
			fprintf(stderr,"%s: saved image to file %s\n", _progName, iname);

		    /* Write to display if requested */
		    if (displayFrame && !rawMode) {
			char cmd[100];

			sprintf(cmd, "ipaste %s", iname);
			system(cmd);
			if (forcedWriteFrame)
			    unlink(iname);
		    }
		    else if (displayFrame) {
			fprintf(stderr, "Can't display raw frame\n");
		    }
		}
		/* Finished with frame, unlock the buffer */
#ifdef KEEP_DMRBCOMPAT
		vlPutFree(svr, buffer);
#else
		dmBufferFree(dmbuf);
#endif
	    }
	    /* Exit when all frames are grabbed */
      	    if (frames == imageCount)
	        docleanup(0);
	break;

	case VLTransferFailed:
	    fprintf(stderr,"%s: Transfer failed.\n",_progName);
	    docleanup(1);
	break;

	default:
	break;
    }
}


/*
 * Do general cleanup before exiting. All of our resources should
 * be freed.
 */
void
docleanup(int ret)
{
    vlEndTransfer(svr, path);
#ifdef KEEP_DMRBCOMPAT
    vlDeregisterBuffer(svr, path, drn, buffer);
    vlDestroyBuffer(svr, buffer);
#else
    vlDMPoolDeregister(svr, path, drn, pool);
    dmBufferDestroyPool(pool);
#endif
    vlDestroyPath(svr, path);
    vlCloseVideo(svr);
    exit(ret);
}

/* 
   Search table for packing string, which will 
   be appended to the filename 
*/

char *
get_packing(void)	
{
    VLControlValue val;
    char *packing = "unknown";
    int i;

    if (vlGetControl(svr, path, drn, VL_PACKING, &val) < 0)
	vlPerror("Getting VL_PACKING");

    for (i = 0; i < VMMAX; i++) {
	if ( val.intVal == packingTable[i].packing ) {
		packing = packingTable[i].packingString;
		break;
	}
    }

    return packing;
}
