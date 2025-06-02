/*
 * avcapture: record video and audio to a QuickTime disk file.
 * The movie library and SGI QT extensions are used to record audio and video
 * synchronously in realtime.
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
#include <unistd.h>
#include <sys/syssgi.h>
#include <sys/time.h>
/* for scatter-gather */
#include <sys/uio.h>

#include <dmedia/vl.h>

#define EVO TRUE
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
/*
 * to work around no support for VL_TRIGGER_VITC
 */
#define EMUL_VITC_TRIG TRUE

/*
 * to work around DIVO prob with RICE in FRAME mode
 */
#define NO_RICE_FRAMES 0

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
static char *progName = "avcapture";
static int Verbose = FALSE;
static int Debug = FALSE;

/*
 * globals for semaphores and mp threads
 */
#define ANYKID -1

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

#define DEFAULT_IMAGE_COUNT 30
#define IOVEC_SIZE	4

typedef struct _ioVecParams* ioVecParams_p;

typedef struct _ioVecParams
  {
  off64_t   offset;
  long	    image_size;
  long	    aligned_size;
  int	    index;
  } ioVecParams_t;

static char *ioFileName = "videodata";
static int ioFileFD = 0;
static struct iovec *videoData = NULL;
static ioVecParams_p imageData = NULL;
static long ioVecCount=IOVEC_SIZE;
static long ioVecCountMax;
static double total_bytes = 0;
static int images_xfered = 0;

/* 
 * ioBlockSize and ioAlignment are extremely important to the functionality and
 * performance of this utility.  Their values depend on the type of I/O
 * used.  There are three methods of writing to the disk: 1) raw device via
 * syssgi(); 2) standard write with XFS; 3) writev/readv.
 */  
#define DISK_BLOCK_SIZE 	512
#define NANOS_PER_SEC   	1e9
#define DISK_BYTES_PER_MB	1000000.0F
/* this should be in a sys header file but it is not */
static int ioBlockSize = DISK_BLOCK_SIZE;
static int ioAlignment;
static int ioMaxXferSize = 0;
static int maxBytesPerXfer=0;

/* 
 * benchmark timing parameters 
 */
static struct timeval BeginTime, EndTime;
static double DiskTransferTime = 0;

/* 
 * functions for disk I/O
 */
static int write_qt_offset_data(int, int);
static int write_qt_file_header(void);
static int initialize_file_data(void);
static int initialize_dmbuffers(void);
static int initialize_movie_file(MVid *);

/*
 * globals for dmbuffers
 */
static DMbuffer * dmBuffers;
static int dmBufferPoolSize = DEFAULT_IMAGE_COUNT;
static DMbufferpool dmPool;

/*
 * globals for video
 */
static VLServer vlServer;
static VLDevList vlDeviceList;
static VLDevice *vlDeviceInfo;
static VLNode vlVidNode, vlMemNode, vlDevNode;
static int vlVidNodeNum = VL_ANY;
static int vlMemNodeNum = VL_ANY;
static VLPath vlPath;
static VLTransferDescriptor vlXferDesc;
static int vlBytesPerPixel = 0;
static int vlXsize = 0, vlYsize = 0;
static int vlBytesPerImage = 0;
static int vlDevice = 0;
#if SRV_FICUS_COMPAT
static int isSRV = 0;
static int isEVO = 0;
#endif
static int vlCompatMode = 0;
static int vlFormat = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
#ifdef MVP
static int vlPacking = VL_PACKING_YVYU_422_8;
#else
static int vlPacking = VL_PACKING_R242_8;
#endif
static int vlColorSpace = VL_COLORSPACE_CCIR601;
static int vlRicePrecision = 0;		/* if = 0 we do not do compression */
static int vlRiceSampling = 0;
static int vlRiceEnable = 0;
static int vlTiming = VL_TIMING_525_CCIR601;
static int vlVITC = 0;
static int vlshowVITC = 0;
static int vlVITCsync = 0;
static int cur_tc = 0;
static int vlGPI = 0;
static int vlFieldDominance = -1;
static int vlNumImages = -1;
static int vlSkipImages = 0;
static int vlPadImages = 0;
static int vlCaptureMode = VL_CAPTURE_INTERLEAVED; 	/* frames */
static int vlXferCount = 0;
static int vlDropCount = 0;
static int vlAbortOnSequenceLost = 0;
static int vlBenchmark = 0;
static USTMSCpair vlFirstUSTMSC;	/* UST/MSC of a reference video field */

/* 
 * functions for video  
 */
static int initialize_video(void);
static void cleanup_video(void);
static void (* video_recorder)();
static void video_DIVO_recorder(void *);
static void video_MVP_recorder(void *);
static int getVITC(DMbuffer);

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
static int	alFileFormat = 0;	/* default is no audio */
static AFfilehandle aflAudioFile;       /* handle to audio file */
static ALconfig alPortConfig;		/* handle to audio port */
static ALport   alAudioPort = 0;
static int      alSampleBufferSize;     /* samples / sample buffer */
static int      alFrameBufferSize;      /* frames / sample buffer */
static char     *alSampleBuffer;        /* sample buffer */
static stamp_t  alUSTPerMSC;
static long 	alSampleRate = 44100;	/* audio input sample rate */
static long     alSamplesPerFrame = 2;	/* ASO only supports two channels */
static volatile int alInterrupt=0;      /* flag set when audio ends */
/* hack until DIVO support UST/MSC */

/*
 * defines for audio
 */
#define NO_AUDIO	0
#define QUICKTIME_AUDIO	1
#define AIFF_AUDIO	2

/* functions for audio */
static int initialize_audio(void);
static void audio_recorder(void *);
static void audio_interrupt(void);
static void cleanup_audio(void);

/*
 * globals for movie parameters
 */
typedef enum _captureScheme
{
    frames,
    fields,
    unknown
} captureScheme;


/* 
 * time scale for libmovie files. 
 * large NTSC value is used to generate true refresh value, i.e.
 * MV_IMAGE_TIME_SCALE_NTSC/mvFrameTime; 30000/1001 = 29.97003
 */
#define MV_IMAGE_TIME_SCALE_NTSC 30000	
#define MV_IMAGE_TIME_SCALE_PAL  25
#define MV_IO_BLOCKSIZE	 	"DM_IMAGE_BLOCKSIZE"

static MVfileformat mvFormat = MV_FORMAT_QT;
static DMpacking mvPacking = DM_IMAGE_PACKING_CbYCrY;

/* 
 * setup defaults for capturing frames
 */
static DMinterlacing mvInterlacing = DM_IMAGE_NONINTERLACED;
static captureScheme mvCaptureScheme = frames;
static int mvSwapFieldDominance;
static int mvFrameTime = 1001;  /* for NTSC */
static MVtimescale mvImageTimeScale=MV_IMAGE_TIME_SCALE_NTSC;
static MVtimescale mvAudioTimeScale=48000;
static int mvXsize;
static int mvYsize;
static double mvRate = 29.97;
static double mvAspectRatio = 1.0;
static MVid theMovie;
static MVid mvImageTrack;
static MVid mvAudioTrack;
static off64_t mvFieldGap;
static off64_t mvFileOffset=0;
static int setUpImageTrack(MVid);
static int setUpAudioTrack(MVid);
static void setCaptureScheme(char *);
static char * getCaptureScheme(void);
static void setMovieFormat(char *);

/* 
 * variables and defines for VLAN control 
 */
#define VLAN_RETRY 5
#define VLAN_CMD_BUF_SIZE 80
#define VLAN_EDIT_CMD "RV"

/* 
 * allow for longest V-LAN command string, i.e. "SI -00:00:00:00\0"
 */
#define VLAN_TIMING_NTSC 0
#define VLAN_TIMING_PAL 1
#define VLAN_CMD_SIZE 3
#define VLAN_PARAM_SIZE 13
				
static VLAN_DEV * vlanDevice = NULL;
static FILE * vlanFileDesc;
static unsigned int vlanFramesPerSec = 30;
static unsigned int vlanFramesPerMin = 1800;
static unsigned int vlanFramesPerHour = 108000;
static int vlanTiming = VLAN_TIMING_NTSC;
static char *vlanTTY = NULL;
static char * vlanCmdFile = NULL;
static char * vlanResp = NULL;
static int vlanCoincidence = 0;
static int vlanINTInPoint = -1;
static int vlanINTOutPoint = -1;
static char * vlanInPoint = NULL;
static char * vlanOutPoint = NULL;
static char * vlanDuration = NULL;

/* 
 * V-LAN functions 
 */
/* extern void vlan_read_byte(VLAN_DEV *, char *); */
static unsigned int SMPTE_to_INT(char *);
static unsigned int SMPTE_to_DMTC(char *, DMtimecode *);
static char * INT_to_SMPTE(unsigned int);
static int read_vlan_cmd_file(void);
static int sendVLANCmd(char *);

#define RANGE(X, MIN, MAX) ((X < MIN) || (X > MAX))

