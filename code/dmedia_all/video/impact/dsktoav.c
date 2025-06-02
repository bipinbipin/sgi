/*
 * Files:         dsktoav.c
 *
 * Usage:         dsktoav  [-m mem node number] [-v vid node number]
 *		  	   [-c frame count] [-l] [-d] -f filename
 *
 * Description:   Dsktoav plays back a dioav file captured with avtodsk to 
 *		  video out. If run as root, non-degrading CPU priority will
 *		  be requested and process memory will be locked.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <malloc.h>
#include <getopt.h>
#include <ulocks.h>
#include <errno.h>
#include <assert.h>
#include <values.h>
#include <unistd.h>
#include <fcntl.h>
#include <bstring.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/schedctl.h>

#include <dmedia/vl.h>
#include <dmedia/vl_mgv.h>
#include <dmedia/audio.h>

#include "dioav.h"

#ifndef MAXNAMLEN
#define MAXNAMLEN 1024
#endif

#define DMRBSIZE 8
#define YUV_BLACK_LONG_LONG 0x8010801080108010LL

char *_progName;
VLServer svr = NULL;
VLPath path = NULL;
VLNode src = NULL, drn = NULL;
VLBuffer buf = NULL;
int memnodenum = VL_ANY, vidnodenum = VL_ANY;
char infilename[MAXNAMLEN];
int maxframes = MAXINT;
long long frames_played = 0;
long long vidust_per_msc;
int verbose = 0;
int seq_lost_ok = 0;
int loopcount = 1;

ALport audport;
long long nsec_per_audframe;

dioav_header dioav;

void process_event(VLServer vlSvr);
void do_cleanup(int exit_type);
int dmrb_ready(VLServer vlSvr, void *junk);
int fd;

#define USAGE \
"%s: [-c framecount] [-v videonode] [-m memnode] [-l] [-d]\n\
\t -f filename [-h]\n\
\n\
\t-f\tinput filename\n\
\t-c num\tnumber of frames to capture\n\
\t-v num\tuse video drain node num\n\
\t-m num\tuse memory source node num\n\
\t-l\tloop at end of sequence\n\
\t-d\tenable verbose (diagnostic) synchronization messages\n\
\t-h\tthis help message\n"

void
usage(void)
{
    fprintf(stderr, USAGE, _progName);
}

/*
 * Initialize the direct I/O parameters.
 */
int
dioinit(void)
{
    struct dioattr dioinfo;
    char *tbuf;

   /*
    * Read in the dio header using normal I/O
    */
    fd = open(infilename, O_RDONLY);
    if (fd < 0) {
	perror("Unable to open input file for reading");
	return -1;
    }
    if (read(fd, &dioav, sizeof(dioav)) != sizeof(dioav)) {
	perror("Unable to read dioav header");
	return -1;
    }
    close(fd);

   /*
    * Now reopen the file using direct I/O
    */
    fd=open(infilename, O_RDONLY | O_DIRECT);
    if (fd < 0) {
	fd = open(infilename, O_RDONLY);
	if (fd < 0) {
	    perror("Unable to open input file for reading");
	    return -1;
	}
	fprintf(stderr, "Warning: Unable to open file using direct I/O\n");
    }

   /*
    * Dump some direct I/O information
    */
    if (fcntl(fd, F_DIOINFO, &dioinfo) >= 0) {
	printf("Direct I/O Information:\n");
	printf("    alignment = %d\n", dioinfo.d_mem);
	printf("    miniosz = %d\n", dioinfo.d_miniosz);
	printf("    maxiosz = %d\n", dioinfo.d_maxiosz);

       /*
	* Ensure that the playback block size can handle the block
	* size the file was written with.
	*/
	if (dioav.dio_size % dioinfo.d_miniosz  != 0 ||
	    dioav.dio_size < dioinfo.d_miniosz) {
	    fprintf(stderr, "Direct I/O block size (%d) is incompatible with capture device (%d)\n", dioinfo.d_miniosz, dioav.dio_size);
	}
    }
    else {
        fprintf(stderr, "No direct I/O information available\n");
    }

    return 0;
}

