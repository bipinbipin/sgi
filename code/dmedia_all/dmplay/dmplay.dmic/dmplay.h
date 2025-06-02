#ifndef __INC_DMPLAY_H__
#define __INC_DMPLAY_H__

/*
 * dmplay.h: header file for dmplay.h
 *
 * Original version by Rajal Shah. (5/24/94)
 *
 */


/*
 * C/POSIX/Irix include files
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <bstring.h>
#include <string.h>
#include <libgen.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/wait.h>
#include <bstring.h>
#include <sys/time.h>
#include <abi_mutex.h>
/*
 * X / GL include files
 */
#include <X11/X.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/keysym.h>
#include <Xm/Xm.h>
#include <Xm/MwmUtil.h>
/*
 * dmedia headers
 */
#include <dmedia/dm_buffer.h>
#include <dmedia/dm_color.h>
#include <dmedia/dm_image.h>
#include <dmedia/vl.h>
#include <dmedia/audio.h>
#include <dmedia/moviefile.h>
#include <dmedia/dm_imageconvert.h>

#define GRAPHICS               10
#define VIDEO_2_FIELD          12
#define NO_DISPLAY		0

#define PLAY_STEP 0
#define PLAY_CONT 1

#define LOOP_NOTSPEC	-1	/* not specified */
#define LOOP_NONE	0	/* no looping - spin on last field at end */
#define LOOP_REPEAT	1	/* repeat looping */
#define LOOP_FRAME      2       /* spin on last frame - 2 fields - at end */

typedef struct {
    char *filename;
    char *title;
    MVloopmode loopMode;
    MVid mv;
    MVid imgTrack;
    MVid audTrack;
    MVfileformat format; /* MV_FORMAT_SGI_3 or MV_FORMAT_QT */
} _Movie;

typedef struct {
    int isTrack;  			/* is there a video ('image') track?*/
    int height;
    int width;
    int cliptop;
    int clipbottom;
    int clipleft;
    int clipright;
    DMinterlacing interlacing;
    DMorientation orientation;
    int display;
    int numberFrames;
    double frameRate;
    MVtimescale timeScale;
    int hasFieldInfo;
    int hasFieldLen;
    float pixelAspect;
} _Image;

typedef struct {
    int loopMode; /* LOOP_NONE or LOOP_REPEAT */
    int audioActive;
    int playMode; /* PLAY_STEP or PLAY_CONT */
    int advanceAudio; 
    int advanceVideo;
    int autoStop; /* 0 if wait for user input, 1 if stop automatically */
    int speedcontrol;
    int timing;
    int syncInterval;
    int gfx_lag; /* in milli-seconds */
    int video_lag; /* in milli-seconds */
    int lubeBuffers;
} _PlayState;

#define GFX_LAG 0
#define VIDEO_LAG 0


typedef struct {
    DMimageconverter id;
    DMicspeed speed; /* DM_IC_SPEED_REALTIME or DM_IC_SPEED_NONREALTIME */
    int		idfd; /* file descriptor for decompressor output*/
    DMbufferpool inpool; /* input pool to decompressor */
    DMbufferpool outpool; /* output pool from decompressor */
    int inbufs; /* number of buffers in input pool */
    int outbufs; /* number of buffers in output pool */
    int inbufsize; /* bytes per buffer */
    int outbufsize;  /* bytes per buffer */
    int outpoolfd; /* file descriptor for outpool */
    int inpoolfd; /* file descriptor for inpool */
    USTMSCpair inustmsc;
    volatile unsigned long long outust;
    volatile unsigned long long outmsc;
    volatile int bufferDone;
    abilock_t lock; /* lock to make sure ust and msc are in sync when read */
} _dmIC;

typedef struct {
    int        isTrack;                   /* is there an audio track ?*/
    ALport     outPort;                   /* audio output port */
    int        frameCount;                /* audio track length */
    int        sampleWidth;               /* audio sample width, in bits */
    int	       frameSize;		  /* bytes per audio frame */
    int        blockSize;                 /* ?? */
    int        channelCount;              /* 1=mono, 2=stereo, etc */
    int        queueSize;                 /* audio output port queue */
    int		preroll;
    double     frameRate;                 /* audio sample rate */
} _Audio;

typedef struct {
    VLServer svr;
    VLDevice  *device;
    VLPath path;		/* video data path from Cosmo */
    VLNode src;
    VLNode scrdrn;		/* screen out */
    VLNode drn;			/* video out */
    int    dataActive;		/* vlBeginTransfer on data path? */
    int    dataFrozen;		/* has VL_FREEZE been called? */
    VLPath timingPath;
    int    timingActive;	/* is the timing path currently active? */
    int    height;
    int    width;
    int	   preroll;
    int	   windowId;		/* window Id of screen node used for impact */
} _Video;


/* 
 * Command line options
 */

typedef struct {
    char *myname;                 /* name of this program */
    int use_default_ports;        /* if no -p options specified by user */
    int display_port_specified;   /* if -p graphics or -p video flag seen */
    int audio_port_specified ;    /* if -p audio flag seen */
    int playAudioIfPresent;       /* set when -p audio flag seen */
    int verbose;		  /* -v flag */
    int initialLoopMode;          /* LOOP_NOTSPEC, LOOP_NONE or LOOP_REPEAT */
    int autostop;                 /* 1 if -E auto, 0 if -E key */
    int initialPlayMode;          /* PLAY_CONT if -P cont,PLAY_STEP if -P step*/
    char *ifilename;              /* input file name */
    char *image_engine;	          /* image track decompression engine */
    int image_port_type;          /* image track display: graphics or video*/
    char *vid_device;	          /* video device name: 'ev1' or 'mvp' */
    int  vid_portnum;             /* video device port num: not used yet */
    float  zoom;                  /* zoom */
    int videoOnly;
    int hiPriority;
    int forceSampleRate;	  /* Force the sample rate to the correct
				   * value. */
} _Options;