int
main(int ac, char **av)
{
    int c, tc_required, have_tc;
    extern char *optarg;
    extern int optind;
    int video_recorder_pid=NULL, audio_recorder_pid=NULL;
    int dead_pid=NULL, dead_stat=0;
    char filename[128], *strpos=NULL;

    /*
     * Flags and arguments
     */
    progName = av[0];

    while ((c = getopt(ac, av, "a:b:c:d:fg:hi:o:p:r:st:v:ABDFG:I:L:O:P:RSTV")) != EOF) {
	switch (c) {
	case 'a':
            alFileFormat = atoi(optarg);
            switch (alFileFormat) {
                case 1:
                    alFileFormat = NO_AUDIO;
                    break;
                case 2:
                    alFileFormat = QUICKTIME_AUDIO;
                    break;
                case 3:
                    alFileFormat = AIFF_AUDIO;
                    break;
                default:
		    fprintf(stderr, "illegal audio format specified\n");
                    usage();
                    break;
            }
	    break;
	case 'b':
	    dmBufferPoolSize = atoi(optarg);
	    break;
	case 'c':
#ifndef MVP
	    vlColorSpace = atoi(optarg);
	    switch (vlColorSpace) {
		case 1:
                    vlColorSpace = VL_COLORSPACE_CCIR601;
                    break;
		case 2:
                    vlColorSpace = VL_COLORSPACE_YUV;
                    break;
		case 3:
                    vlColorSpace = VL_COLORSPACE_RP175;
                    break;
		case 4:
                    vlColorSpace = VL_COLORSPACE_RGB;
                    break;
		default:
		    fprintf(stderr, "illegal colorspace specified\n");
		    usage();
                    break;
	    }
#endif
	    break;
	case 'd':
	    vlDevice = atoi(optarg);
	    break;
	case 'f':
	    setCaptureScheme("fields");
	    break;
	case 'g':
	    ioVecCount = atoi(optarg);
	    break;
	case 'i':
	    if (vlanDuration)
	      {
	      fprintf(stderr,"can't specify duration and image count\n");
	      exit(DM_FAILURE);
	      }
	    if (vlanInPoint && vlanOutPoint)
	      {
	      fprintf(stderr,"can't specify In point, Out point and image count\n");
	      exit(DM_FAILURE);
	      }
	    vlNumImages = atoi(optarg);
	    vlanDuration = strdup(INT_to_SMPTE(vlNumImages));
	    if (vlanInPoint)
	      {
	      vlanINTOutPoint = vlanINTInPoint + vlNumImages;
	      vlanOutPoint = strdup(INT_to_SMPTE(vlanINTOutPoint));
	      if (Debug)
		{
		fprintf(stderr, "Calculated values %s from %s\n",
		    "vlanDuration, vlanIntOutPoint & vlanOutPoint",
		    "vlNumImages & vlanIntInPoint");
		fprintf(stderr, "\t%s  [%s], %s  [%d]\n",
		    "vlanInPoint", vlanInPoint, "vlanINTInPoint", vlanINTInPoint);
		fprintf(stderr, "\t%s [%s], %s     [%d]\n",
		    "vlanDuration", vlanDuration, "vlNumImages", vlNumImages);
		fprintf(stderr, "\t%s [%s], %s [%d]\n",
		    "vlanOutPoint", vlanOutPoint, "vlanINTOutPoint", vlanINTOutPoint);
		}
	      break;
	      }
	    if (vlanOutPoint)
	      {
	      vlanINTInPoint = vlanINTOutPoint - vlNumImages;
	      vlanInPoint = strdup(INT_to_SMPTE(vlanINTInPoint));
	      if (Debug)
		{
		fprintf(stderr, "Calculated values %s from %s\n",
		    "vlanDuration, vlanIntInPoint & vlanInPoint",
		    "vlNumImages & vlanIntOutPoint");
		fprintf(stderr, "\t%s  [%s], %s  [%d]\n",
		    "vlanInPoint", vlanInPoint, "vlanINTInPoint", vlanINTInPoint);
		fprintf(stderr, "\t%s [%s], %s     [%d]\n",
		    "vlanDuration", vlanDuration, "vlNumImages", vlNumImages);
		fprintf(stderr, "\t%s [%s], %s [%d]\n",
		    "vlanOutPoint", vlanOutPoint, "vlanINTOutPoint", vlanINTOutPoint);
		}
	      }
	    break;
	case 'o':
	    mvFileOffset = strtoll(optarg, NULL, 10);
	    fprintf(stderr, "starting offset = %lld\n", mvFileOffset);
	    break;
	case 'p':
/*
 * NOTE to myself: clean up this -p mess for the 6.5 release!!!
 */
#ifdef MVP
            vlPacking = atoi(optarg);
            switch (vlPacking) {
                case 1:
                    vlPacking = VL_PACKING_YVYU_422_8;
                    break;
                case 2:
                    vlPacking = VL_PACKING_RGB_8_P;
                    break;
                case 3:
                    vlPacking = VL_PACKING_RGBA_8;
                    break;
#if TEN_BIT_SUPT
                case 4:
                    vlPacking = VL_PACKING_YVYU_422_10;
                    break;
                case 5:
                    vlPacking = VL_PACKING_RGB_10;
                    break;
                case 6:
                    vlPacking = VL_PACKING_RGBA_10;
                    break;
#endif
                default:
                    fprintf(stderr, "illegal packing specified\n");
                    usage();
                    break;
            }
#endif

#ifdef DIVO
	    /* Rice sampling is a subset of packing for uncompressed so we
	     * set it at the same time */
	    vlPacking = atoi(optarg);
	    switch (vlPacking) {
		case 1:
                    vlPacking = VL_PACKING_R242_8;
		    vlRiceSampling = VL_RICE_COMP_SAMPLING_422;
                    vlRicePrecision = VL_RICE_COMP_PRECISION_8;
                    break;
		case 2:
                    vlPacking = VL_PACKING_242_8;
		    vlRiceSampling = VL_RICE_COMP_SAMPLING_422;
                    vlRicePrecision = VL_RICE_COMP_PRECISION_8;
                    break;
		case 3:
                    vlPacking = VL_PACKING_444_8;
		    vlRiceSampling = VL_RICE_COMP_SAMPLING_444;
                    vlRicePrecision = VL_RICE_COMP_PRECISION_8;
                    break;
		case 4:
                    vlPacking = VL_PACKING_R444_8;
		    vlRiceSampling = VL_RICE_COMP_SAMPLING_444;
                    vlRicePrecision = VL_RICE_COMP_PRECISION_8;
                    break;
		case 5:
                    vlPacking = VL_PACKING_4444_8;
		    vlRiceSampling = VL_RICE_COMP_SAMPLING_4444;
                    vlRicePrecision = VL_RICE_COMP_PRECISION_8;
                    break;
		case 6:
                    vlPacking = VL_PACKING_R4444_8;
		    vlRiceSampling = VL_RICE_COMP_SAMPLING_4444;
                    vlRicePrecision = VL_RICE_COMP_PRECISION_8;
                    break;
#if TEN_BIT_SUPT
		case 7:
                    vlPacking = VL_PACKING_R242_10;
		    vlRiceSampling = VL_RICE_COMP_SAMPLING_422;
                    vlRicePrecision = VL_RICE_COMP_PRECISION_10;
                    break;
		case 8:
                    vlPacking = VL_PACKING_242_10;
		    vlRiceSampling = VL_RICE_COMP_SAMPLING_422;
                    vlRicePrecision = VL_RICE_COMP_PRECISION_10;
                    break;
		case 9:
                    vlPacking = VL_PACKING_R2424_10_10_10_2Z;
		    vlRiceSampling = VL_RICE_COMP_SAMPLING_4224;
                    vlRicePrecision = VL_RICE_COMP_PRECISION_10;
                    break;
		case 10:
                    vlPacking = VL_PACKING_2424_10_10_10_2Z;
		    vlRiceSampling = VL_RICE_COMP_SAMPLING_4224;
                    vlRicePrecision = VL_RICE_COMP_PRECISION_10;
                    break;
		case 11:
                    vlPacking = VL_PACKING_242_10_in_16_L;
                    break;
#endif
		default:
		    fprintf(stderr, "illegal packing specified\n");
		    usage();
                    break;
	    }
#endif
	    break;
	case 'r':
	    vlanTiming = atoi(optarg) % 2;
	    if (!vlanTiming)
		{
		vlanFramesPerSec  = 30;
		vlanFramesPerMin  = 1800;
		vlanFramesPerHour = 108000;
		}
	      else
		{
		vlanFramesPerSec  = 25;
		vlanFramesPerMin  = 1500;
		vlanFramesPerHour = 90000;
		}
	    break;
	case 's':
	    vlshowVITC = 1;
	    break;
	case 't':
	    vlanTTY = optarg;
	    break;
	case 'v':
	    vlanCmdFile = optarg;
	    break;
	case 'A':
	    vlAbortOnSequenceLost = 1;
	    break;
	case 'B':
	    vlBenchmark = 1;
	    break;
	case 'D':
	    Debug = 1;
	    break;
	case 'F':
	    mvSwapFieldDominance = 1;
	    break;
	case 'G':
	    vlGPI = atoi(optarg);
	    if (RANGE(vlGPI, 1, 2))
	      {
	      fprintf(stderr, "Invalid GPI trigger # %d\n", vlGPI);
	      exit(DM_FAILURE);
	      }
	    break;
	case 'I':
	    if (vlanDuration && vlanOutPoint)
	      {
	      fprintf(stderr,"can't specify In point, Out point and duration (or image count)\n");
	      exit(DM_FAILURE);
	      }
	    vlanInPoint = optarg;
	    /* make sure it's valid - converter bails if not */
	    vlanINTInPoint = SMPTE_to_INT(vlanInPoint);
	    if (vlanDuration)
	      {
	      vlanINTOutPoint = vlanINTInPoint + vlNumImages;
	      vlanOutPoint = strdup(INT_to_SMPTE(vlanINTOutPoint));
	      if (Debug)
		{
		fprintf(stderr, "Calculated values %s from %s\n",
		    "vlanIntInPoint, vlanIntOutPoint & vlanOutPoint",
		    "vlanDuration & vlanInPoint");
		fprintf(stderr, "\t%s  [%s], %s  [%d]\n",
		    "vlanInPoint", vlanInPoint, "vlanINTInPoint", vlanINTInPoint);
		fprintf(stderr, "\t%s [%s], %s     [%d]\n",
		    "vlanDuration", vlanDuration, "vlNumImages", vlNumImages);
		fprintf(stderr, "\t%s [%s], %s [%d]\n",
		    "vlanOutPoint", vlanOutPoint, "vlanINTOutPoint", vlanINTOutPoint);
		}
	      break;
	      }
	    if (vlanOutPoint)
	      {
	      vlNumImages = vlanINTOutPoint - vlanINTInPoint;
	      vlanDuration = strdup(INT_to_SMPTE(vlNumImages));
	      if (Debug)
		{
		fprintf(stderr, "Calculated values %s from %s\n",
		    "vlNumImages, vlanIntInPoint & vlanDuration",
		    "vlanInPoint & vlanIntOutPoint");
		fprintf(stderr, "\t%s  [%s], %s  [%d]\n",
		    "vlanInPoint", vlanInPoint, "vlanINTInPoint", vlanINTInPoint);
		fprintf(stderr, "\t%s [%s], %s     [%d]\n",
		    "vlanDuration", vlanDuration, "vlNumImages", vlNumImages);
		fprintf(stderr, "\t%s [%s], %s [%d]\n",
		    "vlanOutPoint", vlanOutPoint, "vlanINTOutPoint", vlanINTOutPoint);
		}
	      }
	    break;
	case 'L':
	    if (vlNumImages > 0)
	      {
	      fprintf(stderr,"can't specify duration and image count\n");
	      exit(DM_FAILURE);
	      }
	    if (vlanInPoint && vlanOutPoint)
	      {
	      fprintf(stderr,"can't specify In point, Out point and duration\n");
	      exit(DM_FAILURE);
	      }
	    vlanDuration = optarg;
	    /* make sure it's valid - converter bails if not */
	    vlNumImages = SMPTE_to_INT(vlanDuration);
	    if (vlanInPoint)
	      {
	      vlanINTOutPoint = vlanINTInPoint + vlNumImages;
	      vlanOutPoint = strdup(INT_to_SMPTE(vlanINTOutPoint));
	      if (Debug)
		{
		fprintf(stderr, "Calculated values %s from %s\n",
		    "vlNumImages, vlanIntOutPoint & vlanOutPoint",
		    "vlanDuration & vlanIntInPoint");
		fprintf(stderr, "\t%s  [%s], %s  [%d]\n",
		    "vlanInPoint", vlanInPoint, "vlanINTInPoint", vlanINTInPoint);
		fprintf(stderr, "\t%s [%s], %s     [%d]\n",
		    "vlanDuration", vlanDuration, "vlNumImages", vlNumImages);
		fprintf(stderr, "\t%s [%s], %s [%d]\n",
		    "vlanOutPoint", vlanOutPoint, "vlanINTOutPoint", vlanINTOutPoint);
		}
		break;
	      }
	    if (vlanOutPoint)
	      {
	      vlanINTInPoint = vlanINTOutPoint - vlNumImages;
	      vlanInPoint = strdup(INT_to_SMPTE(vlanINTInPoint));
	      if (Debug)
		{
		fprintf(stderr, "Calculated values %s from %s\n",
		    "vlNumImages, vlanIntInPoint & vlanInPoint",
		    "vlanDuration & vlanIntOutPoint");
		fprintf(stderr, "\t%s  [%s], %s  [%d]\n",
		    "vlanInPoint", vlanInPoint, "vlanINTInPoint", vlanINTInPoint);
		fprintf(stderr, "\t%s [%s], %s     [%d]\n",
		    "vlanDuration", vlanDuration, "vlNumImages", vlNumImages);
		fprintf(stderr, "\t%s [%s], %s [%d]\n",
		    "vlanOutPoint", vlanOutPoint, "vlanINTOutPoint", vlanINTOutPoint);
		}
	      }
	    break;
	case 'O':
	    if (vlanDuration && vlanInPoint)
	      {
	      fprintf(stderr,"can't specify In point, Out point and duration (or image count)\n");
	      exit(DM_FAILURE);
	      }
	    vlanOutPoint = optarg;
	    /* make sure it's valid - converter bails if not */
	    vlanINTOutPoint = SMPTE_to_INT(vlanOutPoint);
	    if (vlanDuration)
	      {
	      vlanINTInPoint = vlanINTOutPoint - vlNumImages;
	      vlanInPoint = strdup(INT_to_SMPTE(vlanINTInPoint));
	      if (Debug)
		{
		fprintf(stderr, "Calculated values %s from %s\n",
		    "vlanIntInPoint, vlanIntOutPoint & vlanInPoint",
		    "vlanDuration & vlanOutPoint");
		fprintf(stderr, "\t%s  [%s], %s  [%d]\n",
		    "vlanInPoint", vlanInPoint, "vlanINTInPoint", vlanINTInPoint);
		fprintf(stderr, "\t%s [%s], %s     [%d]\n",
		    "vlanDuration", vlanDuration, "vlNumImages", vlNumImages);
		fprintf(stderr, "\t%s [%s], %s [%d]\n",
		    "vlanOutPoint", vlanOutPoint, "vlanINTOutPoint", vlanINTOutPoint);
		}
	      break;
	      }
	    if (vlanInPoint)
	      {
	      vlNumImages = vlanINTOutPoint - vlanINTInPoint;
	      vlanDuration = strdup(INT_to_SMPTE(vlNumImages));
	      if (Debug)
		{
		fprintf(stderr, "Calculated values %s from %s\n",
		    "vlNumImages, vlanIntOutPoint & vlanDuration",
		    "vlanOutPoint & vlanIntInPoint");
		fprintf(stderr, "\t%s  [%s], %s  [%d]\n",
		    "vlanInPoint", vlanInPoint, "vlanINTInPoint", vlanINTInPoint);
		fprintf(stderr, "\t%s [%s], %s     [%d]\n",
		    "vlanDuration", vlanDuration, "vlNumImages", vlNumImages);
		fprintf(stderr, "\t%s [%s], %s [%d]\n",
		    "vlanOutPoint", vlanOutPoint, "vlanINTOutPoint", vlanINTOutPoint);
		}
	      }
	    break;
	case 'P':
	    vlPadImages = atoi(optarg);
	    break;
	case 'R':
	    vlRiceEnable = 1;
	    setCaptureScheme("fields");
	    mvFileOffset = 1;
	    break;
	case 'S':
	    vlshowVITC = 1;
	    vlVITCsync = 1;
	    if (!vlPadImages)
	      vlPadImages = 20;
	    break;
	case 'T':
	    vlVITC = 1;
	    vlshowVITC = 1;
#if EMUL_VITC_TRIG
	    vlVITCsync = 1;
	    if (!vlPadImages)
	      vlPadImages = 20;
#endif
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

    /* do some quick sanity checking */

    /*
     * set the default image count if none was specified
     */
    if (vlNumImages < 0)
      vlNumImages = DEFAULT_IMAGE_COUNT;
      
    /*
     *	Sanity check V-LAN & timecode & trigger options
     */
    tc_required = ((vlanTTY || vlVITC) && !vlanCmdFile);
    have_tc = (vlanInPoint && (vlanDuration || vlanOutPoint));
    if ((tc_required && !have_tc) || (!tc_required && have_tc))
      {
      fprintf(stderr, "must specify either -v or two of -i, -I, -L, -O options with -t or -T\n");
      exit(DM_FAILURE);
      }

    /*
     * Sanity check colorspace and packing
     */
    if ((vlPacking == VL_PACKING_242_8 || vlPacking == VL_PACKING_R242_8) &&
	(vlColorSpace == VL_COLORSPACE_RP175 || vlColorSpace == VL_COLORSPACE_RGB))
      {
      fprintf(stderr, "can not use 422 sampling with RGB colorspaces\n");
      exit(DM_FAILURE);
      }

    /* if we are not using a QT file save the audio in a separate aifc file */
    if (AIFF_AUDIO == alFileFormat) {
	strcpy(filename, ioFileName);
	strpos = strchr(filename, '.');
	/* if there is no suffix add a NULL character and move ptr to the end */
	if (!strpos)
	    strpos = strchr(filename, '\0');
	strcpy(strpos, ".aifc");
	alFileName = filename;
    }
    else if (QUICKTIME_AUDIO == alFileFormat) {
	fprintf(stderr, "QT embedded audio not yet implemented\n");
    }	

    /* if capturing fields must have an even number, libmoviefile requires
     * this, although we could duplicate the last, odd field */
    if (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED) {
	if (vlNumImages % 2) {
	    vlNumImages++;
	    if (Verbose) {
		fprintf(stderr, "cannot request and odd # of fields\n");
		fprintf(stderr, "adjusting image count to %d\n", vlNumImages);
	    }
	}
    }

    /* check legal values for ioVec, if you are saving fields ioVecCount must 
     * be even for QT files, since fields with gaps between them can only be 
     * written out in pairs
     */
    if (ioVecCount % 2)
	ioVecCount++;

    /* check for range */
    ioVecCountMax = sysconf(_SC_IOV_MAX);
    if (ioVecCount > ioVecCountMax) {
	ioVecCount = ioVecCountMax;
	if (Verbose) {
	    fprintf(stderr, "cannot create more than %d I/O vectors\n", ioVecCountMax);
	    fprintf(stderr, "\tresetting value!\n");
	}
    }
    else if (ioVecCount <= 0)
	ioVecCount = 2;
   
    assert(dmBufferPoolSize > 0);

#if NO_RICE_FRAMES
    if ((frames == mvCaptureScheme) && vlRiceEnable) {
	fprintf(stderr, "Rice encoding is not support with frame capture mode\n");
	exit(DM_FAILURE);
    }
#endif

    /*
     * initialize video first
     */
    if (initialize_video() != DM_SUCCESS)
	exit(DM_FAILURE);

    /* 
     * there are two methods of calibrating UST/MSC values so we use a 
     * function pointer for convenience.
     */
    initialize_video_ustmsc = initialize_DM_ustmsc;

    if (alFileFormat && (initialize_audio() == DM_SUCCESS)) {
	if ((audio_recorder_pid = sproc(audio_recorder, PR_SADDR|PR_SFDS)) < 0){
	    perror("audio_recorder");
	    exit(DM_FAILURE);
	}
	if (Debug)
	    fprintf(stderr, "audio recorder PID is %d\n", audio_recorder_pid);
    }

    if (initialize_file_data() == DM_FAILURE) {
	fprintf(stderr, "error initializing file data\n");
	exit(DM_FAILURE);
    }

    if (initialize_dmbuffers() == DM_FAILURE) {
	fprintf(stderr, "error initializing dmbuffers\n");
	exit(DM_FAILURE);
    }

    /*  
     * we must call this after the initialize_file_data
     */
    if (mvFormat == MV_FORMAT_QT) {
	if (initialize_movie_file(&theMovie) == DM_FAILURE) {
	    fprintf(stderr, "error creating movie file\n");
	    exit(DM_FAILURE);
	}
    }

    if ((video_recorder_pid = sproc(video_recorder, PR_SADDR | PR_SFDS)) < 0) {
        perror("video_recorder");
	exit(DM_FAILURE);
    }
    if (Debug)
	fprintf(stderr, "video recorder PID is %d\n", video_recorder_pid);

    /*
     * Wait for processes to complete, then clean up and exit.
     * If the parent is interrupted we send signals to all of the children.
     */
    dead_pid = waitpid(ANYKID, &dead_stat, 0);

    while(dead_pid > 0) {
	/* 
	 * check the status of how the video process exited
	 */
	if ((dead_pid == video_recorder_pid) && WIFEXITED(dead_stat)) {

	    cleanup_video();

	    if (WEXITSTATUS(dead_stat) == DM_SUCCESS) {
		/* DEBUG, BM does not include the audio processing */
		if (vlBenchmark)
		    benchmark_results();
        	fprintf(stderr, "video recorder finished successfully\n");
	    }
	    else
        	fprintf(stderr, "video recorder failed\n");
	}

        if ((dead_pid == audio_recorder_pid) && WIFEXITED(dead_stat)) {

	    cleanup_audio();

	    if (WEXITSTATUS(dead_stat) == DM_SUCCESS)
         	fprintf(stderr, "audio recorder finished successfully\n");
	    else
        	fprintf(stderr, "audio recorder failed\n");

	}

	dead_pid = waitpid(ANYKID, &dead_stat, 0);
    }

    fprintf(stderr, "%s finished!\n", progName);
}


static void
usage(void)
{
    fprintf(stderr, "Usage: %s -[a# b# c# d# f h i# o# p# t%%s w# A B D F R T V v vlanfile.cmd] data_file_name\n",
	progName);
    fprintf(stderr, "\t-a#  audio file format\n");
    fprintf(stderr, "\t\t1 = no audio (default),\n");
    fprintf(stderr, "\t\t2 = QuickTime,\n");
    fprintf(stderr, "\t\t3 = AIFC\n");
    fprintf(stderr, "\t-b#  dmbuffer pool size (default = 30)\n");
    fprintf(stderr, "\t-c#  colorspace:\n");
    fprintf(stderr, "\t\t1 = CCIR601 (default),\n");
    fprintf(stderr, "\t\t2 = YUV,\n");
    fprintf(stderr, "\t\t3 = RP175,\n");
    fprintf(stderr, "\t\t4 = RGB\n");
    fprintf(stderr, "\t-d#  VL device number\n");
    fprintf(stderr, "\t-f\n");
    fprintf(stderr, "\t\tframes (default),\n");
    fprintf(stderr, "\t\tfields\n");
    fprintf(stderr, "\t-g#  writev # images to disk (default = 4)\n");
    fprintf(stderr, "\t-h   give this help message\n");
    fprintf(stderr, "\t-i#  number of images (default = 30)\n");
    fprintf(stderr, "\t-o#  moviefile starting offset\n");
    fprintf(stderr, "\t-p#  packing format:\n");
#if MVP
/*
 * this is bad cuz it prolongs the old VL style packing names - fix for 6.5
 */
    fprintf(stderr, "\t\t1 = YUV_8 (default),\n");
    fprintf(stderr, "\t\t2 = RGB_8,\n");
    fprintf(stderr, "\t\t3 = RGBA_8 (RGB)");
#if TEN_BIT_SUPT
    fprintf(stderr, ",\n");
    fprintf(stderr, "\t\t4 = YUV_10,\n");
    fprintf(stderr, "\t\t5 = RGB_10,\n");
    fprintf(stderr, "\t\t6 = RGBA_10,\n");
#else
    fprintf(stderr, "\n");
#endif
#elif DIVO
    fprintf(stderr, "\t\t1 = R242_8 (default),\n");
    fprintf(stderr, "\t\t2 = 242_8,\n");
    fprintf(stderr, "\t\t3 = 444_8 (RGB),\n");
    fprintf(stderr, "\t\t4 = R444_8 (BGR),\n");
    fprintf(stderr, "\t\t5 = 4444_8 (RGBA),\n");
    fprintf(stderr, "\t\t6 = R4444_8 (ABGR)");
#if TEN_BIT_SUPT
    fprintf(stderr, ",\n");
    fprintf(stderr, "\t\t7 = R242_10,\n");
    fprintf(stderr, "\t\t8 = 242_10,\n");
    fprintf(stderr, "\t\t9 = R2424_10,\n");
    fprintf(stderr, "\t\t10 = 2424_10,\n");
    fprintf(stderr, "\t\t11 = 2424_10_in_16\n");
#else
    fprintf(stderr, "\n");
#endif
#endif
    fprintf(stderr, "\t-t%%s specify /dev/ttyc# for V-LAN control\n");
    fprintf(stderr, "\t-v   filename  V-LAN command file\n");
    fprintf(stderr, "\t-A   abort if any image is dropped\n");
    fprintf(stderr, "\t-B   calculate capture performance and print results\n");
    fprintf(stderr, "\t-D   debug mode\n");
    fprintf(stderr, "\t-F   swap field dominance (only supported in fields mode)\n");
    fprintf(stderr, "\t-R   enable Rice encoding\n");
    fprintf(stderr, "\t-T   trigger with VITC\n");
    fprintf(stderr, "\t-V   verbose mode\n");
    fprintf(stderr, "\tfile_name  file name\n");

    exit(DM_FAILURE);
}


static int
initialize_video(void)
{
    int dev;
    VLControlValue val;

/*
 * need to fix field dominance stuff for 6.5
 */
#if MVP
    int dom_ctrl = VL_MVP_DOMINANT_FIELD;
#endif

#if 0
#if MGV
    int dom_ctrl = VL_MGV_DOMINANCE_FIELD;
#endif

#if DIVO
    int dom_ctrl = VL_FIELD_DOMINANCE;
#endif
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
     * find out what kind of video hardware we have so that we can make
     * the hardware dependency decisions
     */
    if (vlGetDeviceList(vlServer, &vlDeviceList) != DM_SUCCESS) {
	fprintf(stderr, "cannot get VL device information\n");
	exit(DM_FAILURE);
    }

    /*  
     * parse device names from the list and set video recorder function pointer 
     */
    for (dev=0; dev < vlDeviceList.numDevices; dev++) {
	vlDeviceInfo = &(vlDeviceList.devices[dev]);
	if (Debug) {
	    fprintf(stderr, "found video device %d : %s\n", dev, vlDeviceInfo->name);
	}
	if (dev != vlDevice)
	  continue;
	if (strcmp(vlDeviceInfo->name, "mvp") == 0) {
	    video_recorder = video_MVP_recorder;
	    if (Debug) {
		fprintf(stderr, "setup MVP recorder\n");
	    }
        }
        else {
	    video_recorder = video_DIVO_recorder;
	    if (Debug) {
		fprintf(stderr, "setup DIVO/SRV recorder\n");
	    }
#if SRV_FICUS_COMPAT
	    /*
	     * on ficus, SRV uses old VL so we force to compat mode
	     */
	    if ((isSRV = !strcmp(vlDeviceInfo->name, "impact")))
		{
		if (Debug) {
		    fprintf(stderr, "setup SRV FICUS compatibility mode\n");
		}		
		/* prevent trying to set color space via new VL */
		vlCompatMode = 1;
	    
		/* disable any stray rice stuff */
		vlRicePrecision = 0;
		vlRiceSampling = 0;
		vlRiceEnable = 0;

		/*
		 * remap new VL colorspace and packing back to old format and
		 *  packing where possible
		 */
		switch (vlPacking)
		  {
		  case VL_PACKING_R242_8:
		    switch (vlColorSpace)
		      {
		      case VL_COLORSPACE_CCIR601:
			vlFormat = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
			vlPacking = VL_PACKING_YVYU_422_8;
			break;
		      case VL_COLORSPACE_YUV:
			vlFormat = VL_FORMAT_SMPTE_YUV;
			vlPacking = VL_PACKING_YVYU_422_8;
			break;
		      case VL_COLORSPACE_RP175:
		      case VL_COLORSPACE_RGB:
			fprintf(stderr, "can not use 422 sampling with RGB colorspaces\n");
			exit(DM_FAILURE);
			break;
		      }
		    vlVidNodeNum = VL_MGV_NODE_NUMBER_VIDEO_1;
		    vlMemNodeNum = VL_MGV_NODE_NUMBER_MEM_VGI_1;
		    break;
		  case VL_PACKING_444_8:
		    switch (vlColorSpace)
		      {
		      case VL_COLORSPACE_CCIR601:
			vlFormat = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
			vlPacking = VL_PACKING_UYV_8_P;
			break;
		      case VL_COLORSPACE_YUV:
			vlFormat = VL_FORMAT_SMPTE_YUV;
			vlPacking = VL_PACKING_UYV_8_P;
			break;
		      case VL_COLORSPACE_RP175:
			vlFormat = VL_FORMAT_DIGITAL_COMPONENT_RGB_SERIAL;
			vlPacking = VL_PACKING_BGR_8_P;
			break;
		      case VL_COLORSPACE_RGB:
			vlFormat = VL_FORMAT_RGB;
			vlPacking = VL_PACKING_BGR_8_P;
			break;
		      }
		    vlVidNodeNum = VL_MGV_NODE_NUMBER_VIDEO_DL;
		    vlMemNodeNum = VL_MGV_NODE_NUMBER_MEM_VGI_DL;
		    break;
		  case VL_PACKING_R444_8:
		    fprintf(stderr, "Octane Digital Video does not support VL_PACKING_R444_8\n");
		    exit(DM_FAILURE);
		    break;
		  case VL_PACKING_4444_8:
		    switch (vlColorSpace)
		      {
		      case VL_COLORSPACE_CCIR601:
			vlFormat = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
			vlPacking = VL_PACKING_AUYV_4444_8;
			break;
		      case VL_COLORSPACE_YUV:
			vlFormat = VL_FORMAT_SMPTE_YUV;
			vlPacking = VL_PACKING_AUYV_4444_8;
			break;
		      case VL_COLORSPACE_RP175:
			vlFormat = VL_FORMAT_DIGITAL_COMPONENT_RGB_SERIAL;
			vlPacking = VL_PACKING_ABGR_8;
			break;
		      case VL_COLORSPACE_RGB:
			vlFormat = VL_FORMAT_RGB;
			vlPacking = VL_PACKING_ABGR_8;
			break;
		      }
		    vlVidNodeNum = VL_MGV_NODE_NUMBER_VIDEO_DL;
		    vlMemNodeNum = VL_MGV_NODE_NUMBER_MEM_VGI_DL;
		    break;
		  case VL_PACKING_R4444_8:
		    switch (vlColorSpace)
		      {
		      case VL_COLORSPACE_CCIR601:
			vlFormat = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
			vlPacking = VL_PACKING_YUVA_4444_8;
			break;
		      case VL_COLORSPACE_YUV:
			vlFormat = VL_FORMAT_SMPTE_YUV;
			vlPacking = VL_PACKING_YUVA_4444_8;
			break;
		      case VL_COLORSPACE_RP175:
			vlFormat = VL_FORMAT_DIGITAL_COMPONENT_RGB_SERIAL;
			vlPacking = VL_PACKING_RGBA_8;
			break;
		      case VL_COLORSPACE_RGB:
			vlFormat = VL_FORMAT_RGB;
			vlPacking = VL_PACKING_RGBA_8;
			break;
		      }
		    vlVidNodeNum = VL_MGV_NODE_NUMBER_VIDEO_DL;
		    vlMemNodeNum = VL_MGV_NODE_NUMBER_MEM_VGI_DL;
		    break;
		  default:
		    fprintf(stderr, "unsupported packing for Octane Digital Video [%d]\n", vlPacking);
		    exit(DM_FAILURE);
		  }
		}
	      else
		isEVO = !strncasecmp(vlDeviceInfo->name, "evo", 3);
		vlCompatMode = 0;

	    if (Debug) {
		fprintf(stderr, "SRV_FICUS_COMPAT : isSRV = %d, vlCompatMode = %d\n", isSRV, vlCompatMode);
	    }
#endif
        }
    }

    /*
     * Get nodes: a video source, and a memory drain
     */
    if ((vlVidNode = vlGetNode(vlServer, VL_SRC, VL_VIDEO, vlVidNodeNum)) == DM_FAILURE) {
	fprintf(stderr, "cannot get video source node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    if ((vlMemNode = vlGetNode(vlServer, VL_DRN, VL_MEM, vlMemNodeNum)) == DM_FAILURE) {
	fprintf(stderr, "cannot get memory node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    /*
     * Get a path
     */
    if ((vlPath = vlCreatePath(vlServer, vlDevice, vlVidNode, vlMemNode)) < 0)
    {
	fprintf(stderr, "cannot create path: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    /*
     * get the device node
     */
    if ((vlDevNode = vlGetNode(vlServer, VL_DEVICE, 0, VL_ANY)) ==
	  DM_FAILURE) {
	fprintf(stderr, "cannot get device node: %s\n", vlStrError(vlErrno));
	return(DM_FAILURE);
	}

    /*
     * Add it to the path
     */
    if (vlAddNode(vlServer, vlPath, vlDevNode) == DM_FAILURE) {
	fprintf(stderr, "cannot add device node to path: %s\n",
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

    /* compatibility mode is for 6.4 support of Octane, this will not be
     * necessary for 6.5 */
    if (vlCompatMode) {
	val.intVal = vlFormat;
	if (vlSetControl(vlServer, vlPath, vlMemNode, VL_FORMAT, &val)) {
	    fprintf(stderr, "cannot set format on memory node: %s\n",
			vlStrError(vlErrno));
	    return(DM_FAILURE);
	}
    }
    else {

#ifndef MVP
	/* for DIVO and beyond */
	val.intVal = vlColorSpace;
	if (vlSetControl(vlServer, vlPath, vlMemNode, VL_COLORSPACE, &val)) {
	    fprintf(stderr, "cannot set colorspace on memory node: %s\n",
			vlStrError(vlErrno));
	    return(DM_FAILURE);
	}

	/* Let's check what we got */
	if (vlGetControl(vlServer, vlPath, vlMemNode, VL_COLORSPACE, &val)) {
	    fprintf(stderr, "cannot verify colorspace on memory node: %s\n",
			vlStrError(vlErrno));
	    return(DM_FAILURE);
	}
	else {
	    switch (val.intVal) {
		case VL_COLORSPACE_CCIR601:
		    fprintf(stderr, "colorspace is CCIR601\n");
                    break;
		case VL_COLORSPACE_YUV:
		    fprintf(stderr, "colorspace is SMPTE YUV\n");
                    break;
		case VL_COLORSPACE_RGB:
		    fprintf(stderr, "colorspace is RGB\n");
                    break;
		case VL_COLORSPACE_RP175:
		    fprintf(stderr, "colorspace is RP175\n");
                    break;
		default:
		    fprintf(stderr, "unknown colorspace\n");
		    usage();
                    break;
	    }
	}

	if (vlRiceEnable) {

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

    /* STW - setup DIVO triggering */
    if (vlGPI)
	{
	val.xfer_trigger.triggerType = VL_TRIGGER_GPI; 
	val.xfer_trigger.value.instance = vlGPI;
	if (vlSetControl(vlServer, vlPath, vlVidNode, VL_TRANSFER_TRIGGER, &val))
	  {
	  fprintf(stderr, "cannot set GPI transfer trigger on video node: %s\n",
		  vlStrError(vlErrno));
	  return(DM_FAILURE);
	  }
	}
#if ! EMUL_VITC_TRIG
      else if (vlVITC)
	{
	if (Verbose)
	  fprintf("stderr, setting VL_TRIGGER_VITC\n");
	val.xfer_trigger.triggerType = VL_TRIGGER_VITC; 
	SMPTE_to_DMTC(vlanInPoint, &val.xfer_trigger.value.vitc);
	if (vlSetControl(vlServer, vlPath, vlVidNode, VL_TRANSFER_TRIGGER, &val))
	  {
	  fprintf(stderr, "cannot set VITC transfer trigger on video node: %s\n",
		  vlStrError(vlErrno));
	  return(DM_FAILURE);
	  }
	}
#endif
      else
	{
#if ! EVO
	val.xfer_trigger.triggerType = VL_TRIGGER_NONE; 
	if (vlSetControl(vlServer, vlPath, vlVidNode, VL_TRANSFER_TRIGGER, &val))
	  {
	  fprintf(stderr, "cannot set normal transfer trigger on video node: %s\n",
		  vlStrError(vlErrno));
	  return(DM_FAILURE);
	  }
#endif
	}
#endif
    }

    /* 
     * NOTE: cannot set the timing on a memory node as was possible with
     * Sirius
     */
    if (vlGetControl(vlServer, vlPath, vlVidNode, VL_TIMING, &val)) {
	fprintf(stderr, "cannot verify timing on source node: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }
    vlTiming = val.intVal;

#if 0
	if (vlSetControl(vlServer, vlPath, vlMemNode, VL_TIMING, &val)) {
	    fprintf(stderr, "cannot set timing on memory node: %s\n",
		    vlStrError(vlErrno));
	    return(DM_FAILURE);
	}
#endif

    /* 
     * sanity checking for colorspace, format, and packing is done when the 
     * packing is set so we do this last.
     */
    val.intVal = vlPacking;
    if (vlSetControl(vlServer, vlPath, vlMemNode, VL_PACKING, &val)) {
	fprintf(stderr, "cannot set path packing: %s\n", vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    /*
     * many of the packing modes are here for compatibility
     * although the video device may support a particular packing value
     * libmoviefile may not.  We will fix this mapping later!
     */
    switch (vlPacking) {

#ifndef MVP
        case VL_PACKING_R242_8:
	    mvPacking = DM_IMAGE_PACKING_CbYCrY;
            vlBytesPerPixel = 2;
            break;

        case VL_PACKING_242_8:
	    /* This is not a supported DM_IMAGE_PACKING value */
	    if (Verbose)
		fprintf(stderr, "WARNING: libmoviefile does not support reverse component order (CrYCbY)\n");
	    mvPacking = DM_IMAGE_PACKING_CbYCrY;
            vlBytesPerPixel = 2;
            break;
#endif
        case VL_PACKING_YVYU_422_8:
	    mvPacking = DM_IMAGE_PACKING_CbYCrY;
            vlBytesPerPixel = 2;
            break;

#ifndef MVP
        case VL_PACKING_444_8:
#endif
	case VL_PACKING_UYV_8_P:
        case VL_PACKING_BGR_8_P:
	    switch (vlColorSpace)
	      {
	      case VL_COLORSPACE_CCIR601:
	      case VL_COLORSPACE_YUV:
		/* This is not a supported DM_IMAGE_PACKING value */
		if (Verbose)
		    fprintf(stderr, "WARNING: libmoviefile does not support reverse component order (CrYCb)\n");
		mvPacking = DM_IMAGE_PACKING_CbYCr;
		break;
	      case VL_COLORSPACE_RP175:
	      case VL_COLORSPACE_RGB:
/* DM_IMAGE_PACKING_RGB is not supported */
		mvPacking = DM_IMAGE_PACKING_RGB;
		break;
	      }
            vlBytesPerPixel = 3;
            break;

#ifndef MVP
        case VL_PACKING_R444_8:
#endif
        case VL_PACKING_RGB_8_P:
	    switch (vlColorSpace)
	      {
	      case VL_COLORSPACE_CCIR601:
	      case VL_COLORSPACE_YUV:
		mvPacking = DM_IMAGE_PACKING_CbYCr;
		break;
	      case VL_COLORSPACE_RP175:
	      case VL_COLORSPACE_RGB:
/* DM_IMAGE_PACKING_BGR is not supported */
		mvPacking = DM_IMAGE_PACKING_BGR;
		break;
	      }
            vlBytesPerPixel = 3;
            break;

#ifndef MVP
        case VL_PACKING_4444_8:
#endif
        case VL_PACKING_AUYV_4444_8:
        case VL_PACKING_ABGR_8:
	    switch (vlColorSpace)
	      {
	      case VL_COLORSPACE_CCIR601:
	      case VL_COLORSPACE_YUV:
		/* This is not a supported DM_IMAGE_PACKING value */
		if (Verbose)
		    fprintf(stderr, "WARNING: libmoviefile does not support reverse component order (CrYCbA)\n");
		mvPacking = DM_IMAGE_PACKING_CbYCrA;
		break;
	      case VL_COLORSPACE_RP175:
	      case VL_COLORSPACE_RGB:
		mvPacking = DM_IMAGE_PACKING_RGBA;
		break;
	      }
            vlBytesPerPixel = 4;
            break;

#ifndef MVP
        case VL_PACKING_R4444_8:
#endif
        case VL_PACKING_YUVA_4444_8:
        case VL_PACKING_RGBA_8:
	    switch (vlColorSpace)
	      {
	      case VL_COLORSPACE_CCIR601:
	      case VL_COLORSPACE_YUV:
		mvPacking = DM_IMAGE_PACKING_CbYCrA;
		break;
	      case VL_COLORSPACE_RP175:
	      case VL_COLORSPACE_RGB:
		mvPacking = DM_IMAGE_PACKING_ABGR;
		break;
	      }
             vlBytesPerPixel = 4;
            break;

        case VL_PACKING_242_10_in_16_L:
	    mvPacking = DM_IMAGE_PACKING_CbYCrY;
            vlBytesPerPixel = 8;
            break;

        case VL_PACKING_R242_10: 
        case VL_PACKING_242_10:
        case VL_PACKING_R2424_10_10_10_2Z:
        case VL_PACKING_2424_10_10_10_2Z:
            fprintf(stderr, "10-bit pixel component packings are not supported in libmovifile\n");
            return(DM_FAILURE);

	default:
            fprintf(stderr, "invalid vlPacking value %d\n", vlPacking);
            return(DM_FAILURE);
    }

#if 0
    fprintf(stderr, "vlCaptureMode : %sINTERLEAVED\n",
	    (vlCaptureMode == VL_CAPTURE_INTERLEAVED ? "" : "NON"));
#endif
    val.intVal = vlCaptureMode;
    if (vlSetControl(vlServer, vlPath, vlMemNode, VL_CAP_TYPE, &val)) {
	fprintf(stderr, "cannot set path capture type: %s\n", 
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    if (vlGetControl(vlServer, vlPath, vlMemNode, VL_CAP_TYPE, &val)) {
	fprintf(stderr, "cannot verify path capture type: %s\n",
		vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    /* call this after VL_CAP_TYPE to get the right values */
    if (vlGetControl(vlServer, vlPath, vlMemNode, VL_SIZE, &val)) {
	fprintf(stderr, "couldn't verify path size: %s\n", vlStrError(vlErrno));
	return(DM_FAILURE);
    }

    vlXsize = val.xyVal.x;
    mvXsize = vlXsize;  	/* set X size for movie file */
    vlYsize = val.xyVal.y;

    if (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED) {
	mvYsize = vlYsize*2;
    }
    else { 
	mvYsize = vlYsize;
    }

    vlBytesPerImage = vlGetTransferSize(vlServer, vlPath);

    if (Debug)
      {
      fprintf(stderr, "vlXsize : %d\n", vlXsize);
      fprintf(stderr, "vlYsize : %d\n", vlYsize);
      fprintf(stderr, "mvXsize : %d\n", mvXsize);
      fprintf(stderr, "mvYsize : %d\n", mvYsize);
      fprintf(stderr, "vlBytesPerPixel : %d\n", vlBytesPerPixel);
      fprintf(stderr, "vlBytesPerImage : %d\n", vlBytesPerImage);
      fprintf(stderr, "vlTransferSize : %d\n", vlGetTransferSize(vlServer, vlPath));
      }

/* DEBUG, how do we program for the 64 bit formats? */

    /* i think there is a problem with this for O2!
     * get the field dominance, we only use this to store in the disk file
     * header use dominance 
     */
#ifdef MVP
    /*
     * this is bad even though it makes things compile on O2... it breaks PAL
     */
    vlFieldDominance = 1;
#else
#ifndef SRV_FICUS_COMPAT
    if (vlGetControl(vlServer, vlPath, vlVidNode, dom_ctrl, &val) == DM_FAILURE) {
	vlPerror(progName);
	fprintf(stderr, "cannot verify field dominance\n");
	return(DM_FAILURE);
    }
    vlFieldDominance = val.intVal;
#endif
#endif

    /* compute the proper timing factors for movie file parameters and 
     * the SMPTE to int conversion, note we must do this before we call 
     * read_vlan_cmd_file()
     */
    if (vlTiming == VL_TIMING_525_SQ_PIX) {

        vlanFramesPerSec  = 30;
        vlanFramesPerMin  = 1800;
        vlanFramesPerHour = 108000;
	mvAspectRatio = 1.0;
	mvRate = 29.97;
	mvFrameTime = 1001;
	mvImageTimeScale = MV_IMAGE_TIME_SCALE_NTSC;

	/* check dominance against the timing */
	if ((VL_F1_IS_DOMINANT == vlFieldDominance) && Verbose)
	    fprintf (stderr, "WARNING: field dominance has been switched from the normal setting for 525 timing!\n");    

  	/* note that swapping field dominance only applies if we are capturing
	 * fields.
  	 */
	if (vlCaptureMode == VL_CAPTURE_INTERLEAVED) 
	    mvInterlacing = DM_IMAGE_NONINTERLACED;
	else if (!mvSwapFieldDominance)
	    mvInterlacing = DM_IMAGE_INTERLACED_ODD;
	else
	    mvInterlacing = DM_IMAGE_INTERLACED_EVEN;

    }
    else if (vlTiming == VL_TIMING_525_CCIR601) {

        vlanFramesPerSec  = 30;
        vlanFramesPerMin  = 1800;
        vlanFramesPerHour = 108000;
	mvAspectRatio = 1.095;
	mvRate = 29.97;
	mvFrameTime = 1001;
	mvImageTimeScale = MV_IMAGE_TIME_SCALE_NTSC;

	/* check dominance against the timing */
	if ((VL_F1_IS_DOMINANT == vlFieldDominance) && Verbose)
	    fprintf (stderr, "WARNING: field dominance has been switched from the normal setting for 525 timing!\n");    

	if (vlCaptureMode == VL_CAPTURE_INTERLEAVED) 
	    mvInterlacing = DM_IMAGE_NONINTERLACED;
	else if (!mvSwapFieldDominance)
	    mvInterlacing = DM_IMAGE_INTERLACED_ODD;
	else
	    mvInterlacing = DM_IMAGE_INTERLACED_EVEN;

    }
    else if (vlTiming == VL_TIMING_625_SQ_PIX) {

        vlanFramesPerSec  = 25;
        vlanFramesPerMin  = 1500;
        vlanFramesPerHour = 90000;
	mvAspectRatio = 1.0;
	mvRate = 25.;
	mvFrameTime = 1;
	mvImageTimeScale = MV_IMAGE_TIME_SCALE_PAL;

	/* check dominance against the timing */
	if ((VL_F2_IS_DOMINANT == vlFieldDominance) && Verbose)
	    fprintf (stderr, "WARNING: field dominance has been switched from the normal setting for 625 timing!\n");    

	if (vlCaptureMode == VL_CAPTURE_INTERLEAVED) 
	    mvInterlacing = DM_IMAGE_NONINTERLACED;
	else if (!mvSwapFieldDominance)
	    mvInterlacing = DM_IMAGE_INTERLACED_EVEN;
	else
	    mvInterlacing = DM_IMAGE_INTERLACED_ODD;

    }
    else if (vlTiming == VL_TIMING_625_CCIR601) {

        vlanFramesPerSec  = 25;
        vlanFramesPerMin  = 1500;
        vlanFramesPerHour = 90000;
	mvAspectRatio = .9157;
	mvRate = 25.;
	mvFrameTime = 1;
	mvImageTimeScale = MV_IMAGE_TIME_SCALE_PAL;

	/* check dominance against the timing */
	if ((VL_F2_IS_DOMINANT == vlFieldDominance) && Verbose)
	    fprintf (stderr, "WARNING: field dominance has been switched from the normal setting for 625 timing!\n");    

	if (vlCaptureMode == VL_CAPTURE_INTERLEAVED) 
	    mvInterlacing = DM_IMAGE_NONINTERLACED;
	else if (!mvSwapFieldDominance)
	    mvInterlacing = DM_IMAGE_INTERLACED_EVEN;
	else
	    mvInterlacing = DM_IMAGE_INTERLACED_ODD;

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

	/* if we have an external setup file let's read it */
/* NOTE: what if we want to put this in a loop with multiple edits in
the command file */
	if (vlanCmdFile) {
	    if ((vlanFileDesc = fopen(vlanCmdFile, "r")) == NULL) {
        	fprintf (stderr, "Cannot open V-LAN command file!\n");
        	exit(DM_FAILURE);
	    }

	    read_vlan_cmd_file();
	}
	/* start the edit */
	if (sendVLANCmd(VLAN_EDIT_CMD) == DM_FAILURE) {
	    fprintf (stderr, "Cannot issue V-LAN start edit command!\n");
	    return(DM_FAILURE);
	}
    }

    /* setup transfer descriptor */
    vlXferDesc.mode = VL_TRANSFER_MODE_DISCRETE;
    vlXferDesc.count = vlNumImages + vlPadImages;
    vlXferDesc.delay = 0;
#if EMUL_VITC_TRIG
    vlXferDesc.trigger = ((vlGPI) ? VLDeviceEvent : VLTriggerImmediate);
#else
    vlXferDesc.trigger = ((vlGPI || vlVITC) ? VLDeviceEvent : VLTriggerImmediate);
#endif


    if (Verbose) {
	if (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED)
	    fprintf(stderr, "capturing data as fields\n");
	else
 	    fprintf(stderr, "capturing data as frames\n");
    }

    if (Verbose) {
        fprintf(stderr, "\t%d images of size: %d x %d x %d\n",
		vlNumImages, vlXsize, vlYsize, vlBytesPerPixel);
	fprintf(stderr, "\tvlBytesPerImage = %d\n", vlBytesPerImage);
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

    return(DM_SUCCESS);
}


/*
 * initialize_dmbuffers() - 
 *      
 */
static int
initialize_dmbuffers(void)
{
    DMparams * params_list;

    if (dmParamsCreate(&params_list) == DM_FAILURE) {
        fprintf(stderr, "error creating parameter list\n");
        return(DM_FAILURE);
    }

    if (dmBufferSetPoolDefaults(params_list, dmBufferPoolSize, maxBytesPerXfer,
		DM_FALSE, DM_FALSE) == DM_FAILURE) {
        fprintf(stderr, "error setting pool defaults\n");
        return(DM_FAILURE);
    }

#if MVP
    if (vlDMPoolGetParams(vlServer, vlPath, vlMemNode, params_list) == DM_FAILURE) {
        fprintf(stderr, "error getting pool params for VL\n");
        return(DM_FAILURE);
    }
#endif

#if (MGV || DIVO) 
    if (vlDMGetParams(vlServer, vlPath, vlMemNode, params_list) == DM_FAILURE) {
        fprintf(stderr, "error getting pool params for VL\n");
        return(DM_FAILURE);
    }
#endif
    dmParamsSetInt(params_list, DM_BUFFER_ALIGNMENT, getpagesize());
    if (vlRiceEnable)
	dmParamsSetEnum(params_list, DM_POOL_VARIABLE, DM_TRUE);

    /* create the buffer pool */
    if (dmBufferCreatePool(params_list, &dmPool) == DM_FAILURE) {
        fprintf(stderr, "error creating DM pool\n");
        return(DM_FAILURE);
    }

    if (vlDMPoolRegister(vlServer, vlPath, vlMemNode, dmPool) == DM_FAILURE) {
        fprintf(stderr, "error registering DM pool\n");
        return(DM_FAILURE);
    }

    /* sets size of dmBuffer array in user */
    if ((dmBuffers = (DMbuffer *) calloc(dmBufferPoolSize, sizeof(DMbuffer *)))
        	== NULL ) {
        fprintf(stderr, "error mallocing memory for dmbuffers\n");
        return(DM_FAILURE);
    }
 
    return(DM_SUCCESS);
}


/*
 * initialize_audio() - set file parameters using the audiofile library
 *      
 *      
 */
static int
initialize_audio(void)
{
    AFfilesetup audio_file_setup;
    ALpv audio_params[5];
    char alDevName[32],
	 alCGenLabel[32],
	 alMClkLabel[32];
    long bytes_per_sample = AL_SAMPLE_16;
    long long  sampleRate;
    long err;
    int  alDefInpIntRID,
	 alClkGenRID,
	 alMClkRID,
	 alDigital;

    /* the audio system outputs one frame per clock tick,
     * a frame may contain multiple channels (samples and channels are really
     * the same thing) and multiple bytes per sample, therefore, 
     * total # samples = frames * channels
     */ 

    /* if there is a problem with the audio hardware we return failure status
     * get the default input devicename and resource id
     */
    audio_params[0].param = AL_NAME;
    audio_params[0].value.ptr = alDevName;
    audio_params[0].sizeIn = 32;
    audio_params[1].param = AL_CHANNELS;
    audio_params[2].param = AL_RATE;
    audio_params[3].param = AL_INTERFACE;
    audio_params[4].param = AL_CLOCK_GEN;
    if (alGetParams(AL_DEFAULT_INPUT, audio_params, 5) < 0)
      {
      if (Debug)
	fprintf(stderr, "alGetParams AL_NAME failed\n");
      return(DM_FAILURE);	
      }
    alSamplesPerFrame = audio_params[1].value.i;
    sampleRate = audio_params[2].value.ll;
    alDefInpIntRID = audio_params[3].value.i;
    alClkGenRID = audio_params[4].value.i;

    audio_params[0].param = AL_TYPE;
    if (alGetParams(alDefInpIntRID, audio_params, 1) < 0)
      {
      if (Debug)
	fprintf(stderr, "alGetParams AL_TYPE failed\n");
      return(DM_FAILURE);	
      }
    if (Debug)
      fprintf(stderr,"audio_params[0].value.i (INTERFACE_TYPE) : %08X\n",
	      audio_params[0].value.i);
    alSampleRate = 0;
    if ((alDigital = alIsSubtype(AL_DIGITAL_IF_TYPE, audio_params[0].value.i)))
	{
	/* not all audio hardware support the ability to read the digital input
	 * sampling rate
	 */
	switch (sampleRate)
	  {
	  case AL_RATE_UNDEFINED:
	    if (Verbose)
	      fprintf(stderr, "the digital input signal is valid, but rate is not encoded in the input signal\n");
	    break;

	  case AL_RATE_UNACQUIRED:
	    if (Verbose)
	      fprintf(stderr, "the digital input signal is valid, but rate is not acquired from the input signal\n");
	    break;

	  case 0:
	  case AL_RATE_NO_DIGITAL_INPUT:
	    if (Verbose)
	      fprintf(stderr, "the digital input signal is not valid\n");
	    break;

	  default:
	    alSampleRate = (long) alFixedToDouble(sampleRate);
	  }
	}
      else
	alSampleRate = (long) alFixedToDouble(sampleRate);

    /* figure out if ksync is setup */
    audio_params[0].param = AL_LABEL;
    audio_params[0].value.ptr = alCGenLabel;
    audio_params[0].sizeIn = 32;
    audio_params[1].param = AL_MASTER_CLOCK;
    if (alGetParams(alClkGenRID, audio_params, 2) < 0)
      {
      if (Debug)
	fprintf(stderr, "alGetParams AL_MASTER_CLOCK failed\n");
      return(DM_FAILURE);	
      }
    alMClkRID = audio_params[1].value.i;
    audio_params[0].param = AL_LABEL;
    audio_params[0].value.ptr = alMClkLabel;
    audio_params[0].sizeIn = 32;
    audio_params[1].param = AL_VIDEO_SYNC;
    audio_params[2].param = AL_TYPE;
    if (alGetParams(alMClkRID, audio_params, 3) < 0)
      {
      if (Debug)
	fprintf(stderr, "alGetParams AL_VIDEO_SYNC failed\n");
      return(DM_FAILURE);	
      }
    if (Debug)
      {
      fprintf(stderr,"audio_params[1].value.i (VIDEO_SYNC): %08X\n",
	      audio_params[1].value.i);
      fprintf(stderr,"audio_params[2].value.i (MCLK_TYPE): %08X\n",
	      audio_params[2].value.i);
      }

    if (Verbose)
      {
      fprintf(stderr, "Using %d channels on %s audio device %s @ %ld Hz\n",
		      alSamplesPerFrame, (alDigital ? "digital" : "analog"),
		      alDevName, alSampleRate);
      fprintf(stderr, "Using %s to drive %s\n", alMClkLabel, alCGenLabel);
      }

    /* this assumes that ADAT or AES clocks a synced to video externally */
    if (audio_params[2].value.i == AL_CRYSTAL_MCLK_TYPE)
      fprintf(stderr, "WARNING: audio clock is free running.  It must be locked to guarantee synchronization\n");

    if (!alSampleRate)
      {
      fprintf(stderr, "Invalid audio sample rate\n");
      return DM_FAILURE;
      }
    alUSTPerMSC = (unsigned long long) NANOS_PER_SEC / alSampleRate;

    /* configure audio port */
    if (!(alPortConfig = alNewConfig())) {
        err = oserror();
        strerror(err);
	return(DM_FAILURE);
    }

    /* make buffer large enough to store about 1 sec. of audio */
    alFrameBufferSize = alSampleRate;  /* number of audio frames in the buffer*/
    alSampleBufferSize = alFrameBufferSize * alSamplesPerFrame;

    /* set hardware to format the data to match that which was configured for
     * the file with afInitSampleFormat()
     * ALsetqueuesize() will produce an error if there is a problem with 
     * the hardware.
     */
    alSetQueueSize(alPortConfig, alFrameBufferSize);
    alSetWidth(alPortConfig, bytes_per_sample);
    alSetChannels(alPortConfig, alSamplesPerFrame);
    alSetSampFmt(alPortConfig, AL_SAMPFMT_TWOSCOMP);

    /* initialize the structure which passes configuration parameters to 
     * AFopenfile which formats the skeleton for a new file.
     */
    audio_file_setup = afNewFileSetup();

    /* initialize audio file parameters */
    afInitChannels(audio_file_setup, AF_DEFAULT_TRACK, alSamplesPerFrame);
    afInitSampleFormat(audio_file_setup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 
		bytes_per_sample*8); /* number of bits in a sample data point */
    afInitRate(audio_file_setup, AF_DEFAULT_TRACK, alSampleRate);

    /* open the audio file */
    if ((aflAudioFile = afOpenFile(alFileName, "w", audio_file_setup)) ==
                AF_NULL_FILEHANDLE) {
        /* error message handling is not necessary because the audiofile
         * library has a default verbose error handler */
        fprintf(stderr,"cannot open audio file %s\n", alFileName);
        return(DM_FAILURE);
    }

    afFreeFileSetup(audio_file_setup);

    /* allocate buffer memory and fill with zeroes which will be used to 
     * initialize msc 
     */
    if ((alSampleBuffer = (char *) calloc(alFrameBufferSize, afGetFrameSize(aflAudioFile, AF_DEFAULT_TRACK, 0))) == NULL) {
        fprintf(stderr, "error mallocing memory for audio buffer\n");
        return(DM_FAILURE);
    }

    if (Verbose)
    {
        fprintf(stderr,"audio samples per frame = %d\n", alSamplesPerFrame);
        fprintf(stderr,"bytes per sample = %d\n", bytes_per_sample);
        fprintf(stderr,"audio sample buffer contains %d frames of %d samples\n",
                alFrameBufferSize, alSampleBufferSize);
        fprintf(stderr,"audio filename is %s\n", alFileName);
    }

    return(DM_SUCCESS);

} /* --------------------- end initialize_audio() --------------- */


static void 
audio_recorder(void *arg)
{
    stamp_t audio_ust;
    long err, current_samples; 
    long long drop_samples;
    fd_set read_fds;
    int nfds, audio_port_fd; 
    int fill_point; 

    prctl(PR_TERMCHILD);
    sigset(SIGHUP, cleanup_audio);
    FD_ZERO(&read_fds);

    /* start capturing audio which is ahead of the video because
     * the video hardware delay is much longer than any audio delays,
     * this will always be true!
     */
    if (Verbose)
        fprintf(stderr, "starting the audio recorder\n");

    /* open to port for reading */
    alAudioPort = alOpenPort(progName, "r", alPortConfig);
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
        exit(DM_FAILURE);
    }

    /* set the audio port to wake us up when the buffer is 3/4 full, we
     * do not want an underflow or overflow condition */
    audio_port_fd = alGetFD(alAudioPort); 
    /* select checks file descriptors from 0 through nfds - 1 */
    nfds = audio_port_fd + 1;

    fill_point = (alFrameBufferSize*3)/4;
    drop_samples = alFrameBufferSize/2;

    if (Verbose)
	fprintf(stderr, "audio process waiting for video to begin\n");

    /* we throw away audio data until valid data appears in the video 
     * DMediaInfo structure, note this has been changed to do work with
     * frames rather than samples */
    while (vlFirstUSTMSC.ust == 0) {

	if (alSetFillPoint(alAudioPort, fill_point) == DM_FAILURE) {
	    fprintf(stderr, "cannot set fill point for audio port\n");
	    err = oserror();
            strerror(err);
	    exit(DM_FAILURE);
	}

	/* find out how many samples are in the AL library buffer */
	current_samples = alGetFilled(alAudioPort);
	alReadFrames(alAudioPort, alSampleBuffer, current_samples); 
	if (Debug)
	    fprintf(stderr, "audio sample while waiting for video\n");
	FD_SET(audio_port_fd, &read_fds);	
	select(nfds, &read_fds, NULL, NULL, NULL);
    }

    /* calculate the time of the first audio sample which came into the port */
    if ((audio_ust = initialize_audio_sync()) == DM_FAILURE)
        exit(DM_FAILURE);

    /* if the number of samples to be dropped exceeds the size of the buffer
     * that means we screwed up, we can assert that this would never occur.
     */
    drop_samples = initialize_video_ustmsc(audio_ust);

    if (drop_samples > alFrameBufferSize) {
        fprintf(stderr, "# of audio samples to drop is larger than the buffer\n");
	exit(DM_FAILURE); 
    }
    else if (drop_samples < 0) {
	fprintf(stderr, "Audio is behind video!  This should never happen!\n");
	exit(DM_FAILURE); 
    }
    else if (Verbose)
        fprintf(stderr, "audio recorder is dropping %ld samples\n", drop_samples);
	
    /* we will read in the number of samples to be dropped but we will
     * not write them to disk.
     */
    if (alReadFrames(alAudioPort, alSampleBuffer, (long) drop_samples) < 0 ) {
	fprintf(stderr, "audio recorder failed while trimming samples\n");
	err = oserror();
        strerror(err);
	exit(DM_FAILURE);
    }

    /* continue audio capture */
    do {
	if (alReadFrames(alAudioPort, alSampleBuffer, alFrameBufferSize) < 0) {
	    err = oserror();
            strerror(err);
	    exit(DM_FAILURE);
  	}

	if (afWriteFrames(aflAudioFile, AF_DEFAULT_TRACK, alSampleBuffer, alFrameBufferSize) < 0) {
	    err = oserror();
            strerror(err);
	    exit(DM_FAILURE);
        }
    /* since the AL has no event mechanism we read until the video
     * recorder is finished and tells the audio recorder or until the parent 
     * dies 
     */
    } 
    while(!alInterrupt);

    exit(DM_SUCCESS);
} /* --------------------- end audio_recorder() --------------- */


static void
audio_interrupt(void)
{
    alInterrupt = 1;
}



/* this routine can be used for synchronizing both input and output of audio
 * and video data.  Care is taken to perform signed arithmetic with the unsigned
 * function parameters.
 */
static
stamp_t initialize_audio_sync(void)
{
    stamp_t first_audio_msc,
	    first_audio_ust,
	    ref_audio_msc,
	    ref_audio_ust,
	    msc1,
	    ust2,
	    msc2,
	    dummy;

    /* time the last audio sample arrived at the port */
    if (alGetFrameTime(alAudioPort, &ref_audio_msc, &ref_audio_ust) < 0) {
        fprintf(stderr, "error getting first audio frame, underflow!\n");
        perror(progName);
        return(DM_FAILURE);
    }
    msc2 = ref_audio_msc;
    ust2 = ref_audio_ust;

    /* what is the # of the first sample which came into the port? */
    if (alGetFrameNumber(alAudioPort, &first_audio_msc) < 0) {
        fprintf(stderr, "error getting last audio frame, underflow!\n");
        perror(progName);
        return(DM_FAILURE);
    }
    msc1 = first_audio_msc;

    /* this is the time at which the first audio sample was put into the buffer 
    alStartTime = ref_audio_ust - (ref_audio_msc - first_audio_msc)*alUSTPerMSC;
    */

    first_audio_ust = ust2 + (stamp_t)((double)(msc1 - msc2) * alUSTPerMSC);

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
    audio_frame_diff = (vlFirstUSTMSC.ust - audio_start_time) / alUSTPerMSC;
    /* old al was sample based
    audio_sample_diff = audio_frame_diff * alGetChannels(alPortConfig);
    *  new al is frame based
    */
    audio_sample_diff = audio_frame_diff;

    if (Verbose && Debug) {
        fprintf(stderr, "video start time = %lld\n", vlFirstUSTMSC.ust);
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

    if ((video_ust_per_msc = vlGetUSTPerMSC(vlServer, vlPath, vlMemNode)) < 0) {
        vlPerror(progName);
        exit(DM_FAILURE);
    }

    /* grab a reference UST/MSC pair, this is the most recent field which came 
     * into the jack.  Although this event probably occured sometime in the 
     * past the data may not necessarily be in the dmbuffer yet.
     */ 
    if (vlGetUSTMSCPair(vlServer, vlPath, vlVidNode, 0, vlMemNode, &ref_video_pair) < 0) {
        vlPerror(progName);
        return(DM_FAILURE);
    }
 
    /* use the msc of the "first" field which is made available from the 
     * buffer, this is the "last"
     */
    first_video_msc = vlFirstUSTMSC.msc; 

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
    /* tell the audio process to stop */
    audio_interrupt();

    if (Verbose)
	fprintf(stderr, "cleaning up video!\n");
    vlEndTransfer(vlServer, vlPath);
    vlDMPoolDeregister(vlServer, vlPath, vlVidNode, dmPool);
    dmBufferDestroyPool(dmPool);
    vlDestroyPath(vlServer, vlPath);
    vlCloseVideo(vlServer);

    free(videoData);
    free(dmBuffers);

    if (vlanCmdFile)
	fclose(vlanFileDesc);

    if (vlanTTY && vlanDevice)
	vlan_close(vlanDevice);
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


static int
initialize_file_data(void)
{
    long num_pages, io_vec;
    struct dioattr da;
    int ioBlocksPerImage = 0;

    ioFileFD = open(ioFileName, O_DIRECT | O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (ioFileFD < 0) {
	fprintf(stderr, "error opening %s for writing\n", ioFileName);
	perror(progName);
	return(DM_FAILURE);
    }

    /* query the block size and memory alignment */
    if (fcntl(ioFileFD, F_DIOINFO, &da) < 0) {
	perror(progName);
	return(DM_FAILURE);
    }
	  
    ioBlockSize = da.d_miniosz;
    ioMaxXferSize = da.d_maxiosz;
    
    /* page alignment is best for any mode */
    ioAlignment = getpagesize();

    /* Note this is not documented in the man pages of writev or direct I/O!
     * If we are using writev we must align to the pagesize not the file
     * system blocksize which is required for direct I/O.
     * if (blocksize >= pagesize) then we do not have to worry but
     * if (blocksize < pagesize) then we must align the I/O request size
     * to the pagesize not the blocksize.  This will waste space but is
     * necessary to maintain performance.
     */
     if (ioAlignment > ioBlockSize) {
	if (Verbose && Debug)
            fprintf(stderr, "page size (%d) is larger than the file system block size (%d) so the I/O request will be aligned the page size\n", ioAlignment, ioBlockSize);
	ioBlockSize = ioAlignment;
    }
    assert(ioBlockSize != 0);

    /* calculate blocks/field based on blocksize */
    ioBlocksPerImage = (vlBytesPerImage + ioBlockSize - 1) / ioBlockSize;
    maxBytesPerXfer = ioBlocksPerImage * ioBlockSize;

    if (Debug)
	assert(maxBytesPerXfer != 0);

    /* we must compare the max DMA xfer size with the user's I/O request */
    if (maxBytesPerXfer > ioMaxXferSize) {
	if (Verbose) {
	    fprintf(stderr, "I/O request size = %d exceeds max dma size = %d\n",
				maxBytesPerXfer, ioMaxXferSize);
	}
	return(DM_FAILURE);
    }

    /* we must also check if the # of I/O vectors exceeds the DMA xfer size */
    if (maxBytesPerXfer * ioVecCount > ioMaxXferSize) {
	/* could readjust ioVecCount but then we would have to do the same check
	 * as is done earlier so we punt and give a recommendation.
	 */

	/* all values in this equation are multiples of the page size so we
	 * do not lose anything in our integer arithmetic */
	num_pages = (maxBytesPerXfer * ioVecCount) % ioMaxXferSize;
	num_pages /= ioAlignment; 

/* DEBUG */
	io_vec = ioMaxXferSize/maxBytesPerXfer + 1;

	if (Verbose) {
	    fprintf(stderr, "ioVecCount forces max dma size to be exceeded\n");
	    fprintf(stderr, "\ttry an ioVecCount of %d\n", io_vec);
	    fprintf(stderr, "\tor increase max dma size by %d pages\n", 
			num_pages);
	}
	return(DM_FAILURE);
    }

    videoData = (struct iovec *) malloc(ioVecCount * sizeof(struct iovec));
    imageData = (ioVecParams_p) malloc(ioVecCount * sizeof(ioVecParams_t));

    if (Verbose) {
	fprintf(stderr, "memory alignment = %d\n", ioAlignment);
	fprintf(stderr, "offset to first image = %lld\n", mvFileOffset);
	fprintf(stderr, "I/O block size = %d\n", ioBlockSize);
	fprintf(stderr, "max bytes per I/O xfer = %d\n", maxBytesPerXfer);
	fprintf(stderr, "using scatter-gather, count = %d\n", ioVecCount);
    }
    return(DM_SUCCESS);
}


static int
initialize_movie_file(MVid *movie)
{
    DMparams * movie_user_data;
    DMparams * movie_params;
    char * buffer;
   
    /*  round file offset to DIO miniosz */
    if ((mvFileOffset != 0) && (mvFileOffset % ioBlockSize))
      {
      mvFileOffset = ((mvFileOffset / ioBlockSize) + 1) * ioBlockSize;
      if (Verbose)
	fprintf(stderr, "aligned offset to first image = %lld\n", mvFileOffset);
      }

    if (dmParamsCreate(&movie_params ) == DM_FAILURE) {
        fprintf(stderr, "unable to create default params.\n");
        return(DM_FAILURE);
    }

    if (mvSetMovieDefaults(movie_params, mvFormat) == DM_FAILURE) {
        fprintf(stderr, "unable to set default params.\n");
        dmParamsDestroy(movie_params);
        return(DM_FAILURE);
    }

    if (ioFileName != NULL)
	dmParamsSetString(movie_params, MV_TITLE, ioFileName);

    if (mvCreateFD(ioFileFD, movie_params, NULL, movie) == DM_FAILURE) {
	fprintf(stderr, "unable to open movie file.\n");
	 return(DM_FAILURE);
    }

    dmParamsDestroy(movie_params);

    /* save the I/O blocksize so that we can check it when we read the file */
    mvGetMovieUserDataListHandle(*movie, &movie_user_data);
    dmParamsSetInt(movie_user_data, MV_IO_BLOCKSIZE, ioBlockSize);

    if (setUpImageTrack(*movie) == DM_FAILURE)
        return(DM_FAILURE);

    /* if the user requests audio to be embedded in the QT file */
    if (alFileFormat == QUICKTIME_AUDIO)
	if (setUpAudioTrack(*movie) == DM_FAILURE)
	    return(DM_FAILURE);

    return(DM_SUCCESS);
}


void
video_DIVO_recorder(void *arg)
{
    VLEvent event;
    int i, done=0, num_events=0;
    int retval=0;
    int iov_index=0;		/* writev vector count */
    int dmbuffer_index=0;	/* index of the active DM buffer */
    static off64_t frame_offset;
    char vlan_resp[1];
    int VLTCEcount = 0;
    struct iovec *iovec_p;
    ioVecParams_p imginf_p;
    int idump;
    stamp_t dummy;
    USTMSCpair checkit;

    prctl(PR_TERMCHILD);
    sigset(SIGHUP, cleanup_video);

    frame_offset = mvFileOffset;
    if (Verbose)
	fprintf(stderr, "starting the video recorder\n");
   
    /* if we are using a V-LAN trigger wait for the coincidence pulse,
     * this will not be as accurate as a GPI trigger
     */
    if (vlanDevice) {
	if (vlanCoincidence) {
	    if (Verbose)
	      fprintf(stderr, "Waiting for coincidence pulse\n");
	    do {
		/* ignore return value for now */
		vlan_read_byte(vlanDevice, vlan_resp); 
		if (Debug)
		  {
		  fprintf(stderr, ".");
		  fflush(stderr);
		  }
	    } while(vlan_resp[0] != '$');

	    if (Verbose) {
		fprintf(stderr, "coincidence pulse received\n");

		if (sendVLANCmd("LR") == DM_SUCCESS) {
		   SMPTE_to_INT(vlanResp);
 		   fprintf(stderr, "begin transfer at %s\n", vlanResp);
		}
		else
 	            fprintf(stderr, "error gettting deck location\n");
	    }
	}
    }

    if (vlBeginTransfer(vlServer, vlPath, 1, &vlXferDesc)) {
	fprintf(stderr, "cannot begin transfer: %s\n", vlStrError(vlErrno));
	exit(DM_FAILURE);
    }

    while (!done) {
	num_events = vlPending(vlServer);
 	if (num_events) {
            if (vlNextEvent(vlServer, &event) < 0) {
                fprintf(stderr, "cannot get next event: %s\n",
                    vlStrError(vlErrno));
                exit(DM_FAILURE);
            }

	    switch(event.reason) {

		case VLStreamStarted:
		    fprintf(stderr, "VL stream started\n");
                    break;

		case VLTransferComplete:
		    VLTCEcount++;
#if (MGV || DIVO)
/* we have to do this because vlDMBufferGetValid is not defined in 6.3 */
		    while (((retval = vlDMBufferGetValid(vlServer, vlPath, vlMemNode, &dmBuffers[dmbuffer_index])) != VLSuccess) && (vlErrno == VLAgain))
			sginap(1);	

		    if (retval == VLSuccess) {

			if (Debug)
			    fprintf(stderr,
				    "VL transfer complete %d %d %d %d\n",
				    VLTCEcount, vlXferCount,
				    event.vltransfercomplete.serial,
				    event.vltransfercomplete.msc);

			if (vlshowVITC)
			  {
			  if ((cur_tc = getVITC(dmBuffers[dmbuffer_index])) < 0)
			    {
			    fprintf(stderr, "VITC in buffer is invalid\n");
			    /* exit(DM_FAILURE); */
			    }
			  if (vlVITCsync)
			    {
			    if (cur_tc < vlanINTInPoint)
			      {
			      if (Verbose)
				fprintf(stderr,
					"Current frame %d [%s] < vlanINTInPoint %d [%s]\n",
					cur_tc, strdup(INT_to_SMPTE(cur_tc)),
					vlanINTInPoint, strdup(INT_to_SMPTE(vlanINTInPoint)));
			      vlSkipImages++;
			      if (vlSkipImages > vlPadImages)
				{
				fprintf(stderr,
					"Skipped frame count %d > pre-roll allowance %d\n",
					vlSkipImages, vlPadImages);
				exit(DM_FAILURE);
				}
			      dmBufferMapData(dmBuffers[dmbuffer_index]);
			      dmBufferFree(dmBuffers[dmbuffer_index]);
			      break;
			      }
			    if (cur_tc > vlanINTOutPoint)
			      {
			      if (Verbose)
				fprintf(stderr,
					"Current frame %d [%s] > vlanINTOutPoint %d [%s]\n",
					cur_tc, strdup(INT_to_SMPTE(cur_tc)),
					vlanINTOutPoint, strdup(INT_to_SMPTE(vlanINTOutPoint)));
			      goto XferComplete;
			      }
			    }
			  }

			/* if first image get timing information */
			if (!vlXferCount)  {
			    vlFirstUSTMSC = dmBufferGetUSTMSCpair(dmBuffers[dmbuffer_index]);
#if 0
fprintf(stderr, "vlXferCount : %d\n", vlXferCount);
fprintf(stderr, "dmbuffer_index : %d\n", dmbuffer_index);
fprintf(stderr, "vlFirstUSTMSC.ust : %lld\n", vlFirstUSTMSC.ust);
fprintf(stderr, "vlFirstUSTMSC.msc : %lld\n", vlFirstUSTMSC.msc);
dmGetUST((unsigned long long *)(&dummy));
fprintf(stderr, "VL dummy UST : %lld\n", dummy);
vlGetUSTMSCPair(vlServer, vlPath, vlVidNode, VL_ANY, vlMemNode, &checkit);
fprintf(stderr, "checkit.ust : %lld\n", checkit.ust);
fprintf(stderr, "checkit.msc : %lld\n", checkit.msc);
#endif
			}

			iovec_p = &videoData[iov_index];
			imginf_p = &imageData[iov_index++];

			iovec_p->iov_base =
				    dmBufferMapData(dmBuffers[dmbuffer_index]);
			imginf_p->offset = frame_offset;
			imginf_p->image_size = (vlRiceEnable) ?
				    dmBufferGetSize(dmBuffers[dmbuffer_index]) :
				    vlBytesPerImage;
			imginf_p->aligned_size = (vlRiceEnable) ?
				    ((imginf_p->image_size + ioBlockSize - 1) /
				    ioBlockSize) * ioBlockSize :
				    maxBytesPerXfer;
			iovec_p->iov_len  = imginf_p->aligned_size;
			total_bytes += imginf_p->aligned_size;
			frame_offset += imginf_p->aligned_size;
			imginf_p->index = dmbuffer_index;

	 		/* increment the buffer index for the next image */
			dmbuffer_index = (dmbuffer_index+1) % dmBufferPoolSize;

			/* write data to disk when we have enough I/O vectors */
			if (iov_index == ioVecCount) {

			    if (vlBenchmark)
				gettimeofday(&BeginTime, NULL);

			    if (lseek64(ioFileFD, imageData[0].offset, SEEK_SET)
					!= imageData[0].offset)
				exit(DM_FAILURE);
#if 0
fprintf(stderr, "writev\n");

for (idump = 0; idump < ioVecCount; idump++)
{
fprintf(stderr, "idump : %d\n", idump);
fprintf(stderr, "videoData[idump].iov_base : %lld\n", videoData[idump].iov_base);
fprintf(stderr, "videoData[idump].iov_len : %lld\n", videoData[idump].iov_len);
fprintf(stderr, "imageData[idump].offset : %lld\n", imageData[idump].offset);
fprintf(stderr, "imageData[idump].image_size : %ld\n", imageData[idump].image_size);
fprintf(stderr, "imageData[idump].aligned_size : %ld\n", imageData[idump].aligned_size);
fprintf(stderr, "imageData[idump].index : %d\n", imageData[idump].index);
}
#endif
			    if (writev(ioFileFD, videoData, ioVecCount) < 0)
				exit(DM_FAILURE);

			    for (i = 0; i < ioVecCount; i++)
				dmBufferFree(dmBuffers[imageData[i].index]);

			    if (write_qt_offset_data(vlXferCount - iov_index + 1,
						      ioVecCount) == DM_FAILURE)
				exit(DM_FAILURE);

			    if (vlBenchmark) {
				gettimeofday(&EndTime, NULL);
				DiskTransferTime += (
				((double) EndTime.tv_usec/DISK_BYTES_PER_MB) +
				EndTime.tv_sec) - (
				((double) BeginTime.tv_usec/DISK_BYTES_PER_MB)+
				BeginTime.tv_sec);
			    }

			    /* reset the I/O vector index */
			    iov_index = 0;
			}
			vlXferCount++;
#if SRV_FICUS_COMPAT 
			if ((vlXferCount == vlNumImages) && (isEVO ||isSRV))
			    goto XferComplete;
#endif
		    }
		    else {
                        fprintf(stderr, "could not get valid buffer!\n");
             		exit(DM_FAILURE);
		    }
#endif
                    break;

		case VLSyncLost:
                    fprintf(stderr, "WARNING: sync signal lost!\n");
		    break;

		case VLTransferFailed:
                    fprintf(stderr, "Transfer failed on image %d!\n", vlXferCount);
           	    exit(DM_FAILURE);

		case VLStreamPreempted:
                    fprintf(stderr, "Stream pre-empted!\n");
           	    exit(DM_FAILURE);

		case VLSequenceLost:
		    if (vlAbortOnSequenceLost) {
                	fprintf(stderr, "Sequence lost @ %d (%d frames dropped) aborting!\n",
				event.vlsequencelost.number,
				event.vlsequencelost.count);
         		exit(DM_FAILURE);
		    }
		    else
                	fprintf(stderr, "Sequence lost @ %d (%d frames dropped)!\n",
				event.vlsequencelost.number,
				event.vlsequencelost.count);
		    break;

		case VLStreamStopped:

#if SRV_FICUS_COMPAT
XferComplete:
#endif
		    /* we have finished successfully so set the done flag */
		    done = 1;

		    /* If the last transfer event fell on
		     * an ioVec boundary then we do not need to write any
		     * data but if we have a fragment of I/O vectors then we 
		     * must store the remainder of the images.
		     */
		    if (iov_index != 0) {

			if (vlBenchmark)
			    gettimeofday(&BeginTime, NULL);

			if (lseek64(ioFileFD, imageData[0].offset, SEEK_SET)
				    != imageData[0].offset)
			    exit(DM_FAILURE);

			if (writev(ioFileFD, videoData, iov_index) < 0)
			    exit(DM_FAILURE);

			for (i = 0; i < iov_index; i++)
			    dmBufferFree(dmBuffers[imageData[i].index]);

			if (write_qt_offset_data(vlXferCount - iov_index,
						  iov_index) == DM_FAILURE)
			    exit(DM_FAILURE);

                        if (vlBenchmark) {
                            gettimeofday(&EndTime, NULL);
                            DiskTransferTime += (
                            ((double) EndTime.tv_usec/DISK_BYTES_PER_MB) +
                            EndTime.tv_sec) - (
                            ((double) BeginTime.tv_usec/DISK_BYTES_PER_MB)+
                            BeginTime.tv_sec);
                        }
		    }

		    if (write_qt_file_header() == DM_FAILURE)
			exit(DM_FAILURE);

                    break;

		default:
		    if (Debug) {
			fprintf(stderr, "received unexpect event %s\n",
				vlEventToName(event.reason));
                    }
                    break;
	    }
	}
    }
    /* since the AL has no event loop we force to audio process to terminate
     * when we are done with recording video */
    exit(DM_SUCCESS);
}


void
video_MVP_recorder(void *arg)
{
    VLEvent event;
    int done=0;
    int iov_index=0;
    int dmbuffer_index=0;
    int i=0, maxfd, pathfd, nready;
    fd_set fdset;
    USTMSCpair dm_ustmsc;
    static off64_t frame_offset;
    struct iovec *iovec_p;
    ioVecParams_p imginf_p;
    char vlan_resp[1];

    prctl(PR_TERMCHILD);
    sigset(SIGHUP, cleanup_video);

    frame_offset = mvFileOffset;
    if (Verbose)
	fprintf(stderr, "starting the video recorder\n");
   
    /* if we are using a V-LAN trigger wait for the coincidence pulse,
     * this will not be as accurate as a GPI trigger
     */
    if (vlanDevice) {
        if (vlanCoincidence) {
            do {
                /* ignore return value for now */
                vlan_read_byte(vlanDevice, vlan_resp);
            } while(vlan_resp[0] != '$');
            if (Verbose) {
                fprintf(stderr, "coincidence pulse received\n");

                if (sendVLANCmd("LR") == DM_SUCCESS) {
                   SMPTE_to_INT(vlanResp);
                   fprintf(stderr, "begin transfer at %s\n", vlanResp);
                }
                else
                    fprintf(stderr, "error gettting deck location\n");
            }
        }
    }
	
    if (vlBeginTransfer(vlServer, vlPath, 1, &vlXferDesc) == DM_FAILURE) {
	fprintf(stderr, "cannot begin transfer: %s\n", vlStrError(vlErrno));
	exit(DM_FAILURE);
    }

    if (vlPathGetFD(vlServer, vlPath, &pathfd) == DM_FAILURE) {
	vlPerror("vlPathGetFD");
	exit(DM_FAILURE);
    }
    maxfd = pathfd;

    while(!done) {

	FD_ZERO(&fdset);
	FD_SET(pathfd, &fdset);

	if ((nready = select(maxfd + 1, &fdset, 0, 0, 0)) == DM_FAILURE) {
            perror("select");
       	    exit(DM_FAILURE);
        }

        if (nready == 0)
            continue;
        
        while(vlEventRecv(vlServer, vlPath, &event) == DM_SUCCESS) {

	    switch(event.reason) {

		case VLStreamStarted:
                    break;

		case VLTransferComplete:

                    if (vlEventToDMBuffer(&event, &dmBuffers[dmbuffer_index]) ==
				DM_SUCCESS) {

		    /* MSC is synchronized to field dominanance so we look for 
		     * a proper frame boundary by looking at the first two 
		     * fields and skip one if the boundary is not synchronized!
		     * The skipped field is not counted as part of the total!
		     * We should note this as an error when we are doing frame
		     * accurate capture as this should not happen if we are
		     * truly frame accurate.
		     */

		    dm_ustmsc = dmBufferGetUSTMSCpair(dmBuffers[dmbuffer_index]);
		        if (!vlXferCount) {
			    if (dm_ustmsc.msc % 2) {
			        if (vlFieldDominance == 1)
				    continue;
		            }
		            else {
			        if (vlFieldDominance == 2)
				    continue;
			    }
			    /* record the time if we have a frame boundary */
		            vlFirstUSTMSC = dm_ustmsc;
		        }

			iovec_p = &videoData[iov_index];
			imginf_p = &imageData[iov_index++];

			iovec_p->iov_base =
				    dmBufferMapData(dmBuffers[dmbuffer_index]);
			imginf_p->offset = frame_offset;
			imginf_p->image_size = (vlRiceEnable) ?
				    dmBufferGetSize(dmBuffers[dmbuffer_index]) :
				    vlBytesPerImage;
			imginf_p->aligned_size = (vlRiceEnable) ?
				    ((imginf_p->image_size + ioBlockSize - 1) /
				    ioBlockSize) * ioBlockSize :
				    maxBytesPerXfer;
			iovec_p->iov_len  = imginf_p->aligned_size;
			total_bytes += imginf_p->aligned_size;
			frame_offset += imginf_p->aligned_size;
			imginf_p->index = dmbuffer_index;

	 		/* increment the buffer index for the next image */
			dmbuffer_index = (dmbuffer_index+1) % dmBufferPoolSize;

			/* write data to disk when we have enough I/O vectors */
			if (iov_index == ioVecCount) {

                            if (vlBenchmark)
                                gettimeofday(&BeginTime, NULL);

			    if (lseek64(ioFileFD, imageData[0].offset, SEEK_SET)
					!= imageData[0].offset)
				exit(DM_FAILURE);
#if 0
fprintf(stderr, "writev\n");

for (idump = 0; idump < ioVecCount; idump++)
{
fprintf(stderr, "idump : %d\n", idump);
fprintf(stderr, "videoData[idump].iov_base : %lld\n", videoData[idump].iov_base);
fprintf(stderr, "videoData[idump].iov_len : %lld\n", videoData[idump].iov_len);
fprintf(stderr, "imageData[idump].offset : %lld\n", imageData[idump].offset);
fprintf(stderr, "imageData[idump].image_size : %ld\n", imageData[idump].image_size);
fprintf(stderr, "imageData[idump].aligned_size : %ld\n", imageData[idump].aligned_size);
fprintf(stderr, "imageData[idump].index : %d\n", imageData[idump].index);
}
#endif
			    if (writev(ioFileFD, videoData, ioVecCount) < 0)
				exit(DM_FAILURE);

			    for (i = 0; i < ioVecCount; i++)
				dmBufferFree(dmBuffers[imageData[i].index]);

			    if (write_qt_offset_data(vlXferCount - iov_index + 1,
						      ioVecCount) == DM_FAILURE)
				exit(DM_FAILURE);

                            if (vlBenchmark) {
                                gettimeofday(&EndTime, NULL);
                                DiskTransferTime += (
                                ((double) EndTime.tv_usec/DISK_BYTES_PER_MB) +
                                EndTime.tv_sec) - (
                                ((double) BeginTime.tv_usec/DISK_BYTES_PER_MB)+
                                BeginTime.tv_sec);
                            }

                            /* reset the I/O vector index */
                            iov_index = 0;
                        }
                        vlXferCount++;
                    }
                    else {
                        fprintf(stderr, "could not get valid buffer!\n");
                        exit(DM_FAILURE);
                    }

                    break;

                case VLSyncLost:
                    fprintf(stderr, "WARNING: sync signal lost!\n");
                    break;

		case VLTransferFailed:
                    fprintf(stderr, "Transfer failed on image %d!\n",vlXferCount);
           	    exit(DM_FAILURE);

                case VLStreamPreempted:
                    fprintf(stderr, "Stream pre-empted!\n");
                    exit(DM_FAILURE);

                case VLSequenceLost:
                    if (vlAbortOnSequenceLost) {
                        fprintf(stderr, "Sequence lost aborting!\n");
                        exit(DM_FAILURE);
                    }
                    else
                        fprintf(stderr, "Sequence lost!\n");
                    break;

		case VLStreamStopped:

                    /* we have finished successfully so set the done flag */
                    done = 1;

                    /* If the last transfer event fell on
                     * an ioVec boundary then we do not need to write any
                     * data but if we have a fragment of I/O vectors then we
                     * must store the remainder of the images.
                     */
 
		    if (iov_index != 0) {

			if (vlBenchmark)
			    gettimeofday(&BeginTime, NULL);

			if (lseek64(ioFileFD, imageData[0].offset, SEEK_SET)
				    != imageData[0].offset)
			    exit(DM_FAILURE);

			if (writev(ioFileFD, videoData, iov_index) < 0)
			    exit(DM_FAILURE);

			for (i = 0; i < iov_index; i++)
			    dmBufferFree(dmBuffers[imageData[i].index]);

			if (write_qt_offset_data(vlXferCount - iov_index,
						  iov_index) == DM_FAILURE)
			    exit(DM_FAILURE);

                        if (vlBenchmark) {
                            gettimeofday(&EndTime, NULL);
                            DiskTransferTime += (
                            ((double) EndTime.tv_usec/DISK_BYTES_PER_MB) +
                            EndTime.tv_sec) - (
                            ((double) BeginTime.tv_usec/DISK_BYTES_PER_MB)+
                            BeginTime.tv_sec);
                        }
                    }

		    if (write_qt_file_header() == DM_FAILURE)
			    exit(DM_FAILURE);

                    break;

		default:
		    if (Debug) {
			fprintf(stderr, "received unexpect event %s\n",
				vlEventToName(event.reason));
                    }
                    break;
            }
	}
    }
    exit(DM_SUCCESS);
}


static int
write_qt_offset_data(int start_frame_index, int image_cnt)
{
    int i,
	j,
	k,
	imax = image_cnt,
	base_frame = start_frame_index,
	mv_frame_index,
	mv_bytes_per_image;
    MVframe mv_dummy_offset;

#if 0
fprintf(stderr, "start_frame_index : %d\n", start_frame_index);
fprintf(stderr, "image_cnt : %d\n", image_cnt);
#endif
    /*	handle quicktime's dainbramaged split-field frame format */
    if (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED)
      {
      imax >>= 1;
      base_frame >>= 1;
      }
      
#if 0
fprintf(stderr, "imax : %d\n", imax);
fprintf(stderr, "base_frame : %d\n", base_frame);
#endif

    for (i = 0; i < imax; i++) {

	j = i + base_frame;
	k = (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED) ? i << 1 : i;
	
	mv_bytes_per_image = (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED) ?
		  imageData[k].aligned_size + imageData[k + 1].aligned_size :
		  imageData[k].image_size;
	
#if 0
fprintf(stderr, "i : %d\n", i);
fprintf(stderr, "j : %d\n", j);
fprintf(stderr, "k : %d\n", k);
fprintf(stderr, "mv_bytes_per_image : %d\n", mv_bytes_per_image);
fprintf(stderr, "imageData[k].offset : %lld\n", imageData[k].offset);
#endif

	if (mvInsertTrackDataAtOffset(
		mvImageTrack,
		1,
		(MVtime) (j * mvFrameTime),
		(MVtime)  mvFrameTime,
		mvImageTimeScale,
		imageData[k].offset,
		mv_bytes_per_image,		/* absolute size of frame */
		MV_FRAMETYPE_KEY,
		0)
		== DM_FAILURE) {

	    fprintf( stderr, "%s\n", mvGetErrorStr(mvGetErrno()) );
	    return(DM_FAILURE);
	}

	if (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED) {

	    /* get the index for the libmovie data corresponding to this field.
 	     * this is necessary in order to set the gap and field sizes for the
 	     * fields in the frame.
 	     */
	    if (mvGetTrackDataIndexAtTime(
			mvImageTrack,
			(MVtime) (j * mvFrameTime),
			mvImageTimeScale,
			&mv_frame_index,
			&mv_dummy_offset)
			== DM_FAILURE) {

		fprintf(stderr, "%s\n", mvGetErrorStr(mvGetErrno()));
		return(DM_FAILURE);
	    }

	    /* tell libmovie the field gap and sizes for each field in the frame
             */
	    mvFieldGap = imageData[k].aligned_size - imageData[k].image_size;
#if 0
fprintf(stderr, "mvFieldGap : %d\n", mvFieldGap);
fprintf(stderr, "imageData[k].image_size : %d\n", imageData[k].image_size);
fprintf(stderr, "imageData[k + 1].image_size : %d\n", imageData[k + 1].image_size);
#endif

	    if (mvSetTrackDataFieldInfo(
			mvImageTrack,
			mv_frame_index,
			imageData[k].image_size,      /* absolute size of field 1 */
			mvFieldGap,		      /* gap between fields */
			imageData[k + 1].image_size)  /* absolute size of field 2 */
			== DM_FAILURE) {

		fprintf(stderr, "%s\n", mvGetErrorStr(mvGetErrno()));
		return(DM_FAILURE);
	    }

	    if (Verbose && Debug) {
		fprintf(stderr, "writing data for split field pair #%d\n", i);
		fprintf(stderr, "field gap = %lld\n", mvFieldGap);
	    }
	}
	else {
	    if (Verbose && Debug)
		fprintf(stderr, "writing data for frame %d\n", i);
	}
    }
    images_xfered += image_cnt;
    return(DM_SUCCESS);
}


static int
getVITC(DMbuffer dmbuf)
{
    int i, max_vitc, j, int_vitc;
    DMBufferVideoInfo vi;
    char hr_str[4], mn_str[4], sc_str[4], fr_str[4], ef_str[4];

    if (vlDMBufferGetVideoInfo(dmbuf, &vi) != VLSuccess) {
	fprintf(stderr, "vlDMBufferGetVideoInfo failed\n");
	return (DM_FAILURE);
    }
    max_vitc = (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED) ? 1 : 2;
    for (i=0; i<max_vitc; i++) {
      /* convert the timecode to integer - return -1 if invalid */
      if (!i)
	{
	int_vitc = (vi.validinfo & DMBUF_VIDINFO_VALID_VITC1) ?
		      (vi.vitc[i].tc.hour * vlanFramesPerHour +
		       vi.vitc[i].tc.minute * vlanFramesPerMin +
		       vi.vitc[i].tc.second * vlanFramesPerSec +
		       vi.vitc[i].tc.frame) : -1;
	}

      /* print out basic timecode */
      if (RANGE(vi.vitc[i].tc.hour, 0, 23))
	  sprintf(hr_str, "**");
	else
	  sprintf(hr_str, "%02d", vi.vitc[i].tc.hour);
      if (RANGE(vi.vitc[i].tc.minute, 0, 59))
	  sprintf(mn_str, "**");
	else
	  sprintf(mn_str, "%02d", vi.vitc[i].tc.minute);
      if (RANGE(vi.vitc[i].tc.second, 0, 59))
	  sprintf(sc_str, "**");
	else
	  sprintf(sc_str, "%02d", vi.vitc[i].tc.second);
      if (RANGE(vi.vitc[i].tc.frame, 0, 29))
	  sprintf(fr_str, "**");
	else
	  sprintf(fr_str, "%02d", vi.vitc[i].tc.frame);
      if (RANGE((int)vi.vitc[i].evenField, 0, 1))
	  sprintf(ef_str, "*");
	else
	  sprintf(ef_str, "%1d", vi.vitc[i].evenField);
      fprintf(stderr, "%s:%s:%s%c%s.%s", hr_str, mn_str, sc_str,
	  ((vi.vitc[i].dropFrame) ? '.' : ':'), fr_str, ef_str);

      /* print out other stuff that's in the structure */
      if (Verbose)
	{
	if(!(vi.validinfo & ((i == 0) ? DMBUF_VIDINFO_VALID_VITC1 :
					DMBUF_VIDINFO_VALID_VITC2)))
	  fprintf(stderr, " [invalid] %d", vi.validinfo);
	fprintf(stderr, " : ");
	switch (vi.vitc[i].tc.tc_type & DM_TC_FORMAT_MASK)
	  {
	  case DM_TC_FORMAT_NTSC:
	    fprintf(stderr, "NTSC, ");
	    break;
	  case DM_TC_FORMAT_PAL:
	    fprintf(stderr, "PAL, ");
	    break;
	  case DM_TC_FORMAT_FILM:
	    fprintf(stderr, "FILM, ");
	    break;
	  }
	switch (vi.vitc[i].tc.tc_type & DM_TC_RATE_MASK)
	  {
	  case DM_TC_RATE_24:
	    fprintf(stderr, "24Hz, ");
	    break;
	  case DM_TC_RATE_25:
	    fprintf(stderr, "25Hz, ");
	    break;
	  case DM_TC_RATE_2997:
	    fprintf(stderr, "29.97Hz, ");
	    break;
	  case DM_TC_RATE_30:
	    fprintf(stderr, "30Hz, ");
	    break;
	  }
	switch (vi.vitc[i].tc.tc_type & DM_TC_DROP_MASK)
	  {
	  case DM_TC_NODROP:
	    fprintf(stderr, "non-drop, ");
	    break;
	  case DM_TC_DROPFRAME:
	    fprintf(stderr, "dropframe, ");
	    break;
	  }
	fprintf(stderr, "user bits: ");
	for(j=0; j<4; j++)
	  fprintf(stderr, "%#02X ", vi.vitc[i].userData[j]);
	}
      fprintf(stderr, "\n");
      }
    return(int_vitc);
}


static int
write_qt_file_header(void)
{
    int flags;

    /* if direct I/O mode is enabled we must disable it because the movie
     * library does not do direct I/O
     */
    if (ioFileFD) {
        fsync(ioFileFD);
        flags = fcntl(ioFileFD, F_GETFL);
        flags &= ~FDIRECT;
        if (fcntl(ioFileFD, F_SETFL, flags) < 0) {
            fprintf(stderr, "unable to reset direct I/O file status\n");
	    return(DM_FAILURE);
	}
    }

    /*	seek to BOF to work around movielib bug for nonzero initial
    **  frame offset
    */
    if (lseek64(ioFileFD, 0, SEEK_SET))
	exit(DM_FAILURE);

    if (mvClose(theMovie) == DM_FAILURE) {
	fprintf(stderr, "unable to write movie file header %s\n",
                    mvGetErrorStr(mvGetErrno()));
	return(DM_FAILURE);
    }

   if (Verbose)
	fprintf(stderr, "finished recording movie file %s\n", ioFileName);

    return(DM_SUCCESS);
}


/*********
 *
 * Set up the parameter list for the image track. The properties that 
 * must always be set for an image track are width and height. If the
 * function dmSetImageDefaults() is used, then other properties, such
 * as compression,  require setting only if they differ from the
 * defaults.
 * stole this from /usr/share/src/dmedia/movie/createmovie/createmovieInit.c++
 *********/
static int 
setUpImageTrack( MVid movie )
{
    DMparams *imageTrackParams;

    if (dmParamsCreate( &imageTrackParams ) != DM_SUCCESS) {
	fprintf(stderr, "unable to create image track params.\n");
        return(DM_FAILURE);
    }

    /* in all other instances we use the field size, here we use the frame 
     * size.
     */
    if (dmSetImageDefaults(imageTrackParams, mvXsize, mvYsize, 
			mvPacking) != DM_SUCCESS) {
        fprintf(stderr, "unable to set image defaults.\n");
        dmParamsDestroy(imageTrackParams);
        return(DM_FAILURE);
    }

    /*
     * NOTE: The following is equivalent to the single dmSetImageDefaults()
     *       call above. 
     * 
     * dmParamsSetEnum( imageTrackParams, DM_IMAGE_PACKING, mvPacking );
     * dmParamsSetInt( imageTrackParams, DM_IMAGE_WIDTH, vlXsize );
     * dmParamsSetInt( imageTrackParams, DM_IMAGE_HEIGHT, vlYsize );
     * dmParamsSetFloat( imageTrackParams, DM_IMAGE_RATE, 15.0 );
     * dmParamsSetEnum( imageTrackParams, DM_IMAGE_INTERLACING, 
                        DM_IMAGE_NONINTERLACED );
     * dmParamsSetEnum( imageTrackParams, DM_IMAGE_ORIENTATION,
                        DM_BOTTOM_TO_TOP );
     */

    /* now we set specific parameters based on the input signal,
     * interlacing is important as it will affect how memory is allocated
     * for the movie during playback */ 

    dmParamsSetFloat(imageTrackParams, DM_IMAGE_PIXEL_ASPECT, mvAspectRatio);
    dmParamsSetFloat(imageTrackParams, DM_IMAGE_RATE, mvRate);

    dmParamsSetEnum(imageTrackParams, DM_IMAGE_INTERLACING, mvInterlacing);
    /* we must invert the image to play it on graphics */
    dmParamsSetEnum(imageTrackParams, DM_IMAGE_ORIENTATION, DM_TOP_TO_BOTTOM);

    if (dmParamsSetString(imageTrackParams, DM_IMAGE_COMPRESSION, 
			getCaptureScheme() ) == DM_FAILURE) {
	fprintf(stderr, "unable to set compression.\n");
	return(DM_FAILURE);
    }
    
    /*
     * now add an image track to the movie with the parameters set above
     */
    if (mvAddTrack(movie, DM_IMAGE, imageTrackParams, 
		NULL, &mvImageTrack) == DM_FAILURE) {
        fprintf(stderr, "unable to add image track to movie.\n");
        dmParamsDestroy(imageTrackParams);
        return(DM_FAILURE);
    }

    /* mvImageTimeScale = (MVtimescale) (60 * MV_FRAMETIME+0.5); */
    if (mvSetTrackTimeScale(mvImageTrack, mvImageTimeScale) == DM_FAILURE) {
        fprintf(stderr, "unable to set timescale in image track\n");
        return(DM_FAILURE);
    }

    dmParamsDestroy(imageTrackParams);
    return(DM_SUCCESS);
}


static int 
setUpAudioTrack( MVid movie )
{
    DMparams* audioSettings;
    dmParamsCreate(&audioSettings);
    dmSetAudioDefaults( audioSettings, 
			ALgetwidth(alPortConfig) * 8,
			(double) alSampleRate,
			ALgetchannels(alPortConfig) );

    mvAddTrack(movie, DM_AUDIO, audioSettings, NULL, &(mvAudioTrack));

/* Should we do this?
movie.audioFrameSize = dmAudioFrameSize(audioSettings);
guarantee that the track time scale is exactly the same as the audio rate
mvSetTrackTimeScale(movie, (int) dmParamsGetFloat(movie, DM_AUDIO_RATE))
*/
    dmParamsDestroy(audioSettings);
    return(DM_SUCCESS);
}


/*********
 *
 * Record the requested movie file format
 *
 *********/
static void 
setMovieFormat( char *movieFormat )
{
    if ( strcmp( movieFormat, "sgi_qt" ) == 0 ) {
        mvFormat = MV_FORMAT_QT;
    }
    else {
        fprintf(stderr, "Unknown movie format %s.\n", movieFormat);
        usage();
    }
}


/*********
 *
 * Record the requested capture scheme.
 *
 *********/
static void 
setCaptureScheme( char *captureArg )
{
    if ( strcmp( captureArg, "frames" ) == 0 ) {
        mvCaptureScheme = frames;
	/* this is the default setting but we do it again to be safe */
	vlCaptureMode = VL_CAPTURE_INTERLEAVED;
    }
    else if ( strcmp( captureArg, "fields" ) == 0 ) {
        mvCaptureScheme = fields;
	vlCaptureMode = VL_CAPTURE_NONINTERLEAVED;
    }
    else {
        fprintf( stderr, "Unknown capture scheme %s.\n", captureArg );
        usage();
    }
}


/*********
 *
 * Return the string corresponding to the capture scheme.
 *
 *********/
static char *
getCaptureScheme( void )
{
    switch(mvCaptureScheme) {
        case frames:
        case fields:
            return(vlRiceEnable ? DM_IMAGE_RICE : DM_IMAGE_UNCOMPRESSED);
        case unknown:
            assert(DM_FALSE);
            break;
    }
    return(NULL);    
}


static int
read_vlan_cmd_file(void)
{
    char vlanCmdBuffer[VLAN_CMD_BUF_SIZE];
    char vlanCommand[VLAN_CMD_SIZE];
    char vlanCmdParam[VLAN_PARAM_SIZE];
    int startTime = -1, finishTime = -1;

    /* parse the file until we reach EOF or EOE (end of edit) */ 
    while (fgets(vlanCmdBuffer, VLAN_CMD_BUF_SIZE, vlanFileDesc) != NULL) {
	/* replace the \n with \0 */
	vlanCmdBuffer[strlen(vlanCmdBuffer) - 1] = '\0';
	/* skip blank lines or comments that begin with a # in col 1 */
	if (!strlen(vlanCmdBuffer))
	  continue;
	if (vlanCmdBuffer[0] == '#')
	  continue;

#if 1
	/* the commands should be issued in the proper order, if not the review
	 * or perform command may be issued prematurely.  We protect the user
	 * from this by delaying.  We send the command after reading the file. 
	 */
 	if (!strncmp(vlanCmdBuffer, "RV", 2) ||
	    !strncmp(vlanCmdBuffer, "PF", 2)) {
	    continue;
	}
	else {
	    /* send the command and then check the response */
	    if (sendVLANCmd(vlanCmdBuffer) == DM_FAILURE)
		return(DM_FAILURE);
	}
#else
	if (sendVLANCmd(vlanCmdBuffer) == DM_FAILURE)
	    return(DM_FAILURE);
#endif

        /* separate the command and timecode, note the white space between the
	 * command and timecode is copied into vlanCommand */
        sscanf(vlanCmdBuffer, "%3s%s", vlanCommand, vlanCmdParam);

/* STW */
 	if (!strcmp(vlanCommand, "SI")) {  /* set in-point */

	    if (vlanInPoint)
	      fprintf(stderr,
		"VLAN command file In-point [%s] overriding command line In-point [%s]\n",
		vlanCmdParam, vlanInPoint);
	    vlanInPoint = strdup(vlanCmdParam);
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

	    if (vlanOutPoint)
	      fprintf(stderr,
		"VLAN command file Out-point [%s] overriding command line Out-point [%s]\n",
		vlanCmdParam, vlanOutPoint);
	    vlanOutPoint = strdup(vlanCmdParam);
	    vlanINTOutPoint = SMPTE_to_INT(vlanCmdParam);
	    finishTime = vlanINTOutPoint;
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

	    if (vlanDuration)
	      fprintf(stderr,
		"VLAN command file Duration [%s] overriding command line Duration [%s]\n",
		vlanCmdParam, vlanDuration);
	    vlanDuration = strdup(vlanCmdParam);
	    vlNumImages = SMPTE_to_INT(vlanCmdParam);
	    if (Verbose)
    		fprintf(stderr, "edit duration %s\n", vlanCmdParam);

	    if (Debug) {
		if (sendVLANCmd("RD") == DM_FAILURE) {/* check duration */
    		    fprintf(stderr, "Could not read duration\n");
		    return(DM_FAILURE);
		}
	    }
	}
 	else if (!strcmp(vlanCommand, "CO")) { /* turn coincidence on */
	    
	    vlanCoincidence = 1;
	    if (Verbose)
    		fprintf(stderr, "coincidence pulse enabled\n");

	    if (Debug) {
		if (sendVLANCmd("RO") == DM_FAILURE) {/* verify coincidence on */
    		    fprintf(stderr, "Could not verify coincidence\n");
		    return(DM_FAILURE);
		}
	    }
	}
    }

    /* make sure we have a valid In-point and count */
    if ((vlanINTInPoint < 0 && (vlNumImages < 0 || vlanINTOutPoint < 0)) ||
	(vlanINTOutPoint < 0 && vlNumImages < 0))
      {
      fprintf(stderr, "Must have two of In-point, Duration and Out-point\n");
      exit(DM_FAILURE);
      }
    /* calculate the third parameter if required */
    if (vlanINTInPoint < 0)
	vlanINTInPoint = vlanINTOutPoint - vlNumImages;
      else if (vlNumImages < 0)
	vlNumImages = vlanINTOutPoint - vlanINTInPoint;
      else if (vlanINTOutPoint < 0)
	vlanINTOutPoint = vlanINTInPoint + vlNumImages;

    /* calculate the number of fields from the time code */
    startTime = vlanINTInPoint;
    finishTime = vlanINTOutPoint;
    if (finishTime < startTime) {
	fprintf(stderr, "in-point %d is later than out-point %d \n", 
		startTime, finishTime);
	exit(DM_FAILURE);
    }
    else if (vlanINTInPoint < 0) {
	fprintf(stderr, "in-point %d < 0\n", vlanINTInPoint);
	exit(DM_FAILURE);
    }
    else if (vlanINTOutPoint < 0) {
	fprintf(stderr, "out-point %d < 0\n", vlanINTOutPoint);
	exit(DM_FAILURE);
    }
    else {
	if (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED)
	    vlNumImages *= 2;
    }

    return(DM_SUCCESS);
}


static unsigned int
SMPTE_to_INT(char *SMPTEtc)
{
    int hh, mm, ss, ff;
/* DEBUG make one error check */
    /* check time format for No Drop Frame and Drop Frame modes */
    if ((sscanf (SMPTEtc, "%u:%u:%u:%u",&hh, &mm, &ss, &ff) != 4) && 
	(sscanf (SMPTEtc, "%u:%u:%u.%u",&hh, &mm, &ss, &ff) != 4)) {
	fprintf (stderr, "\nImproper SMPTE time format: %s\n\n", SMPTEtc);
	exit(DM_FAILURE);
    }
    if (RANGE(hh, 0, 23) || RANGE(mm, 0, 59) || RANGE(ss, 0, 59) ||
	RANGE(ff, 0, (vlanFramesPerSec-1))) {
	fprintf (stderr, "\nImproper value in SMPTE time value: %s\n\n", SMPTEtc);
	exit(DM_FAILURE);
    }
    /* calculate time in FRAMES */
    return(hh * vlanFramesPerHour + mm * vlanFramesPerMin + ss * vlanFramesPerSec + ff);
}

static unsigned int
SMPTE_to_DMTC(char *SMPTEtc, DMtimecode * DMTC_p)
{
    int hh, mm, ss, ff;
/* DEBUG make one error check */
    /* check time format for No Drop Frame and Drop Frame modes */
    if ((sscanf (SMPTEtc, "%u:%u:%u:%u",&hh, &mm, &ss, &ff) != 4) && 
	(sscanf (SMPTEtc, "%u:%u:%u.%u",&hh, &mm, &ss, &ff) != 4)) {
	fprintf (stderr, "\nImproper SMPTE time format: %s\n\n", SMPTEtc);
	exit(DM_FAILURE);
    }
    if (RANGE(hh, 0, 23) || RANGE(mm, 0, 59) || RANGE(ss, 0, 59) ||
	RANGE(ff, 0, (vlanFramesPerSec-1))) {
	fprintf (stderr, "\nImproper value in SMPTE time value: %s\n\n", SMPTEtc);
	exit(DM_FAILURE);
    }
    DMTC_p->hour = hh;
    DMTC_p->minute = mm;
    DMTC_p->second = ss;
    DMTC_p->frame = ff;
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


static void
benchmark_results(void)
{
    /* remember that the number of bytes per field is rounded up to the nearest
     * block size for the actual I/O transfer size
     */
    double total_megabytes = total_bytes / DISK_BYTES_PER_MB;
    double megabyte_bandwidth = (double) maxBytesPerXfer / DISK_BYTES_PER_MB;
    int rate;
    char *img_type;

    if ((total_bytes <= 0) || (images_xfered <= 0)) {
        fprintf(stderr, "cannot compute benchmark results\n");
	return;
    }

    rate = (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED) ? 2 : 1;
    img_type = (vlCaptureMode == VL_CAPTURE_NONINTERLEAVED) ? "fields" :
							      "frames";

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
    fprintf(stderr, "\t# of %s transfered = %d\n", img_type, images_xfered);
    fprintf(stderr, "\t# of %s dropped = %d\n", img_type, vlDropCount);
    fprintf(stderr, "\tMax %s transfer size = %d bytes\n", img_type,
	    maxBytesPerXfer);
    fprintf(stderr, "\tMax I/O transfer size = %d bytes\n",
	    ioVecCount * maxBytesPerXfer);
    fprintf(stderr,
	    "\tMin required disk transfer rate for Max I/O transfer size = %2.3lf MB/s\n",
	    megabyte_bandwidth);
    fprintf(stderr, "\tTotal data volume = %2.3lf MB\n", total_megabytes);
    fprintf(stderr, "\tTotal transfer time = %f sec.\n", DiskTransferTime);
    fprintf(stderr, "\tAverage disk transfer rate = %2.3f MB/s\n",
        total_megabytes/DiskTransferTime);
}

