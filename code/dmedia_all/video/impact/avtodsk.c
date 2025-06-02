/*
 * Files:         avtodsk.c
 *
 * Usage:         avtodsk  [-m mem node number] [-v vid node number]
 *                         [-c frame count] -f filename
 *
 * Description:   avtodsk captures an interleaved audio/video file using a
 *                dioav format. See dioav.h for a description of the file
 *                layout. If run as root, non-degrading CPU priority will
 *                be requested and process memory will be locked.
 *
 */

#include <sys/lock.h>
#include <sys/schedctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>	/* writev() */
#include <sys/dmcommon.h>
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
#include <errno.h>

#include <dmedia/vl.h>
#include <dmedia/vl_mgv.h>
#include <dmedia/dm_audio.h>
#include <dmedia/audio.h>

#include "dioav.h"

char *_progName;

char outfilename[MAXPATHLEN] = "";
int imageCount = MAXINT;
int vinnodenum = VL_ANY, memnodenum = VL_ANY;
VLServer svr;
VLBuffer buffer;
VLPath path;
VLNode src, drn;
int fd;
int frames_captured = 0;
int video_rate = -1;
int video_zoom_numerator = 1;
int video_zoom_denominator = 1;
dioav_header dioav;
int verbose = 0;
long long video_ust_skew = 0;
int packing = VL_PACKING_YVYU_422_8; 

ALport audport;
long long nsec_per_audframe;

#define DMRBSIZE 30

#define USAGE \
"%s: [-c frame count ] [-v videonode ] [-m memnode ] [-s video skew] \
     [-r rate] [-z numn/denom] -f filename\n\
\t\t[-h]\n\
\t-f\toutput filename\n\
\t-c\tnumber of frames to capture\n\
\t-v\tvideo source node number\n\
\t-m\tmemory drain node number\n\
\t-r\tvideo frame rate\n\
\t-s\ttiming difference of video with respect to audio (nanoseconds)\n\
\t-z\tamount to scale the image by, expressed as numerator/denominator\n\
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
  
    fd=open(outfilename, O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0666);
    if (fd < 0) {
	perror("Unable to open output file for writing");
	return -1;
    }

   /*
    * Dump some direct I/O information
    */
    if (fcntl(fd, F_DIOINFO, &dioinfo) >= 0) {
    	printf("Direct I/O Information:\n");
    	printf("    alignment = %d\n", dioinfo.d_mem);
    	printf("    miniosz = %d\n", dioinfo.d_miniosz);
    	printf("    maxiosz = %d\n", dioinfo.d_maxiosz);
	dioav.dio_size = dioinfo.d_miniosz;
    }
    else {
	fprintf(stderr, 
	    "No direct I/O information available, using 4k blocks\n");
	dioav.dio_size = 4096;
    }

   /*
    * Use a minimum of 4k since this file may be copied to a device with
    * a 4k IO size. (Well, it could be copied to a larger one but these
    * are rare and we have to draw a line somewhere...)
    */
    if (dioav.dio_size < 4096) dioav.dio_size = 4096;

    return 0;
}

/*
 * Initialize vide
 *
 * Returns 0 on success, -1 on error
 */
