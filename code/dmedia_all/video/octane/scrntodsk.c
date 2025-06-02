/*
 * Files:         scrntodsk.c
 *
 * Usage:         scrntodsk  [-m mem node number] [-s scrn node number]
 *                           [-c frame count] [-o Xxy] -f filename
 *
 * Description:   Vidtodsk captures a file of raw SMPTE YUV 4:2:2 frames from
 *                video in. If run as root, non-degrading CPU priority will
 *                be requested and process memory will be locked.
 *
 */

#include <errno.h>
#include <sys/lock.h>
#include <sys/schedctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>	/* writev() */
#include <sys/param.h> 

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

char *_progName;

char outfilename[MAXPATHLEN] = "";
int imageCount = MAXINT;
VLServer svr;
VLBuffer buffer;
VLPath path;
VLNode src, drn, viddrn;
int frameSize;
int fd;
int frames_captured = 0;

void docleanup(int);
void usage(void);
void dmrb_ready(VLServer svr);
void ProcessEvent(VLServer svr);

#define DMBUFSIZE 6

#define USAGE \
"%s: [-c frame count ] [-v videonode ] [-m memnode ] -f filename\n\
\t\t[ -o XxY ] [-h]\n\
\t-f\toutput filename\n\
\t-c\tnumber of frames to capture\n\
\t-v\tvideo source node number\n\
\t-m\tmemory drain node number\n\
\t-o\tXxY\origin tscreen area to capture (upper left corner)\n\
\t-h\tthis help message\n"

void
usage(void)
{
    fprintf(stderr, USAGE, _progName);
}