int
vidinit(void)
{
    VLControlValue val;
    int vidTransferSize;

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
    src = vlGetNode(svr, VL_SRC, VL_MEM, memnodenum);
    drn = vlGetNode(svr, VL_DRN, VL_VIDEO, vidnodenum);
    path = vlCreatePath(svr, VL_ANY, src, drn);
    if (path == -1) {
	vlPerror("Video: Unable to create path");
	return -1;
    }

   /* 
    * Set up the hardware for and define the usage of the path 
    */
    if (vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE)<0) {
	vlPerror("Video: Unable to setup path");
	return -1;
    }

   /*
    * Set to 8-bit YUV 4:2:2, SMPTE format, then get the frame size
    */
    val.intVal = dioav.video_packing;
    if (vlSetControl(svr, path, src, VL_PACKING, &val) < 0) {
	vlPerror("Video: Can't set packing");
	return -1;
    }

    val.intVal = dioav.video_format;
    if (vlSetControl(svr, path, src, VL_FORMAT, &val) < 0) {
	vlPerror("Video: Can't set format");
	return -1;
    }

    val.intVal = dioav.video_captype;
    if (vlSetControl(svr, path, src, VL_CAP_TYPE, &val) < 0) {
	vlPerror("Video: Can't set capture type");
	return -1;
    }

    val.fractVal.numerator = dioav.chunk_rate;
    val.fractVal.denominator = 1;
    if (vlSetControl(svr, path, src, VL_RATE, &val) < 0) {
	vlPerror("Video: Can't set rate");
	fprintf(stderr, "Capture rate: %d\n", dioav.chunk_rate);
	return -1;
    }

    val.xyVal.x = dioav.video_width;
    val.xyVal.y = dioav.video_height;
    if (vlSetControl(svr, path, src, VL_SIZE, &val) < 0) {
	vlPerror("Video: Can't set size");
	return -1;
    }

    vidTransferSize = vlGetTransferSize(svr,path);
    if (vidTransferSize != dioav.video_trans_size) {
	fprintf(stderr, "Video: frame size in header doesn't match VL\n");
	return -1;
    }

   /*
    * Register for preempted and transfer failed masks
    */
    vlSelectEvents(svr, path,  ( VLStreamPreemptedMask  |
					VLTransferFailedMask |
					VLSequenceLostMask |
					VLTransferCompleteMask));

    return 0;
}

int
audinit(void)
{
    ALconfig portconfig;
    long pvbuf[2];

   /*
    * Set the global IRIS Audio Processor rate
    */
    portconfig = ALnewconfig();
    pvbuf[0] = AL_OUTPUT_RATE;
    pvbuf[1] = dioav.audio_frame_rate;
    ALsetparams(AL_DEFAULT_DEVICE, pvbuf, 2);
    ALsetchannels(portconfig, dioav.audio_num_channels);
    ALsetwidth(portconfig, dioav.audio_sample_width);
    ALsetsampfmt(portconfig, dioav.audio_sample_format);
    ALsetqueuesize(portconfig, dioav.audio_frame_rate * 
	dioav.audio_num_channels);

    audport = ALopenport("avrecord", "w", portconfig);
    nsec_per_audframe = NSEC_PER_SEC  / dioav.audio_frame_rate;

    if (audport == NULL) {
	fprintf(stderr, "Unable to open audio port\n");
	return -1;
    }

    return 0;
}

/*
 * Initialize the audio and video devices.
 *
 * Returns 0 on success, -1 on failure.
 */