int
vidinit(void) 
{
    VLControlValue val;

   /* 
    * Connect to the video daemon 
    */
    if (!(svr = vlOpenVideo(""))) {
	vlPerror("Video: couldn't open video");
	return -1;
    }

   /* 
    * Create the video to memory path (just grab the first device 
    * that supports it) 
    */
    src = vlGetNode(svr, VL_SRC, VL_VIDEO, vinnodenum);
    if (src == NULL) {
	vlPerror("Video: Unable to get video source");
	return -1;
    }

    drn = vlGetNode(svr, VL_DRN, VL_MEM, memnodenum);
    if (drn == NULL)  {
	vlPerror("Video: Unable to get memory drain");
	return -1;
    }

    path = vlCreatePath(svr, VL_ANY, src, drn);
    if (path == -1) {
	vlPerror("Video: unable to create path");
	return -1;
    }

   /* 
    * Set up the hardware for and define the usage of the path 
    */
    if (vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE)<0) {
	vlPerror("Video: can't setup path");
	return -1;
    }

   /*
    * Set to 8-bit YUV 4:2:2, SMPTE format
    */
    val.intVal = packing;
    if (vlSetControl(svr, path, drn, VL_PACKING, &val) < 0) {
	vlPerror("Video: Can't set packing");	
	return -1;
    }

    val.intVal = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
    if (vlSetControl(svr, path, drn, VL_FORMAT, &val) < 0) {
	vlPerror("Video: Can't set format to SMPTE YUV");
	return -1;
    }

    if (video_rate != -1) {
	val.fractVal.numerator = video_rate;
	val.fractVal.denominator = 1;
    	if (vlSetControl(svr, path, drn, VL_RATE, &val) < 0) {
	    vlPerror("Video: Can't set requested rate");
	    return -1;
	}
    }

    val.fractVal.numerator = video_zoom_numerator;
    val.fractVal.denominator = video_zoom_denominator;
    if (vlSetControl(svr, path, drn, VL_ZOOM, &val) < 0) {
	vlPerror("Video: Can't set requested zoom");
	return -1;
    }

   /*
    * Now start asking video for size, rate, capture type, etc.
    */
    dioav.video_packing = packing;
    dioav.video_format = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
    if (vlGetControl(svr, path, drn, VL_SIZE, &val) < 0) {
	vlPerror("Video: Can't query size");
	return -1;
    }
    dioav.video_height = val.xyVal.y;
    dioav.video_width = val.xyVal.x;

    if (vlGetControl(svr, path, drn, VL_CAP_TYPE, &val) < 0) {
	vlPerror("Video: Can't query capture type");
	return -1;
    }
    dioav.video_captype = val.intVal;

    if (vlGetControl(svr, path, drn, VL_RATE, &val) < 0) {
	vlPerror("Video: Can't query capture type");
	return -1;
    }
    dioav.chunk_rate = val.intVal;

   /* 
    * Register for preempted and transfer failed masks 
    */
    vlSelectEvents(svr, path,  ( VLStreamPreemptedMask	|
					VLTransferFailedMask | 
					VLSequenceLostMask));
    printf("Video: frame rate is %d\n", dioav.chunk_rate);
    printf("       packing is %d, format is %d\n", dioav.video_packing,
	dioav.video_format);
    printf("       size is %d x %d\n", dioav.video_width, dioav.video_height);

    return 0;
}

/*
 * Initialize audio
 *
 * Returns 0 on success, -1 on error
 */
int
audinit(void)
{
    ALconfig portconfig;
    long pvbuf[12];
    DMparams *params;

    portconfig = ALnewconfig();
    if (portconfig == NULL) {
	fprintf(stderr, "Unable to get audio config\n");
	return -1;
    }

   /*
    * Get the global IRIS Audio Processor paramters
    */
    pvbuf[0] = AL_INPUT_RATE;
    ALgetparams(AL_DEFAULT_DEVICE, pvbuf, 2);
    dioav.audio_frame_rate = pvbuf[1];
    dioav.audio_sample_format = ALgetsampfmt(portconfig);
    dioav.audio_num_channels = ALgetchannels(portconfig);
    dioav.audio_sample_width = ALgetwidth(portconfig);

    if (ALsetqueuesize(portconfig, dioav.audio_frame_rate * 
	dioav.audio_num_channels * 2 ) < 0) {
	fprintf(stderr, "Unable to set queue size to 1s\n");
	return -1;
    }

    audport = ALopenport("avrecord", "r", portconfig);
    if (audport == NULL) {
	fprintf(stderr, "Unable to open audio port\n");
	return -1;
    }

    nsec_per_audframe = NSEC_PER_SEC  / dioav.audio_frame_rate;

    if (dmParamsCreate(&params) < 0) {
	fprintf(stderr, "Audio: Out of memory\n");
	return -1;
    }

    if (dmSetAudioDefaults(params, dioav.audio_sample_width * 8,
	dioav.audio_frame_rate, dioav.audio_num_channels) < 0) {
	fprintf(stderr, "Audio: Out of memory\n");
	return -1;
    }

    if (dmParamsSetEnum(params, DM_AUDIO_FORMAT, 
	dioav.audio_sample_format) < 0) {
	fprintf(stderr, "Unable to set audio format in parameter list\n");
	return -1;
    }

    dioav.audio_frame_size = dmAudioFrameSize(params);
    dmParamsDestroy(params);

    if (dioav.audio_frame_size < 0) {
	fprintf(stderr, "Unable to determine audio frame size\n");
	return -1;
    }


    printf("Audio: frame rate is %d\n", dioav.audio_frame_rate);
    printf("       channels: %d\n", dioav.audio_num_channels);
    printf("       samples width: %d bytes\n", dioav.audio_sample_width);

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
    long audTransferSize;


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

   /*
    * Now initialize things common to audio and video. First determine the
    * respective transfer sizes (bytes of audio/video per chunk)
    */
    dioav.video_trans_size = vlGetTransferSize(svr,path);
    if (dioav.video_trans_size < 0) {
	vlPerror("Video: Can't query transfer size");
	return -1;
    }

   /*
    * For audio, reserve space for 110% of the expected required space, 
    * just in case video is playing 10% slower than audio. 
    */
    dioav.audio_num_frames = dioav.audio_frame_rate * 110 / 100;
    dioav.audio_num_frames = (dioav.audio_num_frames + dioav.chunk_rate -1) /
	dioav.chunk_rate;
    audTransferSize = dioav.audio_num_frames * dioav.audio_frame_size;

   /*
    * Set the buffer quantum to the chunk size. This is the cumulative
    * size of the video buffer, audio header, and audio buffer.
    * Additionally round up to a 4k boundry, which satisfies the Ciprico
    * 6900 strip size.
    */
    dioav.chunk_size = dioav.video_trans_size + audTransferSize + 
	sizeof(audio_header);
    dioav.chunk_size = ((dioav.chunk_size + 4095) / 4096) * 4096;
    val.intVal = dioav.chunk_size;
    if (vlSetControl(svr, path, drn, VL_MGV_BUFFER_QUANTUM, &val) < 0) {
        vlPerror("Unable to set chunk size");
        return -1;
    }

   /* 
    * Create a VL ring buffer for the data transfers. This buffer will 
    * actually hold both audio and video data, which is why the buffer
    * quantum was made large enough to hold both.  
    */
    buffer = vlCreateBuffer(svr, path, drn, DMRBSIZE);
    if (!buffer) {
	vlPerror("Unable to create VL buffer");
	return -1;
    }

   /*
    * Make the buffer non-cacheable since we are doing dma into and out of
    * the ring buffer. If the data will be processed, then the buffer should
    * be made cacheable (VL_BUFFER_ADVISE_ACCESS), which is the default mode.
    */
    vlBufferAdvise(buffer, VL_BUFFER_ADVISE_NOACCESS);

    if (vlRegisterBuffer(svr, path, drn, buffer)) {
	vlPerror("Unable to register buffer");
	return -1;
    }

   /*
    * Now write out the dioav header
    */
    tbuf = (char *)memalign(4096, dioav.dio_size);
    bcopy(&dioav, tbuf, sizeof(dioav));
    if (write(fd, tbuf, dioav.dio_size) != dioav.dio_size) {
	perror("Unable to write header");
	free(tbuf);
	return -1;
    }
    free(tbuf);

    printf("Video transfer size is %d\n", dioav.video_trans_size);
    	 
    return 0;
}

