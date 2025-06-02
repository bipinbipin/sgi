/*
 * Files:         vidtodsk_aoi.c 
 *
 * Usage:         vidtodsk_aoi [-m mem node number] [-v vid node number]
 *                             [-c frame count] -f filename
 *
 * Description:   Vidtodsk_aoi captures a file of raw SMPTE YUV 4:2:2 frames  
 *                from video in. If run as root, non-degrading CPU priority will
 *                be requested and process memory will be locked.
 *
 * Notes:	  Asynchronous I/O facilities are used.
 */

#include <errno.h>
#include <sys/lock.h>
#include <sys/schedctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>	/* writev() */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <getopt.h>
#include <bstring.h>
#include <values.h>
#include <fcntl.h>
#include <aio.h>
#include <assert.h>
#include <signal.h>

#include <dmedia/vl.h>
#include <vl/dev_mgv.h>


char *_progName;

#ifndef MAXNAMLEN
#define MAXNAMLEN 1024
#endif
char outfilename[MAXNAMLEN - 5] = "";
int imageCount = MAXINT;
VLServer svr;
VLBuffer buffer;
int frames = 0;
VLPath path;
VLNode src, drn, viddrn;
off_t frameSize;
int fd;
int aiodone_count = 0;
int aioreq_count = 0;

void docleanup(void);
void usage(void);
void dmrb_ready(VLServer svr);
void ProcessEvent(VLServer svr);
void aio_done(void);

#define DMBUFSIZE 30
#define AIORBSIZE DMBUFSIZE+1

/*
 * AIO ring buffer: a ring buffer of AIO reqests. aiohead is the index of the
 * next available request block. aiotail is the index of the index of the
 * earliest outstanding request. The size of the aiorb *MUST* be larger than
 * the size of the dmrb.
 */
struct {
    struct {
	aiocb_t aiocbp;
    } aioinfo[AIORBSIZE];
    int aiohead;
    int aiotail;
    int aioeof;
} aiorb;

#define USAGE \
"%s: [-c framecount] [-v videonode ] [-m memnode ] -f filename\n\
\t\t[-h]\n\
\t-f\toutput filename\n\
\t-c num\tnumber of frames to capture\n\
\t-v\tvideo source node number\n\
\t-m\tmemory drain node number\n\
\t-h\tthis help message\n"

void
usage(void)
{
    fprintf(stderr, USAGE, _progName);
}

/*
 * An empty handler that just catches signls
 */
void
null_sighandler(void)
{
}