int
avinit(void)
{
    char *tbuf;
    VLControlValue val;

   /*
    * Initialize the file for direct I/O
    */
    if (dioinit() == -1) {
	fprintf(stderr, "Unable to initialize for direct I/O\n");
	return -1;
    }

   /*
    * Initialize video
    */
    if (vidinit() == -1) {
	fprintf(stderr, "Unable to configure video\n");
	return -1;
    }

   /*
    * Initialize audio
    */
    if (audinit() == -1) {
	fprintf(stderr, "Unable to configure audio\n");
	return -1;
    }

    val.intVal = dioav.chunk_size;
    if (vlSetControl(svr, path, src, VL_MGV_BUFFER_QUANTUM, &val) < 0) {
	vlPerror("Unable to set chunk size");
	return -1;
    }

   /*
    * Create a VL ring buffer for the data transfers. This buffer will
    * actually hold both audio and video data, which is why the buffer
    * quantum was made large enough to hold both.
    */
    buf = vlCreateBuffer(svr, path, src, DMRBSIZE);
    if (!buf) {
	vlPerror("Unable to create VL buffer");
	return -1;
    }

   /*
    * Make the buffer non-cacheable since we are doing dma into and out of
    * the ring buffer. If the data will be processed, then the buffer should
    * be made cacheable (VL_BUFFER_ADVISE_ACCESS), which is the default mode.
    */
    vlBufferAdvise(buf, VL_BUFFER_ADVISE_NOACCESS);

    if (vlRegisterBuffer(svr, path, src, buf)) {
	vlPerror("Unable to register buffer");
	return -1;
    }

   /*
    * Determine UST per MSC for video. We can ask the VL for this.
    */
    vidust_per_msc = vlGetUSTPerMSC(svr, path, src);
    if (vidust_per_msc < 0) {
	vlPerror("Unable to get video ust per msc");
	return -1;
    }

    return 0;
}

int
sync_audio_and_video(void)
{
    short samps[32768];

    VLInfoPtr info;
    long long *dataPtr;
    USTMSCpair vidpair;
    unsigned long long vidfrontier_msc;
    unsigned long long audmsc, audust, audfrontier_msc;
    unsigned long long ofrontier_msc = 0;
    long long nsamps;
    int i, j;

   /*
    * Prepare some video black and audio silence to be used while synchronizing
    * audio and video. For audio we use 0.10s of silence. For video we need 
    * 0.10s plus an additional 0.10s or so to account for some delay between 
    * the time we enqueue audio and the time it starts playing.
    *
    * At the end of the stuffing we want to ensure that there is more video
    * data enqueued than there is audio data. So the frontier UST for video 
    * will be greater than the frontier UST for audio. When we try to 
    * synchronize, we will determine exactly how many additional audio 
    * samples are required to even out the frontier USTs for audio and video.
    */
    bzero(samps, sizeof(short)*32768);
    for (i = 0; i < dioav.chunk_rate / 10 * 2 + 1; i++) {
	if (info = vlGetNextFree(svr, buf, dioav.chunk_size)) {
	    dataPtr = vlGetActiveRegion(svr, buf, info);
	    for (j = 0; j < dioav.video_trans_size/sizeof(long long); j++) {
		*dataPtr++ = YUV_BLACK_LONG_LONG;
	    } 
	}
	else {
	    fprintf(stderr, "Sync: video  preroll failure!\n");
	    exit(1);
	}
    }

   /*
    * Now fill the AL buffer and write give video black to the VL
    */
    nsamps = dioav.audio_frame_rate * dioav.audio_num_channels / 10 ;
    while (nsamps) {
	ALwritesamps(audport, samps, min(nsamps, 32768));
	nsamps -= min(nsamps, 32768);
    }

    for (i = 0; i < dioav.chunk_rate / 10 * 2 + 1; i++) {
	vlPutValid(svr, buf);
    }

   /*
    * Determine a sync point for audio and video. First get the relevant
    * frontier and ust/msc pair infromation from each.
    */
    if (vlGetUSTMSCPair(svr, path, drn, VL_ANY, src, &vidpair) < 0) {
	vlPerror("vlGetUSTMSCPair");
	exit(1);
    }
    sginap(1);
    vidfrontier_msc = vlGetFrontierMSC(svr, path, src);
    if (vidfrontier_msc == -1) {
	vlPerror("vlGetFrontierMSC");
	exit(1);
    }

    if (verbose) {
    	printf("Video pair: ust = %lld, msc = %lld, frontier = %lld\n", 
	    vidpair.ust, vidpair.msc, vidfrontier_msc);
    }


    if (ALgetframetime(audport, &audmsc, &audust) < 0) {
        perror("ALgetframetime:");
    }
    if (verbose) {
    	printf("Audio pair: ust = %lld, msc = %lld\n", audust, audmsc);
    }
   
   /*
    * Now determine the frontier ust for video, then the target msc for
    * audio.
    */
    if (vidfrontier_msc > vidpair.msc)
    	vidpair.ust += (vidfrontier_msc - vidpair.msc) * vidust_per_msc;
    else 
	vidpair.ust -= (vidpair.msc - vidfrontier_msc) * vidust_per_msc;

    if (vidpair.ust > audust)
    	audmsc += ((vidpair.ust - audust) *  dioav.audio_frame_rate) / 
	    NSEC_PER_SEC;
    else
	audmsc -= ((audust - vidpair.ust) *  dioav.audio_frame_rate) / 
	    NSEC_PER_SEC;

    if (ALgetframenumber(audport, &audfrontier_msc) < 0) {
        perror("ALgetframenumber");
    }
   
    if (verbose) {
    	printf("Audio frontier: %lld\n", audfrontier_msc);
    }

   /*
    * Finally write to audio the required number of samples to sync audio
    * with video.
    */
    nsamps = (audmsc - audfrontier_msc) * dioav.audio_num_channels;

    if (verbose) {
    	printf("Stuffing %d additional audio samples = %lld\n", nsamps);
    }

    while (nsamps > 0) {
	ALwritesamps(audport, samps, min(nsamps, 32768));
	nsamps -= min(nsamps, 32768);
    }

   /*
    * And lastly sanity check the frontier msc, just in case audio underran 
    * between the time that we prerolled it and the time we stuffed it. 
    */
    if (ALgetframenumber(audport, &audfrontier_msc) < 0) {
        perror("ALgetframenumber");
    }
    if (((audfrontier_msc > audmsc) && (audfrontier_msc - audmsc > 4))  ||
	((audmsc > audfrontier_msc) && (audmsc - audfrontier_msc > 4))) {
	printf("FAIL: audfrontier_msc = %lld, target =%lld\n", audfrontier_msc,
	    audmsc);
	exit(1);
    }
}