extern _dmIC dmID;
extern _PlayState playstate;
extern _Image image;
extern _Movie movie;
extern _Audio audio;
extern _Video video;
extern _Options options;

extern void setscheduling( void );
extern void stopAllThreads( void );
extern void streamDecompress( void );
extern void dmICDecInit(void);
extern void mvInit( void );
extern void alInit( void );
extern void vlInit(void);
extern void vl_start( void );
extern void  x_open( int, int );
extern void deinterlaceImage( void * , void * );
extern void parseargs(int , char *[]);
extern void deinterlaceField( void *, void *, int);
extern char *imageBuffer;

#define ZERO *(int *)(imageBuffer)
#define FIELD1LEN *(int *)(imageBuffer+sizeof(int))
#define FIELD2LEN *(int *)(imageBuffer+2*sizeof(int))
#define HASFIELDLEN (image.hasFieldLen == 1)

int gfx_open(int, int, int, int, int, char *);
void gfx_show(int, int, char *, int);

#define MVP	"mvp"
#define VICE	"ice"
#define DMICSW	"sw"
#define IMPACT	"impact"

extern int hasVideoDevice;
#define HASMVP		(hasVideoDevice == 1)
#define HASIMPACT	(hasVideoDevice == 3)
#define HASNEITHER	(hasVideoDevice == 0)

extern int useVideoDevice;
#define USEVIDEO	((useVideoDevice == 1) || (hasVideoDevice == 3))
#define USEGFX		(useVideoDevice == 0)

/* For the following, -1 means non-existence */
extern int hasViceDec;
extern int hasImpactDec;
extern int hasdmICSWDec;

#define HASVICE		(hasViceDec != -1)
#define HASIMPACTCOMP	(hasImpactDec != -1)
#define HASDMICSW	(hasdmICSWDec != -1)

extern int UseThisdmICDec;

extern int useEngine;
#define USEVICE		(useEngine == 1)
#define USEDMICSW	(useEngine == 2)
#define USEIMPACT	(useEngine == 3)

#define DO_FRAME	(image.interlacing == DM_IMAGE_NONINTERLACED)
#define DO_FIELD	(image.interlacing != DM_IMAGE_NONINTERLACED)
#define NEWFORMAT	(image.hasFieldInfo == 1)
#define OLDFORMAT	(image.hasFieldInfo == 0)

#define SINGLESTEP	(playstate.playMode == PLAY_STEP)

#define A_PORT 1
#define V_PORT 2
#define G_PORT 3

#define FIRST_FIELD		1	/* The number of the first field */
					/* as returned by clGetNextImageInfo */

#define VIDEOPRIME		2	/* Number of frames to send to */
					/* make sure that there is no */
					/* garbage in the frame buffer */
					/* when we start video. */

#define VIDEOPREROLL		4	/* Number of frames to prime output */

#define AUDIOPREROLL		 4 	/* Number of video frame times */
					/* to prime audio output. */

#define NANOS_PER_SEC            1000000000
#define TICKS_PER_SEC	CLK_TCK
#define NANOS_PER_TICK	(NANOS_PER_SEC/TICKS_PER_SEC)
#define NANOS_PER_MILLI  1000000

#define DMICINBUFS 8
#define DMICOUTBUFS  14

#define SYNCINTERVAL 20

#define FRAME_GFX_INBUFS	8
#define FRAME_GFX_OUTBUFS	8
#define FRAME_GFX_VIDEOPREROLL	3
#define FRAME_GFX_AUDIOPREROLL	4

#define FIELD_GFX_INBUFS	8
#define FIELD_GFX_OUTBUFS	8
#define FIELD_GFX_VIDEOPREROLL	3
#define FIELD_GFX_AUDIOPREROLL	4

#define FRAME_VIDEO_INBUFS	4
#define FRAME_VIDEO_OUTBUFS	9
#define FRAME_VIDEO_VIDEOPREROLL 4
#define FRAME_VIDEO_AUDIOPREROLL 7

#define FIELD_VIDEO_INBUFS	8
#define FIELD_VIDEO_OUTBUFS	18
#define FIELD_VIDEO_VIDEOPREROLL 4
#define FIELD_VIDEO_AUDIOPREROLL 7


/********
*
* ERROR - Report an error and exit the program.
*
* This example program has a very simple error handling mechanism.
* When something goes wrong in a call that was not expected to fail, 
* an error message is printed, all executing threads are stopped, and
* the program quits.
*
********/

#define ERROR(message)\
	{\
	    fprintf( stderr, "%s\n", message );\
	    fprintf( stderr, "in file %s, line %d\n", __FILE__, __LINE__);\
	    stopAllThreads();\
	}

/* CERROR for use in getdmICOutput thread */
#define CERROR(message)\
	{\
	    fprintf( stderr, "%s\n", message );\
	    fprintf( stderr, "in file %s, line %d\n", __FILE__, __LINE__);\
	    childPid = -1;\
	    if (parentPid != -1) \
		kill(parentPid, SIGINT);\
	    exit(1);\
	}


#define VERROR(message)	\
	{\
	    vlPerror(message);\
	    fprintf(stderr, "in file %s, line %d\n", __FILE__, __LINE__);\
	    stopAllThreads();\
	}

#endif /* __INC_DMPLAY_H__ */