/*
 * Handle video library events
 * (only transfer complete and transfer failed events in this example)
 */
int
ProcessEvent(VLServer svr)
{
    VLEvent ev;

    while (vlCheckEvent(svr, VLAllEventsMask, &ev) != -1) {
    	switch (ev.reason) {
	    case VLTransferFailed:
	    	fprintf(stderr,"%s: Transfer failed.\n",_progName);
	    	return -1;
	  	break;
	    case VLSequenceLost:
	    	fprintf(stderr, "%s: Sequence lost, %d frames\n", _progName, 
		    ev.vlsequencelost.count);
		return -1;
	        break;
	    case VLStreamPreempted:
		fprintf(stderr, "%s: Preempted\n", _progName);
		return -1;
		break;
	    default:
	        break;
    	}
    }

    return 0;
}

/*
 * Do general cleanup before exiting. All of our resources should
 * be freed.
 */
void 
docleanup(int ret)
{
    if (svr) {
	if (path) {
    	    vlEndTransfer(svr, path);
	    if (buffer) {
    	    	vlDeregisterBuffer(svr, path, drn, buffer);
    	    	vlDestroyBuffer(svr, buffer);
	    }
    	    vlDestroyPath(svr, path);
	}
    	vlCloseVideo(svr);
    }

    exit(ret);
}

int 
sync_audio_to_ust(unsigned long long ust)
{
    unsigned long long audmsc, audust, audfrontier_msc;
    long long targetmsc;
    char samps[65536];
    long long nframes;
    
    if (ALgetframetime(audport, &audmsc, &audust) < 0) {
	perror("ALgetframetime:");
    }
    if (ALgetframenumber(audport, &audfrontier_msc) < 0) {
	perror("ALgetframenumber");
    }
    targetmsc = (ust - audust) ;
    targetmsc /= nsec_per_audframe;
    targetmsc += audmsc;
    printf("audmsc = %lld, audust = %lld\n", audmsc, audust);
    printf("target ust = %lld, target msc = %lld, audfrontier_msc = %lld\n", 
	ust, targetmsc, audfrontier_msc);

    if (ALgetframenumber(audport, &audfrontier_msc) < 0) {
	perror("ALgetframetime:");
    }

    do {
	if (ALgetframenumber(audport, &audfrontier_msc) < 0) {
	    perror("ALgetframetime:");
	}
	nframes = (targetmsc-audfrontier_msc);
	printf("nframes = %lld\n", nframes);
	if (nframes > 0) {
	    ALreadsamps(audport, samps, min(nframes*2, 32768));
	}
    } while (nframes > 16384);

    return 0;
}