int
check_audio_video_sync(void)
{
    USTMSCpair vidpair;
    unsigned long long audust, audmsc, audfrontier_msc;
    unsigned long long vidfrontier_msc;
    long long audfrontier_ust, vidfrontier_ust;

   /*
    * Determine a sync point for audio and video
    */
    if (vlGetUSTMSCPair(svr, path, drn, VL_ANY, src, &vidpair) < 0) {
	vlPerror("vlGetUSTMSCPair");
	exit(1);
    }

    if (ALgetframetime(audport, &audmsc, &audust) < 0) {
        perror("ALgetframetime:");
    }

    vidfrontier_msc = vlGetFrontierMSC(svr, path, src);	

    if (vidfrontier_msc == -1) {
	vlPerror("vlGetFrontierMSC");
	exit(1);
    }

    if (ALgetframenumber(audport, &audfrontier_msc) < 0) {
        perror("ALgetframenumber");
    }

   /*
    * Determine the frontier ust for video and audio
    */
    vidfrontier_ust = (vidfrontier_msc - vidpair.msc) * vidust_per_msc + 
	vidpair.ust;
    audfrontier_ust = (audfrontier_msc - audmsc) * NSEC_PER_SEC/
	dioav.audio_frame_rate + audust;
    
#ifdef DEBUG
    printf("vid: %lld, %lld (%lld)  aud: %lld, %lld (%lld)\n", 
	vidpair.ust, vidpair.msc, vidfrontier_msc, audust, audmsc,
	audfrontier_msc);
#endif

    if (verbose) {
    	printf("Frontier %lld diff: %lld\n", vidfrontier_msc, 
	    audfrontier_ust - vidfrontier_ust);
    }
}

