/*
 * avplayback:
 * read video and audio data from a QT file and play out synchronously
 * This program assumes that audio and video clocks are locked in hardware. 
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <bstring.h>
#include <fcntl.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>
#include <sys/syssgi.h>
#include <sys/time.h>
/* for scatter-gather */
#include <sys/uio.h>
/* for semaphores */
#include <ulocks.h>

#include <dmedia/vl.h>

#define MGV TRUE
#define DIVO TRUE
#define USTMSC TRUE

/*
 * to work around lack of streamstopped event in old SRV VL
 * goes away in 6.5 when SRV supports new VL
 */
#define SRV_FICUS_COMPAT TRUE
/*
 * to work around libmoviefile nonsupport for 10 bit formats
 * goes away "soon"
 */
#define TEN_BIT_SUPT FALSE

#if MVP
#include <dmedia/vl_mvp.h>
#endif

#if MGV
#include <dmedia/vl_mgv.h>
#endif

#include <dmedia/dm_image.h>
#include <dmedia/moviefile.h>

/* header for the Audio Library */
#include <dmedia/audio.h>
#include <dmedia/audiofile.h>

/* for using external V-LAN transmitter */
#include <dmedia/vlan.h>

/* header for debugging */
#include <assert.h>

/*
 * globals for debugging
 */
static char *progName = "avplayback";
static int Verbose = FALSE;
static int Debug = FALSE;

/*
 * globals for semaphores and mp threads
 */
#define ANYKID -1
static usema_t *audioStartSema=NULL;
usptr_t * avSyncArena;
/* semaphore for the video player */
static usema_t *videoAvailSema;
static usema_t *videoStopSema;
static int videoStopSemaFD;
static usema_t *mainStartSema;
static usema_t *mainStopSema;

/*
 * general functions
 */
static void usage(void);
static stamp_t initialize_audio_sync(void);
static stamp_t (* initialize_video_ustmsc)();
static stamp_t initialize_VL_ustmsc(long long);
static stamp_t initialize_DM_ustmsc(long long);
static void benchmark_results(void);

/* 
 * globals for disk I/O 
 */
#define DEFAULT_IMAGE_COUNT 	4
#define DEFAULT_IOVEC_SIZE	4

static char *ioFileName = "videodata";
static int dataFileDesc = 0;
static struct stat fileStat;
static struct iovec *videoData=NULL;
static long ioVecCount=DEFAULT_IOVEC_SIZE;

/* 
 * ioBlockSize and ioAlignment are extremely important to the functionality and
 * performance of this utility.  Their values depend on the type of I/O
 * used.  There are three methods of writing to the disk: 1) raw device via
 * syssgi(); 2) standard write with XFS; 3) writev/readv.
 */  
#define NANOS_PER_SEC   	1e9
#define DISK_BYTES_PER_MB	1000000.0F
/* this should be in a sys header file but it is not */
static int ioBlockSize;
static int ioAlignment;
static int ioMaxXferSize = 0;
static int ioBytesPerXfer=0;

/* 
 * functions for disk I/O
 */
static int open_movie_file(void);
static int read_qt_offset_data(int, off64_t *, size_t *, size_t *, size_t *, size_t *);
static int open_data_file(void);

/* 
 * benchmark timing parameters 
 */
static struct timeval BeginTime, EndTime;
static double DiskTransferTime = 0;

/*
 * globals for dmbuffers
 */
static DMbuffer * dmBuffers;
static int dmBufferPoolSize;
static DMbufferpool dmPool;
static DMparams * paramsList;

#define DMBUFFERS_MAX_QUEUE	100	/* maximum number of images which can
					 * be queued in the dmbuffer FIFO */

/*
 * globals for video
 */
static VLServer vlServer;
static VLDevList vlDeviceList;
static VLDevice *vlDeviceInfo;
static VLNode vlMemNode, vlVidNode;
static VLPath vlPath;
static VLTransferDescriptor vlXferDesc;
static int vlDevice = 0;
static int vlCompatMode = 0;
static int vlCompression = 0;
static int vlFormat = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
static int vlPacking = VL_PACKING_YVYU_422_8;  /* == VL_PACKING_242_8; */
#ifndef MVP
static int vlColorSpace = VL_COLORSPACE_CCIR601;
#endif
static int vlXsize = 0, vlYsize = 0;
static int vlNumImages=0;
static int vlTiming = VL_TIMING_525_CCIR601;
static int vlFieldDominance = -1;
static int vlCaptureMode;
static int vlXferCount = 0;
static int vlDropCount = 0;
static int vlAbortOnError = 0;
static int vlMemory = 0;	/* read everything into memory if we can */
static int vlBenchmark = 0;
static int vlRiceEnable = 0;
static int vlRicePrecision = 0;         /* if = 0 we do not do compression */
static int vlRiceSampling = 0;
static int is_SRV = 0;
static USTMSCpair vlUSTMSC;	/* UST/MSC of a reference video field */

/* 
 * functions for video  
 */
static int initialize_video(void);
static int initialize_dmbuffers(void);
static int initialize_black_frames(void);
static void cleanup_video(void);
static void cleanup_dmbuffers(void);
static void (* video_player)();
static void video_DIVO_player(void *);

/*
 * globals for audio
 */
typedef enum _audioScheme
{
    none,
    qt,
    aiff
} audioScheme;

static char	*alFileName = NULL;
static AFfilehandle aflAudioFile;       /* handle to audio file */
static ALconfig alPortConfig;		/* handle to audio port */
static ALport   alAudioPort = 0;
static int      alSampleBufferSize;     /* samples / sample buffer */
static int      alFrameBufferSize;      /* frames / sample buffer */
static char     *alSampleBuffer;        /* sample buffer */
static unsigned long long alUSTPerMSC;
static long 	alSampleRate = 44100;	/* audio input sample rate */
static volatile int alInterrupt=0;      /* flag set when audio ends */
static int alMute = 0;			/* enable audio */

/*
 * defines for audio
 */
#define NO_AUDIO	0
#define QUICKTIME_AUDIO	1
#define AIFF_AUDIO	2

/* functions for audio */
static int initialize_audio(void);
static void audio_player(void *);
static void audio_interrupt(void);
static void cleanup_audio(void);
static int get_audio_hardware_input_rate(void);


/* 
 * time scale for libmovie files 
 */
#define MV_IMAGE_TIME_SCALE_NTSC 30000
#define MV_IMAGE_TIME_SCALE_PAL  25
#define MV_IO_BLOCKSIZE         "DM_IMAGE_BLOCKSIZE"

/* 
 * NOTE: there is inconsistent nomenclature between the VL and ML when talking
 * about odd and even fields 
 */
static MVfileformat mvFormat;
static DMpacking mvPacking;
static DMinterlacing mvInterlacing;
static DMorientation mvOrientation;
static MVtimescale mvImageTimeScale;
static MVtimescale mvAudioTimeScale;
static int mvXsize;
static int mvYsize;
static int mvBytesPerImage;
static int mvBytesPerPixel;
static int mvBlockSize;
static MVid theMovie;
static MVid mvImageTrack;
static MVid mvAudioTrack;

/*
 * functions for libmovie file
 */
static void print_movie_params(const DMparams *);

/* 
 * variables and defines for VLAN control 
 */
#define VLAN_RETRY 5
#define VLAN_CMD_BUF_SIZE 80
#define VLAN_EDIT_CMD "PF"

/* 
 * allow for longest V-LAN command string, i.e. "SI -00:00:00:00\0"
 */
#define VLAN_CMD_SIZE 3
#define VLAN_PARAM_SIZE 13
				
static FILE * vlanFileDesc;
static unsigned int vlanFramesPerSec;
static unsigned int vlanFramesPerMin;
static unsigned int vlanFramesPerHour;
static VLAN_DEV *vlanDevice = NULL;
static char *vlanTTY = NULL;
static char * vlanCmdFile = NULL;
static char * vlanResp = NULL;
static int vlanCoincidence = 0;
static int vlanINTInPoint = 0;

/* 
 * V-LAN functions 
 */
extern int vlan_read_byte(VLAN_DEV *, char *);
static unsigned int SMPTE_to_INT(char *);
static char * INT_to_SMPTE(unsigned int);
static int read_vlan_cmd_file(void);
static int sendVLANCmd(char *);