int
main(int argc, char **argv)
{
    VLControlValue val;
    int c, ret;
    int scrnnode = VL_ANY;
    int memnode = VL_ANY;
    int nfds;
    fd_set readfds, t_readfds, exceptfds, t_exceptfds;
    int vlfd, bufferfd;
    int originx = 0, originy = 0;

    _progName = argv[0];

    /*
     * Parse command line options
     * -f   output filename
     * -c   capture n frames
     * -v   video source node number n
     * -m   mem node number
     * -h   help message
     */
    while ((c = getopt(argc, argv, "hf:c:v:m:")) != EOF) {
	switch (c) {
	    case 'f':
		sprintf(outfilename,"%s",optarg);
	    	break;

	    case 'c':
		imageCount = atoi(optarg);
	    	break;

	    case 'v':
		scrnnode = atoi(optarg);
	     	break;

	    case 'm':
		memnode = atoi(optarg);
		break;

	    case 'o':
		sscanf(optarg, "%dx%d", originx, originy);
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


   /*
    * Lock text and data segments into memory, then gain non-degrading
    * priority.
    */
    if (plock(PROCLOCK)==-1) {
	perror("Warning: Degraded performance: Cannot lock memory");
    }
    if (schedctl(NDPRI, getpid(), NDPHIMIN) == -1) {
	perror("Warning: Degraded performance: No realtime CPU priority");
    }

    if (outfilename[0] == '\0') {
	usage();
	exit(1);
    }

    /* Connect to the daemon */
    if (!(svr = vlOpenVideo(""))) {
	fprintf(stderr, "%s: couldn't open video\n", _progName);
	exit(1);
    }

    /* Create the following path:
     * 
     *        /--------> video out
     * scrn---
     *        \--------> memory
     */
    src = vlGetNode(svr, VL_SRC, VL_SCREEN, scrnnode);
    drn = vlGetNode(svr, VL_DRN, VL_MEM, memnode);
    viddrn = vlGetNode(svr, VL_DRN, VL_VIDEO, VL_ANY);
    if ((path = vlCreatePath(svr, VL_ANY, src, drn)) < 0) {
	vlPerror(_progName);
	exit(1);
    }
    if (vlAddNode(svr, path, viddrn) == -1) {
	vlPerror(_progName);
	exit(1);
    }

   /* 
    * Set up the hardware for and define the usage of the path 
    */
    if (vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE) < 0) {
	vlPerror(_progName);
	exit(1);
    }

   /* 
    * Disable the deinterlacing filter on the screen node, and set the
    * origin to (0, 0) or the user-requested coordinates.
    */
    val.intVal = 0;
    if (vlSetControl(svr, path, src, VL_MGV_DEINTERLACE, &val) < 0) {
	vlPerror("Unable to disable deinterlacing filter");
	exit(1);
    }
    val.xyVal.x = originx;
    val.xyVal.y = originy;
    if (vlSetControl(svr, path, src, VL_ORIGIN, &val) < 0) {
	vlPerror("Unable to set origin to requested coordinates");
	exit(1);
    }

   /*
    * Set the buffer quantum to 4096 bytes. This satisfies the (file) device
    * block size requirements for raw or direct I/O, as well as the Ciprico
    * 6900 array stripe size
    */
    val.intVal = 4096;
    if (vlSetControl(svr, path, drn, VL_MGV_BUFFER_QUANTUM, &val) < 0) {
	vlPerror("Unable to set buffer quantum to requested value");
	exit(1);
    }

   /*
    * Specify what path-related events we want to receive.
    * In this example we want transfer complete and transfer failed
    * events only.
    */
    vlSelectEvents(svr, path, VLTransferFailedMask);

   /*
    * Capture in YUV 4:2:2 format.
    */
    val.intVal = VL_PACKING_YVYU_422_8;
    if (vlSetControl(svr, path, drn, VL_PACKING, &val) < 0) {
        vlPerror("vlSetControl(VL_PACKING)");
        exit(1);
    }

    frameSize = vlGetTransferSize(svr, path);
    printf("Frame size = %d\n", frameSize);

   /* 
    * Create and register a buffer. Mark it "no access." This enables some
    * optimizations, for example cache flushing may be disabled.  
    */
    buffer = vlCreateBuffer(svr, path, drn, DMBUFSIZE);
    if (buffer == NULL) {
	vlPerror(_progName);
	exit(1);
    }
    vlBufferAdvise(buffer, VL_BUFFER_ADVISE_NOACCESS);
    if (vlRegisterBuffer(svr, path, drn, buffer)) {
	vlPerror(_progName);
	exit(1);
    }

   /*
    * Now open the output file, preferably with direct I/O.
    */
    fd = open(outfilename, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (fd < 0) fd = open(outfilename, O_WRONLY|O_CREAT|O_TRUNC, 0666);

    if (fd < 0) {
	perror(outfilename);
	exit(1);
    }

   /*
    * Create select desriptors so we know when the dmrb is ready for more
    * data, or when VL sends us an event.
    */
    bufferfd = vlBufferGetFd(buffer);
    vlfd = vlGetFD(svr);
    assert(bufferfd > 0 && vlfd > 0 );

    FD_ZERO(&readfds);
    FD_SET(vlfd, &readfds);
    FD_SET(bufferfd, &readfds);
    FD_ZERO(&exceptfds);
    FD_SET(vlfd, &exceptfds);
    FD_SET(bufferfd, &exceptfds);
    if (vlfd > bufferfd) nfds = vlfd+1;
    else nfds = bufferfd+1;

   /*
    * Begin the data transfer, and loop until all requested frames
    * have been captured.
    */
    if (vlBeginTransfer(svr, path, 0, NULL)) {
	vlPerror(_progName);
	exit(1);
    }

    while (frames_captured < imageCount) {
	t_readfds = readfds;
	t_exceptfds = exceptfds;

	ret = select(nfds, &t_readfds, NULL, &t_exceptfds, NULL);

	if (ret == -1 && errno != EINTR) break;

	if (FD_ISSET(bufferfd, &t_readfds)) dmrb_ready(svr);
	else if (FD_ISSET(vlfd, &t_readfds)) ProcessEvent(svr);

	if (FD_ISSET(bufferfd, &t_exceptfds)) {
	    fprintf(stderr, "Exception condition on ring buffer descriptor\n");
	    break;
	}
	else if (FD_ISSET(vlfd, &t_exceptfds)) {
	    fprintf(stderr, "Exceptional condition on VL connection\n");
	    break;
	}
    }

    docleanup(0);

    return 0;
}

/*
 * This function is called when the buffer fd is ready for reading, 
 * indicating that a buffer is ready for the application. Obtain it and
 * write it to the file.
 */
void
dmrb_ready(VLServer svr)
{
    VLInfoPtr info;
    char *dataPtr;

   /*
    * Loop as long as there are buffers left to read.
    */
    while (frames_captured < imageCount &&
        (info = vlGetNextValid(svr, buffer))) {

       /*
	* Info points to buffer meta data ("info block"). Get a pointer to 
	* the data so we can write it out.
	*/
	dataPtr  = vlGetActiveRegion(svr, buffer, info);
	write(fd, dataPtr, frameSize);

       /*
	* Return the buffer to the VL so the video device can re-use it.
	*/
	vlPutFree(svr, buffer);
	frames_captured++;

    	printf("frame %d\n", frames_captured);
    }
}


/*
 * Handle video library events
 * (only transfer complete and transfer failed events in this example)
 */
void
ProcessEvent(VLServer svr)
{
    VLEvent ev;

    if (vlCheckEvent(svr, VLAllEventsMask, &ev) == -1) return;

    switch (ev.reason)
    {
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
    vlDeregisterBuffer(svr, path, drn, buffer);
    vlDestroyBuffer(svr, buffer);
    vlDestroyPath(svr, path);
    vlCloseVideo(svr);
    exit(ret);
}