/*
 * Wait for the audio and video buffers to completely drain. This ensures
 * that the initial conditions for the synchronization function are met. 
 * (the assumption is that the audio and video are both empty and that the
 * frontiers are free runing on each).
 */
void
wait_eof(void)
{
    VLEvent ev;

    while ((ALgetfilled(audport) > 0) || (vlGetFilled(svr, path, buf) > 0)) {
	sginap(1);
    }

   /*
    * Now clean out all transfer complete events until we hit a sequence
    * lost event.
    */
    do {
	if (vlCheckEvent(svr, VLAllEventsMask, &ev) == -1) {
	    sginap(1);
	    continue;
   	}
	switch (ev.reason) {
	    case VLTransferComplete:
		break;

	    case VLSequenceLost:
		break;

	    case VLTransferFailed:
		fprintf(stderr, "Transfer failed!\n");
		exit(1);
		break;

	    case VLStreamPreempted:
		fprintf(stderr, "Stream Preempted!\n");
		exit(1);
		break;
		
	    default:
		fprintf(stderr, "Received unexpected event %d\n", ev.reason);
		break;
	}
    } while (ev.reason != VLSequenceLost);

   /*
    * If we are in loop mode, the video daemon will start sending back
    * sequence lost events because we are starving the video daemon between
    * loops. Tell the event reporting routine that this is OK since we
    * deliberately underran the buffers. When a TransferComplete event is
    * received, i.e. when the first frame of video is transferred, it will
    * be reset to 0.
    */
    seq_lost_ok = 1;
}

/*
 * Play from the beginning of the file until the requested number of frames
 * are played, end-of-file is reached, or an error occurs.
 *
 * On exit, a return of 0 indicated the requested frames were played or 
 * eof was reached. A return of -1 indicate an error has occurred.
 */
int
play(void)
{
    fd_set readfds, writefds, exceptfds, t_readfds, t_writefds, t_exceptfds;
    int bufferfd, vlfd;
    int nfds;
    int ret;

    printf("Playing loop %d\n", loopcount++);

    lseek(fd, dioav.dio_size, SEEK_SET);

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
    * This is, for all practical purposes, an event loop that exits after
    * all the data has been read in.
    */
    while (1) {
	t_readfds = readfds;
  	t_writefds = writefds;
	t_exceptfds = exceptfds;

	ret = select(nfds, &t_readfds, &t_writefds, &t_exceptfds, NULL);
   	if (ret < 0) break;
	
       /*
	* Check if the dmrb driver signaled that a dmrb was ready to be
	* filled.
	*/
	if (FD_ISSET(bufferfd, &t_writefds)) {
	    ret = dmrb_ready(svr, NULL);
	    /* Error during read, or eof */
	    if (ret <= 0) {
		break;
	    }
	}

       /*
	* Now see if we have a VL event
	*/
	process_event(svr);

	if (FD_ISSET(vlfd, &t_exceptfds)) {
	    fprintf(stderr, "Exceptional condition on VL connection\n");
	    ret = -1;
	    break;
	}
	if (FD_ISSET(bufferfd, &t_exceptfds)) {
	    fprintf(stderr, "Exception condition on ring buffer\n");
	    ret = -1;
	    break;
	}
    }

    return ret;
}
   
