/*
 * Files:         dsktovid_aoi.c 
 *
 * Usage:         dsktovid_aio [-m mem node number] [-v vid node number]
 *		  	       [-c framecount] -f filename
 *
 * Description:   Dsktovid_aio sends a file of raw SMPTE YUV 4:2:2 frames to 
 *		  video out. If run as root, non-degrading CPU priority will
 *		  be requested and process memory will be locked.
 *
 * Notes:	  Asynchronous I/O facilities are used.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <malloc.h>
#include <getopt.h>
#include <aio.h>
#include <ulocks.h>
#include <errno.h>
#include <assert.h>
#include <values.h>
#include <bstring.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/schedctl.h>

#include <vl/vl.h>
#include <vl/dev_mgv.h>

#ifndef MAXNAMLEN
#define MAXNAMLEN 1024
#endif

#define DMRBSIZE 30
#define AIORBSIZE DMRBSIZE+1

char *_progName;
VLServer svr = NULL;
VLPath MEMtoVIDPath = NULL;
VLNode src = NULL, drn = NULL;
VLBuffer buf = NULL;
off_t transferSize;
off_t fileloc = 0;
char infilename[MAXNAMLEN];
int frames_ready;
int maxframes = MAXINT;

void process_event(VLServer vlSvr, void *junk);
void do_cleanup(int exit_type);
int dmrb_ready(VLServer vlSvr, void *junk);


#define USAGE \
"%s: [-c framecount] [-v videonode ] [-m memnode ] -f filename\n\
\t\t[-h]\n\
\t-f\tinput filename\n\
\t-c num\tnumber of frames to play\n\
\t-v\tvideo drain node number\n\
\t-m\tmemory source node number\n\
\t-h\tthis help message\n"

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

/*
 * An empty handler that just catches signls
 */
void
null_sighandler(void)
{
}

/*
 * AIO has completed. See if we can put the completed AIOs back into the
 * ring buffer. Note that we have to put the buffers back in in the same
 * order that we got them, otherwise the ring buffer may contain inconsistent
 * data.
 */
void
aio_done(void)
{
    int ret, err;

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
	* Check for end-of-file condition from read()
	*/
	ret = aio_return(&aiorb.aioinfo[aiorb.aiotail].aiocbp);
	if (ret == 0) {
	    aiorb.aioeof = 1;
	    break;
	}

	if (ret < 0) {
	    printf("Read error! err = %d\n", err);
	}

       /*
	* Everything OK; put data in the dmrb and increament aiotail
	*/
	vlPutValid(svr, buf);
	frames_ready++;

 	aiorb.aiotail++;
	if (aiorb.aiotail == AIORBSIZE) aiorb.aiotail = 0;
    }
}

void
usage(void)
{
    fprintf(stderr, USAGE, _progName);
}