int
main(int ac, char **av)
{
    int i, c;
    extern char *optarg;
    extern int optind;
    int video_player_pid=NULL, audio_player_pid=NULL;
    char filename[128], *strpos=NULL;
    long iovec_count_max;
    VLEvent event;
    int done=0;

    /*
     * Flags and arguments
     */
    progName = av[0];

    while ((c = getopt(ac, av, "a:b:d:g:hi:k:t:v:ABDMV")) != EOF) {
	switch (c) {
	case 'a':
            alFileName = optarg;
	    break;
	case 'b':
	    dmBufferPoolSize = atoi(optarg);
	    break;
	case 'd':
	    vlDevice = atoi(optarg);
	    break;
	case 'g':
	    ioVecCount = atoi(optarg);
	    break;
	case 'i':
	    vlNumImages = atoi(optarg);
	    break;
	case 't':
	    vlanTTY = optarg;
	    break;
	case 'v':
	    vlanCmdFile = optarg;
	    break;
	case 'A':
	    vlAbortOnError = 1;
	    break;
	case 'B':
	    vlBenchmark = 1;
	    break;
	case 'D':
	    Debug = 1;
	    break;
	case 'M':
	    alMute = 1;
	    break;
	case 'V':
	    Verbose = 1;
	    break;
	case 'h':
	default:
	    usage();
	    break;
	}
    }

    if (ac > (optind + 1)) {
	usage();
    }

    if (ac == (optind + 1))
	ioFileName = av[optind];

    if ( !( mvIsMovieFile( ioFileName ) ) ) {
        fprintf(stderr, "%s is not a movie file.\n", ioFileName);
        exit(DM_FAILURE);
    }


    /* check legal values for ioVec, if you are saving fields ioVecCount must 
     * be even for QT files, since fields with gaps between them can only be 
     * written out in pairs
     */
    if (ioVecCount % 2)
	ioVecCount++;

    /* check for range */
    iovec_count_max = sysconf(_SC_IOV_MAX);
    if (ioVecCount > iovec_count_max) {
	ioVecCount = iovec_count_max;
	if (Verbose) {
	    fprintf(stderr, "cannot create more than %d I/O vectors\n", iovec_count_max);
	    fprintf(stderr, "\tresetting value!\n");
	}
    }
    else if (ioVecCount <= 0)
	ioVecCount = DEFAULT_IOVEC_SIZE;
   
    /* 
     * dmBufferPoolSize should be at least twice the size of ioVecCount so
     * that we get a double buffer affect when queuing buffers for output to 
     * the video driver and reading from the disk.
     */
    if (dmBufferPoolSize < ioVecCount)
	dmBufferPoolSize = ioVecCount << 3; 

    if (open_movie_file() == DM_FAILURE) {
	fprintf(stderr, "error creating movie file\n");
	exit(DM_FAILURE);
    }

    if (dmBufferPoolSize >= vlNumImages)
      dmBufferPoolSize = vlNumImages;

    if (open_data_file() == DM_FAILURE) {
	fprintf(stderr, "error initializing data file parameters\n");
	exit(DM_FAILURE);
    }

    /*
     * Initialize everything
     */

    /* initialize video */
    if (initialize_video() != DM_SUCCESS)
	exit(DM_FAILURE);

    /* initialize dmbuffers */
    if (initialize_dmbuffers() != DM_SUCCESS)
        exit(DM_FAILURE);

    /* 
     * find out what kind of video hardware we have so that we can make
     * the hardware dependency decisions
     */
    if (vlGetDeviceList(vlServer, &vlDeviceList) != DM_SUCCESS) {
	fprintf(stderr, "cannot get VL device information\n");
	exit(DM_FAILURE);
    }

    /*  
     * parse device names from the list and set video player function pointer 
     */
    vlDeviceInfo = &(vlDeviceList.devices[vlDevice]);
    if (strncasecmp(vlDeviceInfo->name, "divo", 4) == 0)
        video_player = video_DIVO_player;
      else if (strncasecmp(vlDeviceInfo->name, "impact", 6) == 0)
	{
	is_SRV = 1;
        video_player = video_DIVO_player;
	}
      else if (strncasecmp(vlDeviceInfo->name, "mvp", 3) == 0)
        video_player = video_DIVO_player;

    /* 
     * there are two methods of calibrating UST/MSC values so we use a 
     * function pointer for convenience.
     */
    initialize_video_ustmsc = initialize_DM_ustmsc;

    /*
     * create the appropriate processes and initialize the semaphore arena.
     */
    usconfig(CONF_ARENATYPE, US_SHAREDONLY);
    avSyncArena = usinit("/var/tmp/avplayback_arena");

    if (alFileName && (initialize_audio() == DM_SUCCESS)) {
	if ((audio_player_pid = sproc(audio_player, PR_SADDR|PR_SFDS)) < 0){
	    perror("audio_player");
	    exit(DM_FAILURE);
	}
        /* allocate a semphore for the audio thread and initialize it to
         * stop the process
         */
        if (NULL == (audioStartSema = usnewsema(avSyncArena, 0))) {
            perror("audioStartSema");
            exit(DM_FAILURE);
        }
    }

    /*
     * allocate and initialize semaphores for the video player thread
     */

    if (NULL == (videoAvailSema = usnewsema(avSyncArena, 0))) {
        perror("videoAvailSema");
        exit(DM_FAILURE);
    }

#if VIDEO_SEMA
    /* initialize main to stop */
    if (NULL == (videoStopSema = usnewpollsema(avSyncArena, 0))) {
        perror("videoStopSema");
        exit(DM_FAILURE);
    }

    /* initialize a FD do that we can select later */
    if (NULL == (videoStopSemaFD = usopenpollsema(videoStopSema,0600))){
        perror("videoStopSemaFD");
        exit(DM_FAILURE);
    }
#endif

    /* initialize black frames and queue them for output */
    if (initialize_black_frames() == DM_FAILURE) {
	fprintf(stderr, "error initializing black frames!\n");
	exit(DM_FAILURE);
    }

    if ((video_player_pid = sproc(video_player, PR_SADDR | PR_SFDS)) < 0) {
        perror("video_player");
	exit(DM_FAILURE);
    }

    /* let the I/O process que up some buffers */
    sginap(10);

    if (vlBeginTransfer(vlServer, vlPath, 1, &vlXferDesc) == DM_FAILURE) {
	fprintf(stderr, "cannot begin transfer: %s\n", vlStrError(vlErrno));
	exit(DM_FAILURE);
    }

    if (alFileName)
      initialize_audio_sync(); 

    while (!done) {

        while (vlPending(vlServer)) {

            if (vlNextEvent(vlServer, &event) < 0) {
                fprintf(stderr, "cannot get next event: %s\n",
                    vlStrError(vlErrno));
                exit(DM_FAILURE);
            }

	    switch(event.reason) {

		case VLTransferComplete:
		    if (vlXferCount++ == 0) {
			if (audioStartSema)
			    usvsema(audioStartSema);
                        vlUSTMSC = dmBufferGetUSTMSCpair(dmBuffers[0]);
                    }

		    if (Verbose)
			fprintf(stderr, "====== Output %s %d ======\n",
				(vlCaptureMode == VL_CAPTURE_NONINTERLEAVED ?
						  "field" : "frame"),
				vlXferCount);

		    /* if no more valid buffers - force this process to yield */
		    uspsema(videoAvailSema);
                    break;

		case VLStreamStarted:
		    if (Verbose)
			fprintf(stderr, "VL stream started\n");
                    break;

		case VLSyncLost:
		    if (vlAbortOnError) {
         		fprintf(stderr, "ABORTING: sync signal lost!\n");
         		exit(DM_FAILURE);
		    }
		    else if (Verbose)
         		fprintf(stderr, "WARNING: sync signal lost!\n");
		    break;

		case VLTransferFailed:
		    if (Verbose) {
                	fprintf(stderr, "ABORTING: transfer failed on image %d!\n", vlXferCount);
           		exit(DM_FAILURE);
		    }
		    else if (Verbose)
                	fprintf(stderr, "Transfer failed on image %d!\n", vlXferCount);

		case VLStreamPreempted:
               	    fprintf(stderr, "Stream pre-empted!\n");
           	    exit(DM_FAILURE);

		case VLSequenceLost:
		    if (vlAbortOnError) {
                	fprintf(stderr, "ABORTING: sequence lost\n");
         		exit(DM_FAILURE);
		    }
		    else {
	 		if (Verbose)
               	  	    fprintf(stderr, "Sequence lost!\n");
		    }
		    break;

		case VLStreamStopped:
		    fprintf(stderr, "Stream stopped\n");
		    done=1;
                    break;

		default:
		    if (vlAbortOnError) {
			fprintf(stderr, "ABORTING: received unexpect event %s.\n", vlEventToName(event.reason));
         		exit(DM_FAILURE);
		    }
		    else if (Debug && Verbose) {
			fprintf(stderr, "Received unexpect event %s\n",
				vlEventToName(event.reason));
                    }
                    break;
	    }
	}

    }

    /* stop video */
    vlEndTransfer(vlServer, vlPath);
    /* stop audio */
    audio_interrupt();
    fprintf(stderr, "%s finished!\n", progName);
}


static void
usage(void)
{
    fprintf(stderr, "Usage: %s -[a%%s b# d# h i# t%%s A B D V v vlanfile.cmd] data_file_name\n",
	progName);
    fprintf(stderr, "\t-a%%s audio file\n");
    fprintf(stderr, "\t-b#  dmbuffer pool size (default = 4)\n");
    fprintf(stderr, "\t-d#  VL device number\n");
    fprintf(stderr, "\t-g#  readv # fields from disk (default = 2)\n");
    fprintf(stderr, "\t-h   give this help message\n");
    fprintf(stderr, "\t-i   only output the first # images from the file\n");
    fprintf(stderr, "\t-t%%s specify /dev/ttyc# for V-LAN control\n");
    fprintf(stderr, "\t-v   filename  V-LAN command file\n");
    fprintf(stderr, "\t-A   abort upon error condition during output\n");
    fprintf(stderr, "\t-B   calculate capture performance and print results\n");
    fprintf(stderr, "\t-D   debug mode\n");
    fprintf(stderr, "\t-M   mute audio output\n");
    fprintf(stderr, "\t-V   verbose mode\n");
    fprintf(stderr, "\tfile_name  file name\n");

    exit(DM_FAILURE);
}