int
dmrb_ready(VLServer svr)
{
    VLInfoPtr info;
    DMediaInfo *dminfo;
    char *dataPtr;
    int run = 30;
    unsigned long long audtime, fnum, audcurframe;
    long long ustdelta;
    static int first = 1;
    audio_header *audioheader;
    char *audiobuffer;
    long long video_frontier_ust;

    while ((--run) &&
	(frames_captured < imageCount) &&
        (info = vlGetNextValid(svr, buffer))) {

  	dminfo = vlGetDMediaInfo(svr, buffer, info);

  	video_frontier_ust = dminfo->ustime + video_ust_skew;
	if (first) {
	    first = 0;
	    sync_audio_to_ust(video_frontier_ust);
	}

    	/* Get a pointer to the frame(s) */
	dataPtr  = vlGetActiveRegion(svr, buffer, info);
	audioheader = (audio_header *)(dataPtr + dioav.video_trans_size);
   	audiobuffer = (char *)(dataPtr + dioav.video_trans_size +
	    sizeof(audio_header));

	if (ALgetframetime(audport, &fnum, &audtime) < 0) {
	    perror("ALgetframetime:");
	}
  	if (ALgetframenumber(audport, &audcurframe) < 0) {
	    perror("ALgetframenumber");
	}

	audtime += (audcurframe - fnum) * (nsec_per_audframe);
	ustdelta = video_frontier_ust + NSEC_PER_SEC/dioav.chunk_rate- audtime;
	audioheader->frame_count = (ustdelta + nsec_per_audframe/2) /
	    nsec_per_audframe;
	if (verbose) {
	    printf("Frame %d: audtime = %lld, vidtime = %lld, delta = %lld\n", 
	    	frames_captured, audtime, video_frontier_ust, 
		audtime - video_frontier_ust);
	}

       /*
	* Make sure that the frame count is valid
	*/
	if (audioheader->frame_count < 0) { 
	    audioheader->frame_count = 0;
	}
	else if (audioheader->frame_count > dioav.audio_num_frames) { 
	    audioheader->frame_count = dioav.audio_num_frames;
	}

	if (ALreadsamps(audport, audiobuffer, 
	    audioheader->frame_count * dioav.audio_num_channels) < 0) {
	    perror("ALreadsamps failed");
	    exit(1);
	}
	write(fd, dataPtr, dioav.chunk_size);
	vlPutFree(svr, buffer);
	printf("Captured frame %d\n", ++frames_captured);
    }

    return 0;
}

int
main(int argc, char **argv)
{
    int c, ret;
    int nfds, bufferfd, vlfd; 
    char *tmp;
    fd_set readfds, t_readfds, exceptfds, t_exceptfds;

    _progName = argv[0];

    /*
     * Parse command line options
     */
    while ((c = getopt(argc, argv, "hf:c:v:m:s:z:p:r:")) != EOF) {
	switch (c) {
	    case 'f':
		sprintf(outfilename,"%s",optarg);
	    	break;

	    case 'c':
		imageCount = atoi(optarg);
	    	break;

	    case 'v':
		vinnodenum = atoi(optarg);
	     	break;

	    case 'm':
		memnodenum = atoi(optarg);
		break;

	    case 'r':
		video_rate = atoi(optarg);
		break;

	    case 's':
		video_ust_skew = atol(optarg);
		break;

	    case 'z':
		tmp = strchr(optarg, '/');
		if (tmp == NULL) {
		    usage();
		    exit(1);
		}
		video_zoom_numerator = atoi(optarg);
		video_zoom_denominator = atoi(tmp+1);
		break;

	    case 'p':
		packing = atoi(optarg);
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


    if (outfilename[0] == '\0') {
	usage();
	exit(1);
    }

    if (avinit() < 0) {
	fprintf(stderr, "Unable to initialize\n");
	docleanup(1);
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

    /* Begin the data transfer */
    printf("Starting transfer\n");
    if (vlBeginTransfer(svr, path, 0, NULL) < 0) {
	vlPerror(_progName);
	exit(1);
    }

    /* Loop until all requested frames are grabbed */
    while (frames_captured < imageCount) {
	t_readfds = readfds;
	t_exceptfds = exceptfds;

	ret = select(nfds, &t_readfds, NULL, &t_exceptfds, NULL);

	if (ret == -1 && errno != EINTR) break;

	if (FD_ISSET(bufferfd, &t_readfds)) dmrb_ready(svr);
	if (FD_ISSET(vlfd, &t_readfds)) {
	    if (ProcessEvent(svr) < 0) {
		docleanup(1);
	    }
	}

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
}