main(int argc, char **argv)
{
    VLControlValue val;
    int ret;
    int c, i;
    int infile;
    int dmrbsize = DMRBSIZE;
    int framenum = 0;
    int voutnode = VL_ANY;
    int memsrcnode = VL_ANY;
    struct sigaction sigact;
    VLTransferDescriptor xferDesc;
    int bufferfd, vlfd;
    fd_set readfds, writefds, exceptfds, t_readfds, t_writefds, t_exceptfds;
    int nfds;

    _progName = argv[0];
    bzero(&aiorb, sizeof(aiorb));
    infilename[0] = 0;

   /* 
    * Parse command line:
    * -f <filename> name of file containing video data
    */
    while ((c = getopt(argc, argv, "f:v:m:c:h")) != EOF) {
	switch(c) {
	    case 'm':
		memsrcnode = atoi(optarg);
		break;

	    case 'v':
		voutnode = atoi(optarg);
	    	break;

	    case 'c':
		maxframes = atoi(optarg);
		break;
	    
	    case 'f':
		sprintf(infilename,"%s",optarg);
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

    if (strlen(infilename) == 0) {
	usage();
	exit(1);
    }

   /* 
    * Connect to the video daemon 
    */
    if (!(svr = vlOpenVideo(""))) {
	printf("couldn't open video\n");
	exit(1);
    }

   /* 
    * Create the memory to video path (just grab the first device 
    * that supports it) 
    */
    src = vlGetNode(svr, VL_SRC, VL_MEM, memsrcnode);
    drn = vlGetNode(svr, VL_DRN, VL_VIDEO, voutnode);
    MEMtoVIDPath = vlCreatePath(svr, VL_ANY, src, drn);

    if (MEMtoVIDPath == -1) {
	printf("%s: can't create path: %s\n", _progName, vlStrError(vlErrno));
	exit(1);
    }

   /* 
    * Set up the hardware for and define the usage of the path 
    */
    if (vlSetupPaths(svr, (VLPathList)&MEMtoVIDPath, 1, VL_SHARE, VL_SHARE)<0) {
	printf("%s: can't setup path: %s\n", _progName, vlStrError(vlErrno));
	exit(1);
    }

   /*
    * Set to 8-bit YUV 4:2:2, SMPTE format, then get the frame size
    */
    val.intVal = VL_PACKING_YVYU_422_8;
    if (vlSetControl(svr, MEMtoVIDPath, src, VL_PACKING, &val) < 0) {
	vlPerror("Unable to set packing");
	exit(1);
    }
    val.intVal = VL_FORMAT_SMPTE_YUV;
    if (vlSetControl(svr, MEMtoVIDPath, src, VL_FORMAT, &val) < 0) {
	vlPerror("Unable to set format");
	exit(1);
    }
    val.intVal = 4096;
    if (vlSetControl(svr, MEMtoVIDPath, src, VL_MGV_BUFFER_QUANTUM, &val) < 0) {
	vlPerror("Unable to set buffer quantum");
	exit(1);
    }
    transferSize = vlGetTransferSize(svr,MEMtoVIDPath);

    printf("transferSize = %d\n", transferSize);

    infile = open(infilename, O_RDONLY|O_DIRECT);
    if (infile<0) {
	perror(infilename);
	do_cleanup(1);
    }

   /*
    * Dump some direct I/O information
    */
    {
	struct dioattr dioinfo;

 	fcntl(infile, F_DIOINFO, &dioinfo);
	printf("Direct I/O Information:\n");
	printf("    alignment = %d\n", dioinfo.d_mem);
	printf("    miniosz = %d\n", dioinfo.d_miniosz);
  	printf("    maxiosz = %d\n", dioinfo.d_maxiosz);
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

   /*
    * Initialize for async I/O. Init the aiorb and register our completion
    * signal handler on SIGUSR1.
    */
    aio_sgi_init(NULL);
    for (i=0; i<AIORBSIZE; i++) {
	aiorb.aioinfo[i].aiocbp.aio_fildes = infile;
	aiorb.aioinfo[i].aiocbp.aio_nbytes = transferSize;
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
	do_cleanup(1);
    }

   /* 
    * Create a VL ring buffer for the data transfers.  
    */
    buf = vlCreateBuffer(svr, MEMtoVIDPath, src, dmrbsize);
    if (!buf) {
	vlPerror(_progName);
	exit(1);
    }
    if (vlRegisterBuffer(svr, MEMtoVIDPath, src, buf)) {
	vlPerror(argv[0]);
	vlDestroyBuffer(svr, buf);
	exit(1);
    }
    	 
   /* 
    * Register for preempted and transfer failed masks 
    */
    vlSelectEvents(svr, MEMtoVIDPath,  ( VLStreamPreemptedMask	|
					VLTransferFailedMask));
    /*
     * Immediate start;
     */
    xferDesc.delay = 0;
    xferDesc.trigger = VLTriggerImmediate;
    
   /*
    * Set up xfer descriptor for continuous transfer mode and begin the 
    * transfer now.  
    */
    xferDesc.count = 0;
    xferDesc.mode = VL_TRANSFER_MODE_CONTINUOUS;

   /*
    * Create select desriptors so we know when the dmrb is ready for more 
    * data, or when VL sends us an event.
    */
    bufferfd = vlBufferGetFd(buf);
    vlfd = vlGetFD(svr);

    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    FD_SET(vlfd, &readfds);
    FD_SET(bufferfd, &writefds);
    FD_SET(vlfd, &exceptfds);
    FD_SET(bufferfd, &exceptfds);
    if (vlfd > bufferfd) nfds = vlfd+1;
    else nfds = bufferfd+1;

   /*
    * Preroll
    */
    printf("Preroll...\n");
    frames_ready = 0;
    for (i=0; i < DMRBSIZE && i < maxframes; i++) {
	dmrb_ready(svr, NULL);
    }

    while (aiorb.aioeof == 0 && frames_ready < DMRBSIZE && frames_ready < maxframes) {
	sleep(1);
	aio_done();
    }

    printf("Starting transfer\n");
    if (vlBeginTransfer(svr, MEMtoVIDPath, 1, &xferDesc) < 0) {
	vlPerror("vlBeginTransfer");
	do_cleanup(1);
    }

   /*
    * This is, for all practical purposes, an event loop that exits after
    * all the data has been read in.
    */
    while (aiorb.aioeof == 0) {
	struct timeval tout;

	tout.tv_sec  = 1;
	tout.tv_usec = 0;

	t_readfds = readfds;
  	t_writefds = writefds;
	t_exceptfds = exceptfds;

	ret = select(nfds, &t_readfds, &t_writefds, &t_exceptfds, &tout);
	
       /*
	* If select returns an error, it's probably because one of the 
	* signals from aio came in. Otherwise there's an internal error,
	* so bail.
	*/
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

       /*
	* Check if the dmrb driver signaled that a dmrb was ready to be
	* filled.
	*/
	if (FD_ISSET(bufferfd, &t_writefds)) {
	    ret = dmrb_ready(svr, NULL);
	    if (ret != 0) break;
	}

       /*
	* Now see if we have a VL event
	*/
	if (FD_ISSET(vlfd, &t_readfds)) process_event(svr, NULL);

       /*
	* Now check for exceptions
	*/
	if (FD_ISSET(vlfd, &t_exceptfds)) {
	    fprintf(stderr, "Exception on VL connection\n");
	    do_cleanup(1);
	}
	if (FD_ISSET(bufferfd, &t_exceptfds)) {
	    fprintf(stderr, "Exception on ring buffer\n");
	    do_cleanup(1);
	}
    }

   /*
    * Release all realtime resources 
    */
    schedctl(NDPRI, getpid(), 0);
    plock(UNLOCK);

    printf("Frame sequence complete.  Hit return to exit.\n");
    while ( getc(stdin) != '\n');

   /* 
    * Cleanup and exit with no error 
    */
    do_cleanup(0);
}

int
dmrb_ready(VLServer svr, void *junk)
{
    VLInfoPtr info;
    static int fnum = 0;

    info = vlGetNextFree(svr, buf, transferSize);
    if (!info) return 0;

    printf("frame %d\n", fnum++);

   /* 
    * read in the video data  - assume raw YUV 
    */
    aiorb.aioinfo[aiorb.aiohead].aiocbp.aio_offset = fileloc;
    aiorb.aioinfo[aiorb.aiohead].aiocbp.aio_buf = 
    	vlGetActiveRegion(svr, buf, info);
    fileloc += transferSize;

    if (aio_read(&aiorb.aioinfo[aiorb.aiohead].aiocbp) != 0) {
	perror(infilename);
	fprintf(stderr, "%s: Unable to read the image data\n", _progName);
	return -1;
    }

    aiorb.aiohead++;
    if (aiorb.aiohead == AIORBSIZE) aiorb.aiohead = 0;
    assert(aiorb.aiohead != aiorb.aiotail);

    if (fnum < maxframes) return 0;
    return 1;
}

void
process_event(VLServer vlSvr, void *junk)
{
    VLEvent ev;
    int ret;

    if (vlCheckEvent(vlSvr, VLAllEventsMask, &ev) == -1) return;

    switch (ev.reason)
    {    
	case VLStreamPreempted:
	    fprintf(stderr, "%s: Stream Preempted by another Program\n",
		    _progName);
	    exit(1);
	break;

	case VLTransferFailed: 
	    fprintf(stderr, "%s: Transfer failed\n", _progName);
	    exit(1);
	break;
	
	default:
	break;
    }

}

void do_cleanup(int exit_type)
{
    /* End the data transfer */
    if (svr) {
        if (MEMtoVIDPath) {
	    vlEndTransfer(svr, MEMtoVIDPath);
    	    /* Disassociate the ring buffer from the path */
    	    if (buf) {
		/* Disassociate the ring buffer from the path */
		vlDeregisterBuffer(svr, MEMtoVIDPath, src, buf);
		/* Destroy the ring buffer, free its memory */
		vlDestroyBuffer(svr, buf);
	    }
    	    /* Destroy the path, free its memory */
    	    vlDestroyPath(svr, MEMtoVIDPath);
	}
        /* Disconnect from the daemon */
    	vlCloseVideo(svr);
    }
    
    exit(exit_type);
}