int
main(int argc, char **argv)
{
    int ret, c;
    int loop = 0;

    _progName = argv[0];
    infilename[0] = 0;

   /* 
    * Parse command line:
    * -f <filename> name of file containing video data
    */
    while ((c = getopt(argc, argv, "f:m:v:c:hld")) != EOF) {
	switch(c) {
	    case 'c':
		maxframes = atoi(optarg);
		break;

	    case 'm':
		memnodenum = atoi(optarg);
		break;

	    case 'v':
		vidnodenum = atoi(optarg);
	    	break;
	    
	    case 'f':
		sprintf(infilename,"%s",optarg);
	    	break;

	    case 'l':
		loop = 1;
		break;

	    case 'd':
		verbose = 1;
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
    * Initialize audio
    */
    if (avinit() == -1) {
	fprintf(stderr, "Unable to configure audio\n");
	exit(1);
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
    * Start the transfer in continuous, immediate start mode
    */
    if (verbose) {
    	printf("Starting transfer\n");
    }

    if (vlBeginTransfer(svr, path, 0, NULL) < 0) {
	vlPerror("vlBeginTransfer");
	do_cleanup(1);
    }

    do {
    	sync_audio_and_video();

    	if (play() < 0) loop = 0;

    	wait_eof();

    } while (loop);

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

/*
 * Process one frame (audio and video). 
 *
 * On exit, 1 is returned on success.
 *          0 is returned on end of file
 *         -1 is returned on error
 */
int
dmrb_ready(VLServer svr, void *junk)
{
    VLInfoPtr info;
    int ret, j;
    char *dataPtr;
    int run = 30;
    audio_header *audioheader;
    char *audiobuffer;
    int read_size = dioav.chunk_size;

    while ((--run) &&
	(frames_played < maxframes) &&
	(info = vlGetNextFree(svr, buf, dioav.chunk_size))) {

	dataPtr  = vlGetActiveRegion(svr, buf, info);

    	ret = read(fd, dataPtr, read_size);
	if (ret == -1) {
	    fprintf(stderr, "Error reading image data\n");
	    vlPutValid(svr, buf);
	    return -1;
	}

       /*
	* End -of-file
	*/
	if (ret < dioav.chunk_size) {
	    long long *tmp = (long long *)dataPtr;
	    for (j = 0; j < dioav.video_trans_size/sizeof(long long); j++) {
		*tmp++ = YUV_BLACK_LONG_LONG;
	    } 
	    vlPutValid(svr, buf);
	    return 0;
	}

 	audioheader = (audio_header *)(dataPtr + dioav.video_trans_size);
   	audiobuffer = (char *)(dataPtr + dioav.video_trans_size + 
	    sizeof(audio_header));
	if (ALwritesamps(audport, audiobuffer,
	    audioheader->frame_count * dioav.audio_num_channels) < 0) {
	    perror("ALwritesamps failed");
	    return -1;
	}

    	vlPutValid(svr, buf);

	frames_played++;
    	check_audio_video_sync();
    }

    if (frames_played == maxframes) return 0;

    return 1;
}

/*
 * Process any events sent by the video library. 
 */
void
process_event(VLServer vlSvr)
{
    VLEvent ev;
    int ret;

    while (vlCheckEvent(vlSvr, VLAllEventsMask, &ev) != -1) {

    	switch (ev.reason) {    

	    case VLTransferComplete:
		if (seq_lost_ok) seq_lost_ok = 0;
		break;

	    case VLStreamPreempted:
	    	fprintf(stderr, "%s: Stream Preempted by another Program\n",
		    _progName);
	    	exit(1);
	    	break;

	    case VLTransferFailed: 
	    	fprintf(stderr, "%s: Transfer failed\n", _progName);
	    	exit(1);
	    	break;

	    case VLSequenceLost:
	    	if (seq_lost_ok) break;
	    	fprintf(stderr, "%s: Sequence lost\n", _progName);
	    	exit(1);
	    	break;
	
	    default:
	    	printf("Unknown event %d\n", ev.reason);
	    	break;
    	}
    }
}

void do_cleanup(int exit_type)
{
    /* End the data transfer */
    if (svr) {
        if (path) {
	    vlEndTransfer(svr, path);
    	    /* Disassociate the ring buffer from the path */
    	    if (buf) {
		/* Disassociate the ring buffer from the path */
		vlDeregisterBuffer(svr, path, src, buf);
		/* Destroy the ring buffer, free its memory */
		vlDestroyBuffer(svr, buf);
	    }
    	    /* Destroy the path, free its memory */
    	    vlDestroyPath(svr, path);
	}
        /* Disconnect from the daemon */
    	vlCloseVideo(svr);
    }
    
    exit(exit_type);
}