int
main(int argc, char **argv)
{
    VLControlValue val;
    int i, c, ret;
    int vin = VL_ANY;
    int memnode = VL_ANY;
    int nfds;
    fd_set readfds, exceptfds, t_readfds, t_exceptfds;
    int vlfd, bufferfd;
    struct sigaction sigact;

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
		vin = atoi(optarg);
	     	break;

	    case 'm':
		memnode = atoi(optarg);
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

    /* Set up a drain node in memory */
    drn = vlGetNode(svr, VL_DRN, VL_MEM, memnode);

    /* And another drain to video out */
    viddrn = vlGetNode(svr, VL_DRN, VL_VIDEO, VL_ANY);

    /* Set up a source node on the user specified source */
    src = vlGetNode(svr, VL_SRC, VL_VIDEO, vin);

    /*
     * If the user didn't specify a device create a path using the first
     * device that will support it
     */
    if ((path = vlCreatePath(svr, VL_ANY, src, drn)) < 0) {
	vlPerror(_progName);
	exit(1);
    }

    if (vlAddNode(svr, path, viddrn) == -1) {
	vlPerror(_progName);
	exit(1);
    }

    /* Set up the hardware for and define the usage of the path */
    if (vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE) < 0) {
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
    * Set the buffer quantum to 4096 bytes. This satisfies the (file) device
    * block size requirements for raw or direct I/O, as well as the Ciprico
    * 6900 array stripe size
    */
    val.intVal = 4096;
    vlSetControl(svr, path, drn, VL_MGV_BUFFER_QUANTUM, &val);

    /*
     * Specify what path-related events we want to receive.
     * In this example we want transfer complete and transfer failed
     * events only.
     */
    vlSelectEvents(svr, path, VLTransferFailedMask);

    val.intVal = VL_PACKING_YVYU_422_8;
    if (vlSetControl(svr, path, drn, VL_PACKING, &val) < 0) {
        vlPerror("vlSetControl(VL_PACKING)");
        exit(1);
    }

    frameSize = vlGetTransferSize(svr, path);
    printf("Frame size = %d\n", frameSize);

    /* Create and register a buffer  */
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
    * Now open the output file, preferably with direct I/O. We also specify
    * O_APPEND so that in the aio structures we don't have to specify the
    * file position. The position member is a long (32-bit), so we would get
    * stuck on 2GB filesize.
    */
    fd = open(outfilename, O_WRONLY|O_CREAT|O_TRUNC|O_DIRECT, 0666);
    if (fd < 0) fd = open(outfilename, O_WRONLY|O_CREAT|O_TRUNC, 0666);

    if (fd < 0) {
	perror(outfilename);
	exit(1);
    }

   /*
    * Initialize for async I/O. Init the aiorb and register our completion
    * signal handler on SIGUSR1.
    */
    aio_sgi_init(NULL);
    for (i=0; i<AIORBSIZE; i++) {
        aiorb.aioinfo[i].aiocbp.aio_fildes = fd;
        aiorb.aioinfo[i].aiocbp.aio_nbytes = frameSize;
        aiorb.aioinfo[i].aiocbp.aio_reqprio = 0;
	aiorb.aioinfo[i].aiocbp.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
        aiorb.aioinfo[i].aiocbp.aio_sigevent.sigev_signo = SIGUSR1;
    }
    aiorb.aiohead = aiorb.aiotail = aiorb.aiotail = 0;

    sigemptyset(&sigact.sa_mask);
    sigact.sa_handler = null_sighandler;
    sigact.sa_flags = 0;
    if (sigaction(SIGUSR1, &sigact, NULL) == -1) {
        perror(_progName);
        fprintf(stderr, "Unable to install signal handler for aio done cb ");
        docleanup();
	exit(1);
    }

   /*
    * Create select desriptors so we know when the dmrb is ready for more
    * data, or when VL sends us an event.
    */
    bufferfd = vlBufferGetFd(buffer);
    vlfd = vlGetFD(svr);

    FD_ZERO(&readfds);
    FD_ZERO(&exceptfds);
    FD_SET(vlfd, &readfds);
    FD_SET(bufferfd, &readfds);
    FD_SET(vlfd, &exceptfds);
    FD_SET(bufferfd, &exceptfds);
    if (vlfd > bufferfd) nfds = vlfd+1;
    else nfds = bufferfd+1;

    /* Begin the data transfer */
    if (vlBeginTransfer(svr, path, 0, NULL)) {
	vlPerror(_progName);
	exit(1);
    }

    /* Loop until all requested frames are grabbed */
    while (aioreq_count < imageCount) {
	t_readfds = readfds;
	t_exceptfds = exceptfds;

	ret = select(nfds, &t_readfds, NULL, &t_exceptfds, NULL);

	if (ret == -1) {
	    if (errno == EINTR) {
		aio_done();
		continue;
	    }
	    else {
	   	perror("Error in event loop!");
		break;
	    }
	}
	else if (FD_ISSET(bufferfd, &t_readfds)) dmrb_ready(svr);
	else if (FD_ISSET(vlfd, &t_readfds)) ProcessEvent(svr);

	if (FD_ISSET(bufferfd, &t_exceptfds)) {
	    fprintf(stderr, "Exceptional condition on ring buffer\n");
	    docleanup();
	    exit(1);
	}
	if (FD_ISSET(vlfd, &t_exceptfds)) {
	    fprintf(stderr, "Exceptional condition on VL connection\n");
	    docleanup();
	    exit(1);
	}
    }

    while (aiodone_count < imageCount && aiorb.aioeof == 0) {
	aio_done();
	sleep(1);
    }

    docleanup();

    return 0;
}

void
dmrb_ready(VLServer svr)
{
    VLInfoPtr info;
    static off_t fileloc = 0;
    char *dataPtr;

    while ((aioreq_count < imageCount) && 
	(info = vlGetNextValid(svr, buffer))) {

    	/* Get a pointer to the frame(s) */
	dataPtr  = vlGetActiveRegion(svr, buffer, info);
	aiorb.aioinfo[aiorb.aiohead].aiocbp.aio_buf = dataPtr;
	aiorb.aioinfo[aiorb.aiohead].aiocbp.aio_offset = fileloc;
	fileloc += frameSize;

	if (aio_write(&aiorb.aioinfo[aiorb.aiohead].aiocbp) != 0) {
	    perror(outfilename);
	    fprintf(stderr, "%s: Unable to read the image data\n", _progName);
	    docleanup();
	    exit(1);
	}

	aiorb.aiohead++;
	if (aiorb.aiohead == AIORBSIZE) aiorb.aiohead = 0;
	assert(aiorb.aiohead != aiorb.aiotail);
	aioreq_count++;
    }
}

void
aio_done(void)
{
    int ret, err;
    int fcount = 0;

   /*
    * Now check to see if the request at aiotail has completed. We must
    * put things on the dmrb in order, so if something other than tail
    * has completed, wait for another signal.
    */
    while (aiorb.aiotail != aiorb.aiohead) {
        err = aio_error(&aiorb.aioinfo[aiorb.aiotail].aiocbp);
        if (err == EINPROGRESS) {
           /*
            * If request at tail is still in progress, bail.
            */
            break;
        }
        else if (err == -1) break;

       /*
        * Check for write error
        */
        if (ret < 0) {
            printf("Write error! err = %d\n", err);
	    aiorb.aioeof = 1;
        }

       /*
        * Everything OK; put buffer back in the dmrb and increment aiotail
        */
	fcount++;
	aiodone_count++;
	vlPutFree(svr, buffer);

        aiorb.aiotail++;
        if (aiorb.aiotail == AIORBSIZE) aiorb.aiotail = 0;
    }

    printf("frame %d\n", aiodone_count);
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
	    docleanup();
	    exit(1);
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
docleanup(void)
{
    vlEndTransfer(svr, path);
    vlDeregisterBuffer(svr, path, drn, buffer);
    vlDestroyBuffer(svr, buffer);
    vlDestroyPath(svr, path);
    vlCloseVideo(svr);
}