static int
initialize_video(void)
{
    VLControlValue val;
#if MVP
    int dom_ctrl = VL_MVP_DOMINANT_FIELD;
#endif

#if MGV
    int dom_ctrl = VL_MGV_DOMINANCE_FIELD;
#endif

    /*
     * Open the video daemon
     */
    if (!(vlServer = vlOpenVideo("")))
    {
	fprintf(stderr, "cannot open video daemon: %s\n", vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    /*
     * Get nodes: a memory source, and a video drain
     */
    if ((vlMemNode = vlGetNode(vlServer, VL_SRC, VL_MEM, VL_ANY)) == DM_FAILURE) {
	fprintf(stderr, "cannot get memory source node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    if ((vlVidNode = vlGetNode(vlServer, VL_DRN, VL_VIDEO, VL_ANY)) == DM_FAILURE) {
	fprintf(stderr, "cannot get video drain node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    /*
     * Get a path
     */
    if ((vlPath = vlCreatePath(vlServer, vlDevice, vlMemNode, vlVidNode)) < 0)
    {
	fprintf(stderr, "cannot create path: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    /*
     * Setup the path
     */
    if (vlSetupPaths(vlServer, (VLPathList)&vlPath, 1, VL_SHARE, VL_SHARE) < 0)
    {
	fprintf(stderr, "cannot setup path: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    if (vlCompatMode) {
    }
    else {
	val.intVal = vlFormat;
	if (vlSetControl(vlServer, vlPath, vlVidNode, VL_FORMAT, &val)) {
	    fprintf(stderr, "cannot set format on source node: %s\n",
			vlStrError(vlErrno));
	    return(DM_FAILURE);
	}

#ifndef MVP
	/* for DIVO and beyond */
	val.intVal = vlColorSpace;
	if (vlSetControl(vlServer, vlPath, vlMemNode, VL_COLORSPACE, &val)) {
	    fprintf(stderr, "cannot set colorspace on drain node: %s\n",
			vlStrError(vlErrno));
	    return(DM_FAILURE);
	}
#endif
    }


    /*
     * NOTE: cannot set or get the timing on a memory node as was possible with
     * Sirius
     */
    if (vlGetControl(vlServer, vlPath, vlMemNode, VL_TIMING, &val)) {
	fprintf(stderr, "cannot get timing on source node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    /* 
     * sanity checking for colorspace, format, and packing is done when the 
     * packing is set so we do this last.
     */
    val.intVal = vlPacking;
    if (vlSetControl(vlServer, vlPath, vlMemNode, VL_PACKING, &val)) {
	fprintf(stderr, "cannot set path packing: %s\n", vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    val.intVal = vlCaptureMode;
    if (vlSetControl(vlServer, vlPath, vlMemNode, VL_CAP_TYPE, &val)) {
	fprintf(stderr, "cannot set path capture type: %s\n", 
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    if (vlGetControl(vlServer, vlPath, vlMemNode, VL_CAP_TYPE, &val)) {
	fprintf(stderr, "cannot get path capture type: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    if (vlRiceEnable)
      {
      if (Verbose && Debug)
	{
	fprintf(stderr, "vlRiceSampling : %d\n", vlRiceSampling);
	fprintf(stderr, "vlRicePrecision : %d\n", vlRicePrecision);
	}

      val.intVal = VL_COMPRESSION_RICE;
      if (vlSetControl(vlServer, vlPath, vlMemNode, VL_COMPRESSION, &val))
	{
	fprintf(stderr, "cannot set compression on memory node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
	}

      if (vlGetControl(vlServer, vlPath, vlMemNode, VL_COMPRESSION, &val))
	{
	fprintf(stderr,"cannot verify compression on memory node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
	}

      val.intVal = vlRicePrecision;
      if (vlSetControl(vlServer, vlPath, vlMemNode, VL_RICE_COMP_PRECISION, &val))
	{
	fprintf(stderr, "cannot set Rice compression precision on memory node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
	}

      if (vlGetControl(vlServer, vlPath, vlMemNode, VL_RICE_COMP_PRECISION, &val))
	{
	fprintf(stderr, "cannot verify Rice compression precision on memory node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
	}

      val.intVal = vlRiceSampling;
      if (vlSetControl(vlServer, vlPath, vlMemNode, VL_RICE_COMP_SAMPLING, &val))
	{
	fprintf(stderr, "cannot set Rice compression sampling on memory node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
	}

      if (vlGetControl(vlServer, vlPath, vlMemNode, VL_RICE_COMP_SAMPLING, &val))
	{
	fprintf(stderr, "cannot verify Rice compression sampling on memory node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
	}
      }

    /* this is terribly confusing, we must be careful when verifying the
     * image dimensions and data transfer size. 
     * the size of the memory node is dependent on the capture type, and this 
     * must be consistent with the data file, however, the video node is always
     * full raster.  libmoviefile supports split fields not real fields so it 
     * also returns full raster sizes even when storing fields.  So we use the 
     * use the libmoviefile full raster size values when computing the data 
     * transfer size.
     * We use the true image dimensions when computing the dmBuffer memory
     * allocation.
     */
    if (vlGetControl(vlServer, vlPath, vlMemNode, VL_SIZE, &val)) {
	fprintf(stderr, "couldn't get path size: %s\n", vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    vlXsize = val.xyVal.x;
    if (vlXsize != mvXsize)
      {
      fprintf(stderr, "Xsize of image file (%d) is inconsistent with memory node setting (%d)\n", mvXsize, vlXsize);
      fprintf(stderr, "\teither the timing is not set properly or the file is not compatible.\n");
      exit(DM_FAILURE);
      }

    vlYsize = val.xyVal.y;
    if (vlYsize != (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED ? mvYsize >> 1 :
								 mvYsize))
      {
      fprintf(stderr, "Ysize of image file (%d) is inconsistent with memory node setting (%d)\n", mvYsize, vlYsize);
      fprintf(stderr, "\teither the timing is not set properly or the file is not compatible.\n");
      exit(DM_FAILURE);
      }

    /*
     * Select interesting events
     */
    if (vlSelectEvents(vlServer, vlPath,
      ( 
    VLStreamBusyMask            |
    VLStreamPreemptedMask       |
    VLAdvanceMissedMask         |
    VLStreamAvailableMask       |
    VLSyncLostMask              |
    VLStreamStartedMask         |
    VLStreamStoppedMask         |
    VLSequenceLostMask          |
    VLControlChangedMask        |
    VLTransferCompleteMask      |
    VLTransferFailedMask        |
    VLDeviceEventMask           |
    VLDefaultSourceMask         |
    VLControlRangeChangedMask   |
    VLControlPreemptedMask      |
    VLControlAvailableMask      |
    VLDefaultDrainMask          |
    VLStreamChangedMask         |
    VLTransferErrorMask
       )) < 0) {
	fprintf(stderr, "cannot select VL events: %s\n",
	    vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    /* setup external V-LAN */
    if (vlanTTY) {

	/* Issue the VLAN command */
	if ((vlanDevice = vlan_open(vlanTTY)) == (VLAN_DEV*)NULL) {
            vlan_perror(progName);
	    return(DM_FAILURE);
        }
	if (vlan_alive(vlanDevice) == VLAN_NOTRUNNING) {
            fprintf(stderr, "no V-LAN transmitter connected\n");
            vlan_close(vlanDevice);
	    return(DM_FAILURE);
        }

	/* we want to be frame accurate so let's check for sync */
	if (sendVLANCmd("SN") == DM_SUCCESS) {
	    if ((vlanResp[0] != 'Y') && Verbose)
		fprintf (stderr, "WARNING: V-LAN controller has no sync\n");    
	}
	else {
            fprintf(stderr, "Error checking sync signal\n");
	    return(DM_FAILURE);
	}

	/* 
	 * if we have an external setup file let's read it.
	 * NOTE: we eventually will support an EDL.
	 */
	if (vlanCmdFile) {
	    if ((vlanFileDesc = fopen(vlanCmdFile, "r")) == NULL) {
        	fprintf (stderr, "Cannot open V-LAN command file!\n");
        	exit(DM_FAILURE);
	    }

	    read_vlan_cmd_file();
	}
	else {  /* if no command file just start the edit */
	    if (sendVLANCmd(VLAN_EDIT_CMD) == DM_FAILURE) {
        	fprintf (stderr, "Cannot issue V-LAN start edit command!\n");
		return(DM_FAILURE);
	    }
	}
    }

    /* setup transfer descriptor */
    vlXferDesc.mode = VL_TRANSFER_MODE_DISCRETE;
    /* allow for black frames */
    vlXferDesc.count = vlNumImages + (vlRiceEnable ? 0 : ioVecCount);
    vlXferDesc.delay = 0;
    /* GPI is not working yet so we trigger immediately */
    vlXferDesc.trigger = VLTriggerImmediate;

    if (Verbose)
	fprintf(stderr, "VL will output %d images\n", vlNumImages);

    return(DM_SUCCESS);
}


/*
 * initialize_dmbuffers() - setup dmBuffers according to the parameters of the
 *      movie file.
 */
static int
initialize_dmbuffers(void)
{
    if (dmParamsCreate(&paramsList) == DM_FAILURE) {
        fprintf(stderr, "error creating parameter list\n");
        return(DM_FAILURE);
    }

/* Todd, let's talk about this */
    /* we will try to batch process everything in memory */
#if 0
    if (vlMemory)
	dmBufferPoolSize = vlNumImages;	
#endif

    if (Verbose && Debug)
      {
      fprintf(stderr, "dmBufferPoolSize : %d\n", dmBufferPoolSize);
      fprintf(stderr, "ioBytesPerXfer : %d\n", ioBytesPerXfer);
      }
    if (dmBufferSetPoolDefaults(paramsList, dmBufferPoolSize, ioBytesPerXfer,
                DM_TRUE, DM_TRUE) == DM_FAILURE) {
        fprintf(stderr, "error setting pool defaults\n");
        return(DM_FAILURE);
    }

#if MVP
    if (vlDMPoolGetParams(vlServer, vlPath, vlMemNode, paramsList) == DM_FAILURE) {
        fprintf(stderr, "error getting pool params for VL\n");
        return(DM_FAILURE);
    }
#endif

#if (MGV || DIVO)
    if (vlDMGetParams(vlServer, vlPath, vlMemNode, paramsList) == DM_FAILURE) {
        fprintf(stderr, "error getting pool params for VL\n");
        return(DM_FAILURE);
    }
#endif

    /* make DMbuffers are aligned with the file system and page size */
    dmParamsSetInt(paramsList, DM_BUFFER_ALIGNMENT, ioAlignment);
    if (vlRiceEnable)
      {
      if (Verbose)
	fprintf(stderr, "Enabling RICE decoding\n");
      dmParamsSetEnum(paramsList, DM_POOL_VARIABLE, DM_TRUE);
      }

    /* create the buffer pool */
    if (dmBufferCreatePool(paramsList, &dmPool) == DM_FAILURE) {
        fprintf(stderr, "error creating DM pool\n");
        return(DM_FAILURE);
    }

#if 0
    if (vlDMPoolRegister(vlServer, vlPath, vlMemNode, dmPool) == DM_FAILURE) {
        fprintf(stderr, "error registering DM pool\n");
        return(DM_FAILURE);
    }
#endif

    /* sets size of the user's dmBuffer array */
    if ((dmBuffers = (DMbuffer *) calloc(dmBufferPoolSize, sizeof(DMbuffer *)))
                == NULL ) {
        fprintf(stderr, "error mallocing memory for dmbuffers\n");
        return(DM_FAILURE);
    }

    return(DM_SUCCESS);
}


/* 
 * create a series of black frames so that we can synchronize
 * NOTE: this only works for the first ioVecCount buffers should generalize
 * to bzero any group of buffers. 
 */
static int
initialize_black_frames(void)
{
    long long free_pool_bytes=0;
    int free_pool_buffers=0;
    int i;
    int err_no;

    /*	if RICE decoding is enabled, we punt since we don't hav properly
     *	encoded fields to load up
     */
    if (vlRiceEnable)
      return(DM_SUCCESS);

    /* check and see if we have any free buffers available */
    if (dmBufferGetPoolState(dmPool, &free_pool_bytes, &free_pool_buffers) != DM_SUCCESS) {
	fprintf(stderr, "%s\n", dmGetError(&err_no, NULL));
	return(DM_FAILURE);
    }
	
    while (free_pool_buffers < ioVecCount) {
	/* we must check again for sufficient space in the buffers */
	if (dmBufferGetPoolState(dmPool, &free_pool_bytes, &free_pool_buffers) == DM_FAILURE) {
	    fprintf(stderr, "no buffers available!\n");
	    fprintf(stderr, "%s\n", dmGetError(&err_no, NULL));
	    return(DM_FAILURE);
	}
    }
	   
    /* now allocate the buffers */
    for (i = 0; i < ioVecCount; i++) {

	if (dmBufferAllocate(dmPool, &dmBuffers[i]) == DM_FAILURE) {
	    fprintf(stderr, "buffer allocation failed!\n");
	    fprintf(stderr, "%s\n", dmGetError(&err_no, NULL));
	    return(DM_FAILURE);
	}
	bzero(dmBufferMapData(dmBuffers[i]), dmBufferGetSize(dmBuffers[i]));

	if (vlDMBufferPutValid(vlServer, vlPath, vlMemNode, 
		dmBuffers[i]) == DM_FAILURE) {
	    fprintf(stderr, "could not output valid buffer!\n");
            return(DM_FAILURE);
        }
	dmBufferFree(dmBuffers[i]);
    }

    /* initialize the semaphore (valid pool count to # black frames) */
#if 0
    if (usinitsema(videoAvailSema, ioVecCount) < 0)
      {
      fprintf(stderr, "initialize available buffer semaphore failed\n");
      return(DM_FAILURE);
      }
#endif
    return(DM_SUCCESS);
}


/*
 * initialize_audio() - get file parameters using the audiofile library
 *      to determine the data format of then initialize audio hardware with 
 * 	the audio library
 */
static int
initialize_audio(void)
{
    ALconfig audio_port_config;
    long audio_buffer_parameters[2];
    int channels_per_frame;
    int err;

    /* open the audio file and initialize all parameters */
    if ((aflAudioFile = afOpenFile(alFileName, "r", AF_NULL_FILESETUP)) ==
                AF_NULL_FILEHANDLE)
    {
        /* error message handling is not necessary because the audiofile
         * library has a default verbose error handler.
	 * we return failure condition if we cannot open a valid file.
         */
        return(DM_FAILURE);
    }

    /* the audio system outputs one frame per clock tick,
     * a frame may contain multiple channels, therefore,
     * samples = frames * channels
     *
     * NOTE: the old audiofile library is frame based, the new one is sample
     * based.
     *
     * we can query the number of channels in the file
     * channels_per_frame = afGetChannels(aflAudioFile, AF_DEFAULT_TRACK)
     * however, since ASO only supports 2 channels we will hardwire the value
     */
    channels_per_frame = afGetChannels(aflAudioFile, AF_DEFAULT_TRACK);
    alSampleRate       = afGetRate(aflAudioFile, AF_DEFAULT_TRACK);

    /* calculate space for 1/2 second worth of samples
     * this i measured in frames
     */
    alFrameBufferSize = 0.5 * alSampleRate;
    alSampleBufferSize = alFrameBufferSize * channels_per_frame;

    /* we read the input file format but some of the attributes of the file
     * may NOT be supported by the device although unlikely since we anticipate
     * that the file was recorded using a SGI audio device.
     * Using the virtual functions the the format of the data copied into the
     * output buffer can be different from the input file data format.
     */

    /* force the sample width of the ouput buffer to be 16 bits */
    afSetVirtualSampleFormat(aflAudioFile, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);

    /* set output channels to the default value */
    afSetVirtualChannels(aflAudioFile, AF_DEFAULT_TRACK, channels_per_frame);

    /*
     * move the fileptr to the beginning of the sample data,
     * this should be the last thing you do to the file data
     */
    afSeekFrame(aflAudioFile, AF_DEFAULT_TRACK, 0);

    /* setup the audio hardware */

    /* configure and open audio port */
    audio_port_config = ALnewconfig();

    /* set hardware to expect format to match that which was configured with
     * afSetVirtualSampleFormat
     */
    ALsetwidth(audio_port_config, AL_SAMPLE_16);
    ALsetchannels(audio_port_config, channels_per_frame);

/* Todd, why did i do this??? */
#if 0
    /* sync the input to an external AES signal */
    audio_buffer_parameters[0] = AL_OUTPUT_RATE;
    audio_buffer_parameters[1] = AL_RATE_AES_1;
    ALsetparams(AL_DEFAULT_DEVICE, audio_buffer_parameters, 2);
#endif

    /* Todd, is this a persistent control? */
    if (alMute) {
	audio_buffer_parameters[0] = AL_SPEAKER_MUTE_CTL;
	audio_buffer_parameters[1] = AL_SPEAKER_MUTE_ON;
	ALsetparams(AL_DEFAULT_DEVICE, audio_buffer_parameters, 2);
    }

    /* make the audio port buffer 2x as large as our sample storage buffer */
    ALsetqueuesize(audio_port_config, alSampleBufferSize*2);
    alAudioPort = ALopenport(progName, "w", audio_port_config);
    if (alAudioPort == 0) {
        err = oserror();
        if (err == AL_BAD_NO_PORTS) {
            fprintf(stderr,"no audio ports available at the moment\n");
            strerror(err);
        }
        else if (err == AL_BAD_OUT_OF_MEM) {
            fprintf(stderr,"not enough memory to open audio port\n");
            strerror(err);
        }
        return(DM_FAILURE);
    }

    /*  get the size of the sample frame in bytes of
        afGetVirtualFrameSize does the same for the virtual format for the
        given track, as set by any combination of calls to
        afSetVirtualSampleFormat and afSetVirtualChannels
        we can check the expect value against the return value:
        size = alSampleBufferSize * sizeof(short)
     */

    alSampleBuffer = malloc(alFrameBufferSize *
                afGetVirtualFrameSize(aflAudioFile, AF_DEFAULT_TRACK, TRUE));
    /* fill buffer with zeroes which will be used to initialize msc */
    bzero(alSampleBuffer, alSampleBufferSize);

    /*
     *  write 0's into the buffer so that there will we can measure a valid msc
     */
    ALwritesamps(alAudioPort, alSampleBuffer, alSampleBufferSize);

    if (Verbose)
    {
        fprintf(stderr,"audio filename is %s\n", alFileName);
        fprintf(stderr,"number of output channels %d\n",
                afGetVirtualChannels(aflAudioFile, AF_DEFAULT_TRACK));
        fprintf(stderr,"audio sample rate is %f\n", alSampleRate);
        fprintf(stderr,"audio sample buffer is %d frames of %d samples\n",
                alFrameBufferSize, alSampleBufferSize);
    }

    return(DM_SUCCESS);

} /* --------------------- end initialize_audio() --------------- */


static void 
audio_player(void *arg)
{
    int frame;

    /* block here until the video process transfers the first frame */
    uspsema(audioStartSema);

    /* while (vlUSTMSC.ust == 0); */

    if (Verbose) {
        fprintf(stderr, "starting the audio player\n");
	if (Debug) {
            fprintf(stderr, "At UST = %lld; MSC = %lld\n",
			vlUSTMSC.ust, vlUSTMSC.msc);
	}
    }

    /* read until we are out of data */
    while (0 != (frame = afReadFrames(aflAudioFile, AF_DEFAULT_TRACK, alSampleBuffer, alFrameBufferSize)))
    {
        ALwritesamps(alAudioPort, alSampleBuffer, alSampleBufferSize);

        /*
         * break out of the loop when we get an uneven number of frames
         * and let the buffer drain
         */
        if (frame != alFrameBufferSize)
            break;
    }

    /*
     * allow the audio buffer to drain
     */
    while(ALgetfilled(alAudioPort) > 0) {
        sginap(1);
    }

    exit (DM_SUCCESS);
} /* --------------------- end audio_player() --------------- */


static void
audio_interrupt(void)
{
    alInterrupt = 1;
}


/* ******************************************************************
 * get_audio_hardware_input_rate: acquire audio hardware input sampling rate
 * ****************************************************************** */
static int
get_audio_hardware_input_rate(void)
{
    long err, audio_buffer[6];

    /* acquire state variables of audio hardware */
    audio_buffer[0] = AL_INPUT_RATE;
    audio_buffer[2] = AL_INPUT_SOURCE;
    audio_buffer[4] = AL_DIGITAL_INPUT_RATE;

    /* if we get an error because of audio hardware problems it will happen
     * here first!  So we must deal with the audio process appropriately
     */
    if ((ALgetparams(AL_DEFAULT_DEVICE, audio_buffer, 6)) < 0) {
	err = oserror();
	strerror(err);
	fprintf(stderr, "Is audio hardware installed?\n");
	return(DM_FAILURE);
    }

    /* if the input source is analog (microphone or line) and the input rate 
     * is not sync'ed to the AES input clock, then just return the value
     * queried with ALgetparams, i.e. we have valid analog input 
     */
    if ((audio_buffer[3] == AL_INPUT_LINE) || (audio_buffer[3] == AL_INPUT_MIC))
    {
	if (Verbose)
	    fprintf(stderr, "audio input source is analog\n");
	if (audio_buffer[1] > 0) {
	    if (Verbose) {
		fprintf(stderr, "audio sample rate is %d\n", audio_buffer[1]);
	        fprintf(stderr, "WARNING: audio clock is free running.  It must be locked to guarantee synchronization\n");
	    }
            alSampleRate = audio_buffer[1];
	}
    }
    else if (audio_buffer[3] == AL_INPUT_DIGITAL) {
	if (Verbose)
	    fprintf(stderr, "audio input source is digital\n");
    }

    /* not all audio hardware support the ability to read the digital input
     * sampling rate
     */
    switch (audio_buffer[5]) {

	case AL_RATE_UNDEFINED:
	    if (Verbose)
		fprintf(stderr, "the digital input signal is valid, but rate is not encoded in the input signal\n");
	    break;

	case AL_RATE_UNACQUIRED:
	    if (Verbose)
		fprintf(stderr, "the digital input signal is valid, but rate is not acquired from the input signal\n");
	    break;

	case AL_RATE_NO_DIGITAL_INPUT:
	    if (Verbose)
		fprintf(stderr, "the digital input signal is not valid\n");
	    break;

	default:
	    if (Verbose)
		fprintf(stderr, "the digital audio clock rate is %d\n", audio_buffer[5]);
	    alSampleRate = audio_buffer[5];
    }

    return(DM_SUCCESS);

}   /* ---- end GetAudioHardwareInputRate() ---- */



/* this routine can be used for synchronizing both input and output of audio
 * and video data.  Care is taken to perform signed arithmetic with the unsigned
 * function parameters.
 */
static
stamp_t initialize_audio_sync(void)
{
    unsigned long long first_audio_msc;
    unsigned long long ref_audio_msc;
    unsigned long long ref_audio_ust;
    long long first_audio_ust, msc1, ust2, msc2;

    /* time the last audio sample arrived at the port */
    if (ALgetframetime(alAudioPort, &ref_audio_msc, &ref_audio_ust) < 0) {
        fprintf(stderr, "error getting first audio frame, underflow!\n");
        perror(progName);
        return(DM_FAILURE);
    }
    msc2 = ref_audio_msc;
    ust2 = ref_audio_ust;

    /* what is the # of the first sample which came into the port? */
    if (ALgetframenumber(alAudioPort, &first_audio_msc) < 0) {
        fprintf(stderr, "error getting last audio frame, underflow!\n");
        perror(progName);
        return(DM_FAILURE);
    }
    msc1 = first_audio_msc;

    /* this is the time at which the first audio sample was put into the buffer 
    alStartTime = ref_audio_ust - (ref_audio_msc - first_audio_msc)*alUSTPerMSC;
    */

    first_audio_ust = ust2 + (msc1 - msc2) * alUSTPerMSC;

    if (Verbose && Debug) {
        fprintf(stderr, "audio ust/msc = %ld\n", alUSTPerMSC);
        fprintf(stderr, "reference audio ust = %lld\n", ref_audio_ust);
        fprintf(stderr, "reference audio msc = %lld\n", ref_audio_msc);
        fprintf(stderr, "first audio msc = %lld\n", first_audio_msc);
        fprintf(stderr, "audio start time = %lld\n\n", first_audio_ust);
    }

    return(first_audio_ust);
}


/* This routine simply uses the UST time of the first field which came into
 * the buffer so we calculate the number of audio frames/samples to drop.
 * For input this simple method is fine, however, for output we must do 
 * more work.
 */
static
stamp_t initialize_DM_ustmsc(long long audio_start_time)
{
    stamp_t audio_frame_diff; 
    stamp_t audio_sample_diff; 

    /* calculate the time difference in audio samples */
    audio_frame_diff = (vlUSTMSC.ust - audio_start_time) / alUSTPerMSC;
    audio_sample_diff = audio_frame_diff * ALgetchannels(alPortConfig);

    if (Verbose && Debug) {
        fprintf(stderr, "video start time = %lld\n", vlUSTMSC.ust);
        fprintf(stderr, "number of audio samples to trim = %ld\n\n", audio_sample_diff);
    }

    return(audio_sample_diff);
}


/* This is called when we know there is valid audio data in the port and
 * we need to synchronize it with the first video frame.  It works for 
 * input or output.  See also avplayout.c
 */ 
static
stamp_t initialize_VL_ustmsc(long long audio_start_time)
{
    USTMSCpair ref_video_pair;		/* UST/MSC of a reference video field */
    stamp_t first_video_msc;		/* typedef signed long long stamp_t; */
    stamp_t video_start_time;
    double video_ust_per_msc;
    stamp_t audio_frame_diff;
    stamp_t audio_sample_diff;

    if ((video_ust_per_msc = vlGetUSTPerMSC(vlServer, vlPath, vlVidNode)) < 0) {
        vlPerror(progName);
        exit(DM_FAILURE);
    }

    /* grab a reference UST/MSC pair, this is the most recent field which came 
     * into the jack.  Although this event probably occured sometime in the 
     * past the data may not necessarily be in the dmbuffer yet.
     */ 
    if (vlGetUSTMSCPair(vlServer, vlPath, vlMemNode, 0, vlVidNode, &ref_video_pair) < 0) {
        vlPerror(progName);
        return(DM_FAILURE);
    }
 
    /* use the msc of the "first" field which is made available from the 
     * buffer, this is the "last"
     */
    first_video_msc = vlUSTMSC.msc; 

    /* note that the data types for the AL ust/msc routines are different
     * than the VL data types, unsigned long long vs. signed long long so
     * be careful with the math
     */

    /* calculate the time at which the current field came into the port */
    video_start_time = ref_video_pair.ust -
		(ref_video_pair.msc - first_video_msc) * video_ust_per_msc; 

    audio_frame_diff = (audio_start_time - video_start_time) / alUSTPerMSC;
    audio_sample_diff = audio_frame_diff * ALgetchannels(alPortConfig);

    if (Verbose && Debug) {
        fprintf(stderr, "video ust/msc = %f\n", video_ust_per_msc);
        fprintf(stderr, "reference video ust = %lld\n", ref_video_pair.ust);
        fprintf(stderr, "reference video msc = %lld\n", ref_video_pair.msc);
        fprintf(stderr, "first video field msc = %lld\n", first_video_msc);
        fprintf(stderr, "video start time = %lld\n", video_start_time);
        fprintf(stderr, "number of audio samples to trim = %ld\n\n", audio_sample_diff);
    }
    return(audio_sample_diff);
}


static void
cleanup_video(void)
{
    if (Verbose)
	fprintf(stderr, "cleaning up video!\n");

    vlDestroyPath(vlServer, vlPath);
    vlCloseVideo(vlServer);

    if (vlanCmdFile)
	fclose(vlanFileDesc);

    if (vlanTTY && vlanDevice)
	vlan_close(vlanDevice);
}


static void
cleanup_dmbuffers(void)
{
    mvClose(theMovie);

    if (Verbose)
	fprintf(stderr, "cleaning up dmbuffers!\n");

    vlDMPoolDeregister(vlServer, vlPath, vlMemNode, dmPool);
    dmBufferDestroyPool(dmPool);

    free(videoData);
    free(dmBuffers);
}


static void 
cleanup_audio(void)
{
    if (Verbose)
	fprintf(stderr, "cleaning up audio!\n");
    afCloseFile(aflAudioFile);
    ALfreeconfig(alPortConfig);
    ALcloseport(alAudioPort);
    free(alSampleBuffer);
}


static void
print_params(const DMparams* params)
{
    int len = dmParamsGetNumElems( params );
    int i;

    fprintf( stderr,"\n\t" );
   
    for ( i = 0;  i < len;  i++ ) {
	const char* name = dmParamsGetElem    ( params, i );
	DMparamtype type = dmParamsGetElemType( params, i );
	
	fprintf( stderr,"\t%25s: ", name );
	switch( type ) 
	    {

	    case DM_TYPE_INT:
		fprintf( stderr, "%d", dmParamsGetInt( params, name ) );
		break;
	    case DM_TYPE_STRING:
		fprintf( stderr, "%s", dmParamsGetString( params, name ) );
		break;
	    case DM_TYPE_FLOAT:
		fprintf( stderr, "%f", dmParamsGetFloat( params, name ) );
		break;
	    case DM_TYPE_FRACTION:
		{
		    DMfraction f;
		    f = dmParamsGetFract( params, name );
		    fprintf( stderr, "%d/%d", f.numerator, f.denominator );
		}
		break;
	    case DM_TYPE_PARAMS:
		print_params( dmParamsGetParams( params, name ));
		break;
	    default:
		fprintf( stderr, "unknown movie file parameter!\n" );
		break;
	    }
	fprintf( stderr, "\n" );
    }
}


static int
open_movie_file()
{
    DMparams * movie_params;
    DMparams * movie_user_data;
    int movie_images;

    if (mvOpenFile(ioFileName, O_RDONLY, &theMovie) == DM_FAILURE) {
	fprintf(stderr, "could not open movie file %s\n", ioFileName);
        return(DM_FAILURE);
    }

    /* verify valid file format */
    mvFormat = mvGetFileFormat(theMovie); 
    if (mvFormat != MV_FORMAT_QT) {
        fprintf(stderr, "%s only works with QuickTime files\n", progName);
        return(DM_FAILURE);
    }

    if (Verbose) {
	if ((movie_params = mvGetParams(theMovie)) == NULL) {
            fprintf(stderr, "unable to get movie params.\n");
	}
	else {
	   /* print_params(movie_params); */
	}
    }

    if (mvFindTrackByMedium(theMovie, DM_IMAGE, &mvImageTrack) == DM_FAILURE) {
        fprintf(stderr, "could not find image track in movie file %s\n",
                ioFileName);
        return(DM_FAILURE);
    }

    /* libmoviefile counts in frames only but can store split fields */
    if ((movie_images = mvGetTrackNumDataIndex(mvImageTrack)) < 0) {
        fprintf(stderr, "no data chunks in movie file\n");
        return(DM_FAILURE);
    }

    /* if we have specified the number of images to be output via cmd line 
     * use that value unless it is > movie_images 
     */
    vlNumImages = ((vlNumImages > movie_images) || (vlNumImages == 0)) ? movie_images : vlNumImages;

    /* check if data format is fields or frames, this is ordinarily called
     * once every field or frame however in our case the format will never
     * change within a given track.  We use this later to determine how to
     * retrieve offsets within the data file.
     */
    if (mvTrackDataHasFieldInfo(mvImageTrack, 0) == DM_TRUE) {
	vlCaptureMode = VL_CAPTURE_NONINTERLEAVED;
	vlNumImages *= 2;
	if (Verbose)
            fprintf(stderr, "data in movie file is stored as split fields\n");
    }
    else {
	vlCaptureMode = VL_CAPTURE_INTERLEAVED;
	if (Verbose)
            fprintf(stderr, "data in movie file is stored as frames\n");
    }

    if (0 == strcmp(mvGetImageCompression(mvImageTrack), DM_IMAGE_RICE)) {
      if (!is_SRV)
	  {
	  vlCompression = VL_COMPRESSION_RICE;
	  vlRiceEnable = 1;
	  vlRicePrecision = VL_RICE_COMP_PRECISION_8;
	  }
	else
	  {
	  fprintf(stderr, "Octane Digital Video does not support RICE encoded files\n");
	  return(DM_FAILURE);
	  }
    }
    else if (0 == strcmp(mvGetImageCompression(mvImageTrack), DM_IMAGE_UNCOMPRESSED)) {
	vlCompression = VL_COMPRESSION_NONE;
    }
    else { 
        fprintf(stderr, "%s only works with uncompressed or Rice encoded data files\n", progName);
        return(DM_FAILURE);
    }

    /*
     * get all of the important parameters from the image track.
     * NOTE: we do not need to look at DM_IMAGE_PIXEL_ASPECT, DM_IMAGE_RATE, nor     * the time scale data as the VL is used to control how the data is played
     * out
     */

    /*
     * set the VL packing to whatever the movie file is set to.
     */
    mvPacking = mvGetImagePacking(mvImageTrack);
    switch (mvPacking) {

	case DM_IMAGE_PACKING_CbYCrY:
#ifndef MVP
	    vlPacking = VL_PACKING_242_8;
#endif
	    vlPacking = VL_PACKING_YVYU_422_8;
            mvBytesPerPixel = 2;
	    if (vlRiceEnable)
	      vlRiceSampling = VL_RICE_COMP_SAMPLING_422;
            break;

	case DM_IMAGE_PACKING_RGB:
#ifndef MVP
	    vlPacking = VL_PACKING_444_8;
#endif
	    vlPacking = VL_PACKING_RGB_8;
            mvBytesPerPixel = 3;
	    if (vlRiceEnable)
	      vlRiceSampling = VL_RICE_COMP_SAMPLING_444;
            break;

	case DM_IMAGE_PACKING_RGBA:
	case DM_IMAGE_PACKING_RGBX:
#ifndef MVP
	    vlPacking = VL_PACKING_4444_8;
#endif
	    vlPacking = VL_PACKING_RGBA_8;
            mvBytesPerPixel = 4;
	    if (vlRiceEnable)
	      vlRiceSampling = VL_RICE_COMP_SAMPLING_4444;
            break;

	case DM_IMAGE_PACKING_ABGR:
	case DM_IMAGE_PACKING_XBGR:
#ifndef MVP
	    vlPacking = VL_PACKING_R4444_8;
#endif
	    vlPacking = VL_PACKING_ABGR_8;
            mvBytesPerPixel = 4;
	    if (vlRiceEnable)
	      vlRiceSampling = VL_RICE_COMP_SAMPLING_4444;
            break;

        default:
            fprintf(stderr, "invalid image packing value %d\n", mvPacking);
            return(DM_FAILURE);
    }

    /* This is not elegant!
     * compute the proper timing factors for movie file parameters and
     * the SMPTE to int conversion, note we must do this before we call
     * read_vlan_cmd_file()
     * NOTE: we are not dealing with square pixels.
     */
    mvYsize = mvGetImageHeight(mvImageTrack);
    mvXsize = mvGetImageWidth(mvImageTrack);

    if ((243 == mvYsize) || (486 == mvYsize)) {
        vlanFramesPerSec  = 30;
        vlanFramesPerMin  = 1800;
        vlanFramesPerHour = 108000;
        /* vlTiming = VL_TIMING_525_SQ_PIX; */
        vlTiming = VL_TIMING_525_CCIR601;
    }
    else if ((288 == mvYsize) || (576 == mvYsize)) {
        vlanFramesPerSec  = 25;
        vlanFramesPerMin  = 1500;
        vlanFramesPerHour = 90000;
        /* vlTiming = VL_TIMING_625_SQ_PIX; */
        vlTiming = VL_TIMING_625_CCIR601;
    }

    mvInterlacing = mvGetImageInterlacing(mvImageTrack);
    mvOrientation = mvGetImageOrientation(mvImageTrack);
    mvBytesPerImage = mvBytesPerPixel * mvXsize *
		      ((vlCaptureMode == VL_CAPTURE_NONINTERLEAVED) ?
			mvYsize >> 1 : mvYsize);

    /* check I/O block size */
    mvGetMovieUserDataListHandle(theMovie, &movie_user_data);
    mvBlockSize = dmParamsGetInt(movie_user_data, MV_IO_BLOCKSIZE);

    if (Verbose)
      {
      if (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED)
	  {
	  fprintf(stderr, "\t%d fields of size: %d x %d x %d\n",
		  movie_images << 1, mvXsize, mvYsize >> 1, mvBytesPerPixel);
	  fprintf(stderr, "\tmvBytesPerImage = %d\n", mvBytesPerImage >> 1);
	  }
	else
	  {
	  fprintf(stderr, "\t%d frames of size: %d x %d x %d\n",
		  movie_images, mvXsize, mvYsize, mvBytesPerPixel);
	  fprintf(stderr, "\tmvBytesPerImage = %d\n", mvBytesPerImage);
	  }
      fprintf(stderr, "\tmvBlockSize = %d\n", mvBlockSize);
      }

    /* check for the presence of an audio track and a 0 length track */
    if (mvFindTrackByMedium(theMovie, DM_AUDIO, &mvAudioTrack) == DM_SUCCESS) {
        fprintf(stderr, "QT embedded audio files are not yet supported!\n");
    }

    return(DM_SUCCESS);
}


static int
open_data_file(void)
{
    long num_pages, io_vec;
    int blocks_per_image;
    struct dioattr da;

    /* NOTE: we now have two file descriptors, one for libmoviefile to read the
     * header and one to read the data in direct I/O mode */
    dataFileDesc = open(ioFileName, O_DIRECT | O_RDONLY);
    if (dataFileDesc < 0) {
	fprintf(stderr, "error opening %s for direct I/O reading\n",ioFileName);
	perror(progName);
	return(DM_FAILURE);
    }

    /* query the block size and memory alignment */
    if (fcntl(dataFileDesc, F_DIOINFO, &da) < 0) {
	perror(progName);
	return(DM_FAILURE);
    }
	  
    ioBlockSize = da.d_miniosz;
    ioMaxXferSize = da.d_maxiosz;
    
    /* page alignment is best for any mode */
    ioAlignment = getpagesize();

    /* NOTE: this is not documented in the man pages of writev or direct I/O!
     * If we are using writev we must align to the pagesize not the file
     * system blocksize which is required for direct I/O.
     * if (blocksize >= pagesize) then we do not have to worry but
     * if (blocksize < pagesize) then we must align the I/O request size
     * to the pagesize not the blocksize.  This will waste space but is
     * necessary to maintain performance.
     */
    if (ioAlignment > ioBlockSize) {
	if (Verbose && Debug)
            fprintf(stderr, "page size (%d) is larger than the file system block size (%d) so the I/O request will be aligned to the page size\n", ioAlignment, ioBlockSize);
	ioBlockSize = ioAlignment;
    }
    if (Debug) {
	assert(ioBlockSize != 0);
    }

    /* compare file system block size with the movie file block size */
    if (mvBlockSize != ioBlockSize) {
        fprintf(stderr, "WARNING: movie file was created with a block size = %d;file system block size = %d\n", mvBlockSize, ioBlockSize);
    }

    /* calculate blocks/field based on blocksize */
    blocks_per_image = (mvBytesPerImage + ioBlockSize - 1) / ioBlockSize;
    ioBytesPerXfer = blocks_per_image * ioBlockSize;

    if (Debug) {
	assert(ioBytesPerXfer != 0);
    }

    /* we must compare the max DMA xfer size with the user's I/O request */
    if (ioBytesPerXfer > ioMaxXferSize) {
	if (Verbose) {
	    fprintf(stderr, "I/O request size = %d exceeds max dma size = %d\n",
				ioBytesPerXfer, ioMaxXferSize);
	}
	return(DM_FAILURE);
    }

    /* 
     * we must also check if the # of I/O vectors requested exceeds the DMA 
     * transfer size 
     */
    if (ioBytesPerXfer * ioVecCount > ioMaxXferSize) {

	/* all values in this equation are multiples of the page size so we
	 * do not lose anything in our integer arithmetic */
	num_pages = (ioBytesPerXfer * ioVecCount) % ioMaxXferSize;
	num_pages /= ioAlignment; 

	/* how many I/O vector will fit into this DMA limit */
	io_vec = ioMaxXferSize/ioBytesPerXfer;

	if (Verbose) {
	    fprintf(stderr, "The # of I/O vectors requested exceeds the max dma size!\n");
	    fprintf(stderr, "\tDo not exceed I/O vector count of %ld\n", 
			io_vec);
	    fprintf(stderr, "\tor increase max dma size by %ld pages\n", 
			num_pages);
	}
	return(DM_FAILURE);
    }

    videoData = (struct iovec *) malloc(ioVecCount * sizeof(struct iovec));

    if (Verbose) {
	fprintf(stderr, "memory alignment is = %d\n", ioAlignment);
	fprintf(stderr, "I/O block size = %d\n", ioBlockSize);
	fprintf(stderr, "blocks per image = %d\n", blocks_per_image);
	fprintf(stderr, "bytes per I/O xfer = %d\n", ioBytesPerXfer);
	fprintf(stderr, "using scatter-gather, count = %d\n", ioVecCount);
    }
    return(DM_SUCCESS);
}


void
video_DIVO_player(void *arg)
{
    int i;
    int image_cnt=vlNumImages;
    int qt_image=0;
    int dmb_index=0;			/* index of the active DM buffer */
    int dmb_free_index=0;		/* index used to free the DM buffer */
    int dmpool_fd;              	/* dmBuffer file descriptor */
    int dmpool_ready;
    int max_fd;
    int imax;
    int index;
    long long free_pool_bytes;
    int free_pool_buffers=0;
    fd_set fdset;
    off64_t file_offset;
    off64_t image_offset;
    size_t image_1_size;
    size_t image_1_pad;
    size_t image_2_size;
    size_t image_2_pad;
    int err_no;
    struct sched_param sched_stuff;


    prctl(PR_TERMCHILD);
    sigset(SIGHUP, cleanup_video);

    /* elevate priority so I/O proc will schedule faster */
    prctl(PR_RESIDENT);
    sched_stuff.sched_priority = 35;
    sched_setscheduler(0, SCHED_FIFO, &sched_stuff);

    if (Verbose)
	fprintf(stderr, "starting the video player\n");
   
    /* get a dmBuffer FD to select on */
    dmpool_fd = dmBufferGetPoolFD(dmPool);
    max_fd = dmpool_fd;

    while (image_cnt) {

	/* check and see if we have any free buffers available */
	if (dmBufferGetPoolState(dmPool, &free_pool_bytes, &free_pool_buffers) != DM_SUCCESS) {
            fprintf(stderr, "%s\n", dmGetError(&err_no, NULL));
	    exit(DM_FAILURE);
 	}
	/* 
	 * since we are using variable size buffers we check against the total
	 * size of all the buffers we need and the # of buffers available  
	 */
	while (free_pool_bytes < (ioVecCount * ioBytesPerXfer) ||
	      free_pool_buffers < (ioVecCount << 1))
	  {
	  if (Verbose && Debug)
	    {
	    fprintf(stderr, "\tfree_pool_bytes : %lld\n", free_pool_bytes);
	    fprintf(stderr, "\tfree_pool_buffers : %d\n", free_pool_buffers);
	    }

	  FD_ZERO(&fdset);
	  FD_SET(dmpool_fd, &fdset);

	  while (select(max_fd+1, NULL, &fdset, NULL, NULL) == 0)
	    fprintf(stderr, "select on dmBuffer pool timeout!\n");

	  /* we must check again for sufficient space in the buffers */
	  if (dmBufferGetPoolState(dmPool, &free_pool_bytes,
				    &free_pool_buffers) == DM_FAILURE)
	    {
	    fprintf(stderr, "no buffers available!\n");
	    fprintf(stderr, "%s\n", dmGetError(&err_no, NULL));
	    exit(DM_FAILURE);
	    }
	  }
	   
	/* now allocate the buffers */
	imax = (image_cnt >= ioVecCount) ? ioVecCount : image_cnt;
	for (i=0; i < imax; ) {

	    index = dmb_index + i;
	    if (dmBufferAllocate(dmPool, &dmBuffers[index]) == DM_SUCCESS) {
		if (Debug)
 		    fprintf(stderr, "got a buffer\n");
	    }
	    else {
		fprintf(stderr, "buffer allocation failed!\n");
		fprintf(stderr, "%s\n", dmGetError(&err_no, NULL));
		exit(DM_FAILURE);
	    }

            /* 
	     * find the size of the next image, if the movie file data is
	     * stored as fields we will grab 2 images at a time.
	     */
            if (read_qt_offset_data(qt_image++,
			      &image_offset,
                              &image_1_size,
                              &image_1_pad,
                              &image_2_size,
                              &image_2_pad) == DM_FAILURE) {
                exit(DM_FAILURE);
            }
	    if (Verbose && Debug)
	      {
	      fprintf(stderr, "image_offset : %lld\n", image_offset);
	      fprintf(stderr, "image_1_size : %d\n", image_1_size);
	      fprintf(stderr, "image_1_pad : %d\n", image_1_pad);
	      fprintf(stderr, "image_2_size : %d\n", image_2_size);
	      fprintf(stderr, "image_2_pad : %d\n", image_2_pad);
	      }


	    /* for readv we need the offset to the first image */
	    if (i == 0)
		file_offset = image_offset;

	    if (dmBufferSetSize(dmBuffers[index], image_1_size) == DM_FAILURE)
	      {
	      fprintf(stderr, "set size of buffer failed!\n");
	      fprintf(stderr, "%s\n", dmGetError(&err_no, NULL));
	      exit(DM_FAILURE);
	      }
	    videoData[i].iov_base = dmBufferMapData(dmBuffers[index]);
	    videoData[i].iov_len  = image_1_size + image_1_pad;
	    if (Verbose && Debug)
	      {
	      fprintf(stderr, "videoData[i].iov_base : %lld\n",
		      videoData[i].iov_base);
	      fprintf(stderr, "videoData[i].iov_len : %lld\n",
		      videoData[i].iov_len);
	      }
	    i++;
	    index++;

	    /* if this image has field data then we must map another buffer */
	    if (image_2_size != 0) {
		if (dmBufferAllocate(dmPool, &dmBuffers[index]) == DM_SUCCESS) {
		    if (Debug)
			fprintf(stderr, "got second field buffer\n");
		}
		else {
		    fprintf(stderr, "second field buffer allocation failed!\n");
		    fprintf(stderr, "%s\n", dmGetError(&err_no, NULL));
		    exit(DM_FAILURE);
		}

		if (dmBufferSetSize(dmBuffers[index], image_2_size) == DM_FAILURE)
		  {
		  fprintf(stderr, "set size of second field buffer failed!\n");
		  fprintf(stderr, "%s\n", dmGetError(&err_no, NULL));
		  exit(DM_FAILURE);
		  }
		videoData[i].iov_base = dmBufferMapData(dmBuffers[index]);
		videoData[i].iov_len  = image_2_size + image_2_pad;
		if (Verbose && Debug)
		  {
		  fprintf(stderr, "(field 2) videoData[i].iov_base : %lld\n",
			  videoData[i].iov_base);
		  fprintf(stderr, "(field 2) videoData[i].iov_len : %lld\n",
			  videoData[i].iov_len);
		  }
		i++;
	    }
	}

	if (vlBenchmark)
	    gettimeofday(&BeginTime, NULL);

	if (err_no = lseek64(dataFileDesc, file_offset, SEEK_SET) != file_offset) {
	    fprintf(stderr, "lseek error!\n");
	    exit(DM_FAILURE);
	}

	if (err_no = readv(dataFileDesc, videoData, ioVecCount) < 0) {
	    fprintf(stderr, "readv error!\n");
	    exit(DM_FAILURE);
	}

	for (i=0; i<imax; i++)
	  {
	  index = dmb_index + i;
	  /* we cannot have more than 100 queued in the dmbuffer fifo */
	  while (vlGetFilledByNode(vlServer, vlPath, vlMemNode) >
		  DMBUFFERS_MAX_QUEUE)
	    sginap(5);

	  if (vlDMBufferPutValid(vlServer, vlPath, vlMemNode,
				  dmBuffers[index]) == DM_FAILURE)
	    {
	    fprintf(stderr, "could not output valid buffer!\n");
	    exit(DM_FAILURE);
	    }

	  dmBufferFree(dmBuffers[index]);

	  /*	incr the semaphore - i.e. the # frames in the buffer pool */
	  usvsema(videoAvailSema);
	  }

	/* increment the buffer index for the next image */
	dmb_index = (dmb_index+imax) % dmBufferPoolSize;
	image_cnt -= imax;

	/* note that the benckmark time includes all of the buffer calls */
	if (vlBenchmark) {
		gettimeofday(&EndTime, NULL);
		DiskTransferTime += (
		((double) EndTime.tv_usec/DISK_BYTES_PER_MB) +
		EndTime.tv_sec) - (
		((double) BeginTime.tv_usec/DISK_BYTES_PER_MB)+
		BeginTime.tv_sec);
	}

#if VIDEO_SEMA
	usvsema(videoStopSema);
#endif
    }

    /* tack on some black frames so the semaphore count works out right */
    if (!vlRiceEnable)
      for (i = 0; i < ioVecCount; i++)
	{
	if (dmBufferAllocate(dmPool, &dmBuffers[dmb_index + i]) == DM_FAILURE)
	  {
	  fprintf(stderr, "buffer allocation failed!\n");
	  fprintf(stderr, "%s\n", dmGetError(&err_no, NULL));
	  exit(DM_FAILURE);
	  }
	bzero(dmBufferMapData(dmBuffers[dmb_index + i]),
			      dmBufferGetSize(dmBuffers[dmb_index + i]));

	if (vlDMBufferPutValid(vlServer, vlPath, vlMemNode, 
		  dmBuffers[dmb_index + i]) == DM_FAILURE)
	  {
	  fprintf(stderr, "could not output valid buffer!\n");
	  exit(DM_FAILURE);
	  }
	dmBufferFree(dmBuffers[dmb_index + i]);

	/* incr the semaphore - i.e. the # frames in the buffer pool */
	usvsema(videoAvailSema);
	}

    /*	output benchmark data */

    exit(DM_SUCCESS);
}


/* This routine finds the byte offset of the frame in the file from the image 
 * index.  For fields it also gets the size of each field and the gap between
 * them. 
 * NOTE: for Rice encoding fields will be of variable size, in fact this 
 * routine is only necessary for Rice data.
 */ 
static int
read_qt_offset_data(int index,
		    off64_t *offset_f1,
		    size_t *size_f1,
		    size_t *pad_f1,
		    size_t *size_f2,
		    size_t *pad_f2)
{
    off64_t offset_f2;
    off64_t gap;
    MVdatatype data_type;
    MVframe num_frames;
    int param_id;
    size_t xfer_size;
    size_t xfer_size2;
    off64_t next_offset;

    /* Since we are not using libmovieplay to playback the file we do not need
     * mvGetTrackTimeScale, mvGetTrackBoundary, and mvGetTrackDataIndexAtTime.
     * We could also get the offset by using mvGetTrackDataIndexAtTime.
     * The library only returns the absolute size of the image data.  We must
     * compute the actual transfer size. 
     */

    /* Get the offset, we always return the offset of the first image */
    if (mvGetTrackDataOffset(mvImageTrack, index, offset_f1) == DM_FAILURE){
        fprintf(stderr, "%s\n", mvGetErrorStr(mvGetErrno()));
        return(DM_FAILURE);
    }

    /* Get the frame size */
    if (mvGetTrackDataInfo(mvImageTrack, index, size_f1, &param_id, &data_type,
			    &num_frames) == DM_FAILURE)
      {
      fprintf(stderr, "%s\n", mvGetErrorStr(mvGetErrno()));
      return(DM_FAILURE);
      }

    /* Do we have split fields, i.e. two fields stored within one frame index */
    if (mvTrackDataHasFieldInfo(mvImageTrack, index) == DM_TRUE) {
        /* it is possible for mixed (fields and frames) to exist in the file
         * although this will be very unlikely, remove if this call introduces
         * any latency.
         */
        if (mvGetTrackDataFieldInfo(mvImageTrack, index, size_f1, &gap, size_f2) == DM_FAILURE) {
                fprintf(stderr, "%s\n", mvGetErrorStr(mvGetErrno()));
                return(DM_FAILURE);
        }
	*pad_f1 = gap;
	xfer_size = *size_f1 + gap;
	offset_f2 = *offset_f1 + xfer_size;
	xfer_size2 = ((*size_f2 + mvBlockSize - 1) / mvBlockSize) * mvBlockSize;
	*pad_f2 = xfer_size2 - *size_f2;

        if (Verbose && Debug) {
            fprintf(stderr, "\toffset of first field = %lld \n", *offset_f1);
            fprintf(stderr, "\tsize of first field = %ld bytes\n", *size_f1);
            fprintf(stderr, "\txfer size of first field = %ld bytes\n", xfer_size);
            fprintf(stderr, "\tfield gap = %lld\n", gap);
            fprintf(stderr, "\toffset of second field = %lld \n", offset_f2);
            fprintf(stderr, "\tsize of second field = %ld bytes\n", *size_f2);
            fprintf(stderr, "\txfer size of second field = %ld bytes\n", xfer_size2);
#if DEBUG
	    next_offset = offset_f2 + (off64_t) *size_f2 + (off64_t)*pad_f2;
            fprintf(stderr, "offset of next image = %lld bytes\n", next_offset);
#endif
        }
    }
    else {
	xfer_size = ((*size_f1 + mvBlockSize - 1) / mvBlockSize) * mvBlockSize;
	*pad_f1 = xfer_size - *size_f1;
	if (Verbose && Debug) {
            fprintf(stderr, "\toffset of frame = %lld \n", *offset_f1);
	    fprintf(stderr, "\tsize = %ld\n", *size_f1);
            fprintf(stderr, "\txfer size of frame = %ld bytes\n", xfer_size);
#if DEBUG
	    next_offset = *offset_f1 + (off64_t) *size_f1;
            fprintf(stderr, "offset of next image = %lld bytes\n", next_offset);
#endif
	}
    }

    return(DM_SUCCESS);
}


static int
read_vlan_cmd_file(void)
{
    char vlanCmdBuffer[VLAN_CMD_BUF_SIZE];
    char vlanCommand[VLAN_CMD_SIZE];
    char vlanCmdParam[VLAN_PARAM_SIZE];
    unsigned int startTime, finishTime;

    /* parse the file until we reach EOF or EOE (end of edit) */ 
    while (fgets(vlanCmdBuffer, VLAN_CMD_BUF_SIZE, vlanFileDesc) != NULL) {
	/* check for \n (empty lines) and break if true */
	/* replace the \n with \0 */
	vlanCmdBuffer[strlen(vlanCmdBuffer) - 1] = '\0';

#if 0
	/* the commands should be issued in the proper order, if not the review
	 * or perform command may be issued prematurely.  We protect the user
	 * from this by delaying.  We send the command after reading the file. 
	 */
 	if (!strcmp(vlanCmdBuffer, "RV") || !strcmp(vlanCmdBuffer, "PF")) {
	    continue;
	}
	else {
	    /* send the command and then check the response */
	    if (sendVLANCmd(vlanCmdBuffer) == DM_FAILURE)
		return(DM_FAILURE);
	}
#endif

	if (sendVLANCmd(vlanCmdBuffer) == DM_FAILURE)
	    return(DM_FAILURE);

        /* separate the command and timecode, note the white space between the
	 * command and timecode is copied into vlanCommand */
        sscanf(vlanCmdBuffer, "%3s%s", vlanCommand, vlanCmdParam);

 	if (!strcmp(vlanCommand, "SI")) {  /* set in-point */

	    startTime = SMPTE_to_INT(vlanCmdParam);
	    vlanINTInPoint = startTime;
	    if (Verbose)
    		fprintf(stderr, "start edit at %s\n", vlanCmdParam); 

	    if (Debug) {
    		if (sendVLANCmd("RI") == DM_FAILURE) {/* check in-point */
    		    fprintf(stderr, "Could not read in-point\n");
		    return(DM_FAILURE);
		}
	    }
	}
 	else if (!strcmp(vlanCommand, "SO")) {  /* set out-point */

	    finishTime = SMPTE_to_INT(vlanCmdParam);
	    if (Verbose)
    		fprintf(stderr, "stop edit at %s\n", vlanCmdParam);

	    if (Debug) {
		if (sendVLANCmd("RO") == DM_FAILURE) {/* check out-point */
    		    fprintf(stderr, "Could not read out-point\n");
		    return(DM_FAILURE);
		}
	    }
	}
 	else if (!strcmp(vlanCommand, "SD")) { /* set duration */

	    finishTime = SMPTE_to_INT(vlanCmdParam);
	    if (Verbose)
    		fprintf(stderr, "edit duration %s\n", vlanCmdParam);

	    if (Debug) {
		if (sendVLANCmd("RO") == DM_FAILURE) {/* check out-point */
    		    fprintf(stderr, "Could not read out-point\n");
		    return(DM_FAILURE);
		}
	    }
	}
 	else if (!strcmp(vlanCommand, "CO")) { /* turn coincidence on */
	    
	    vlanCoincidence = 1;
	    if (Verbose)
    		fprintf(stderr, "coincidence pulse enabled\n");

	    if (Debug) {
		if (sendVLANCmd("RO") == DM_FAILURE) {/* verify coincidence on*/
    		    fprintf(stderr, "Could not verify coincidence\n");
		    return(DM_FAILURE);
		}
	    }
	}
    }

/* DEBUG: THIS IS BUSTED!!!*/
    /* calculate the number of fields from the time code */
    if (finishTime < startTime) {
	if (Verbose)
    	    fprintf(stderr, "in-point is later than out-point\n");
	return(DM_FAILURE);
    }
    else {
	if (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED)
	    vlNumImages = 2 * (finishTime - startTime);
	else
	    vlNumImages = finishTime - startTime;
    }

    return(DM_SUCCESS);
}


static unsigned int
SMPTE_to_INT(char *SMPTEtc)
{
    unsigned int hh, mm, ss, ff;

    /* 
     * Check time format for No Drop Frame and Drop Frame modes,
     * NOTE: this only checks the string it does not compute frame duration
     * properly.
     */
    if ((sscanf (SMPTEtc, "%u:%u:%u:%u",&hh, &mm, &ss, &ff) != 4) && 
	(sscanf (SMPTEtc, "%u:%u:%u.%u",&hh, &mm, &ss, &ff) != 4)) {
	fprintf (stderr, "\nImproper SMPTE time format: %s\n\n", SMPTEtc);
	exit(DM_FAILURE);
    }
    if ((hh > 23) || (mm > 59) || (ss > 59) || (ff > (vlanFramesPerSec-1))) {
	fprintf (stderr, "\nImproper value in SMPTE time value: %s\n\n", SMPTEtc);
	exit(DM_FAILURE);
    }
    /* calculate time in FRAMES */
    return(hh * vlanFramesPerHour + mm * vlanFramesPerMin + ss * vlanFramesPerSec + ff);
}


static char * 
INT_to_SMPTE (unsigned int timeCode)
{
    unsigned int hh, mm, ss, ff; 
    static char tmp[VLAN_CMD_BUF_SIZE];

    hh        = timeCode / vlanFramesPerHour; 
    timeCode -= (hh * vlanFramesPerHour); 
    mm        = timeCode / vlanFramesPerMin; 
    timeCode -= (mm * vlanFramesPerMin); 
    ss        = timeCode / vlanFramesPerSec; 
    ff        = timeCode -= (ss * vlanFramesPerSec); 
    sprintf(tmp, "%02d:%02d:%02d:%02d", hh, mm, ss, ff); 
    return(tmp); 
}


/* this is the version of the V-LAN command dispatch function */ 
static int 
sendVLANCmd(char * vlanCmd)
{
    int retry;

    for (retry = 0; retry < VLAN_RETRY; retry++) {
	if ((vlanResp = vlan_cmd(vlanDevice, vlanCmd)) == (char*) NULL) {
	    vlan_perror(progName);
	    continue;
	}
	else /* successful issue of command */ {
	    if (Verbose)
        	fprintf(stderr, "V-LAN command: %s\n", vlanCmd);
	    if (Debug)
	        fprintf(stderr, "V-LAN Response = %s\n", vlanResp);
	    return(DM_SUCCESS);
	}
    }

    if (Verbose && Debug)
	fprintf(stderr,"retried V-LAN cmd [%s] %d times\n", vlanCmd, retry);

    return(DM_FAILURE);
}


/* DEBUG, BM does not include the audio processing */
static void
benchmark_results(void)
{
    /* remember that the number of bytes per field is rounded up to the nearest
     * block size for the actual I/O transfer size
     */
    long images_xfered = vlXferCount - vlDropCount;
    long total_bytes = ioBytesPerXfer * images_xfered;
    double total_megabytes = (double) total_bytes / DISK_BYTES_PER_MB;
    double megabyte_bandwidth = (double) ioBytesPerXfer / DISK_BYTES_PER_MB;
    int rate;

    if ((total_bytes <= 0) || (images_xfered <= 0)) {
        fprintf(stderr, "cannot compute benchmark results\n");
	return;
    }

    rate = (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED) ? 2 : 1;

    switch (vlTiming) {
        case VL_TIMING_525_CCIR601:
        case VL_TIMING_525_SQ_PIX:
            megabyte_bandwidth *= (30 * rate);
            break;
        case VL_TIMING_625_CCIR601:
        case VL_TIMING_625_SQ_PIX:
            megabyte_bandwidth *= (25 * rate);
            break;
        default:
            fprintf(stderr, "cannot compute benchmark results, invalid timing\n");
            return;
    }

    fprintf(stderr,
        "\n\n===================== BENCHMARK RESULTS ====================\n");
    fprintf(stderr, "\t# of fields transfered = %d\n", images_xfered);
    fprintf(stderr, "\t# of fields dropped = %d\n", vlDropCount);
    fprintf(stderr, "\tMinimum required disk transfer rate = %2.3lf MB/s\n",
        megabyte_bandwidth);
    fprintf(stderr, "\tI/O transfer size = %d bytes\n", ioBytesPerXfer);
    fprintf(stderr, "\tTotal data volume = %2.3ld MB\n", total_megabytes);
    fprintf(stderr, "\tTotal transfer time = %f sec.\n", DiskTransferTime);
    fprintf(stderr, "\tActual disk transfer rate = %2.3f MB/s\n",
        total_megabytes/DiskTransferTime);
}

