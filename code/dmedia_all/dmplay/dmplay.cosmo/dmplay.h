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

/*
 * X / GL include files
 */
#include <X11/X.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/keysym.h>
#include <Xm/Xm.h>
#include <Xm/MwmUtil.h>
#include <Sgm/GlxDraw.h>

/*
 * dmedia headers
 */
#include <dmedia/dm_image.h>
#include <dmedia/cl.h>
#include <dmedia/cl_cosmo.h>
#include <dmedia/cl_impactcomp.h>
#include <dmedia/vl.h>
#include <dmedia/audio.h>
#include <dmedia/moviefile.h>

#define GRAPHICS               10
#define VIDEO_2_FIELD          12

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
    int mem_height;		/* decompressed image height in memory */
    int mem_width;		/* decompressed image width in memory */
    int act_width;		/* movie file image width */
    int externalHeight;		/* size of data out video (codec) port */
    int externalWidth;		/* size of data out video (codec) port */
} _Image;

typedef struct {
    int loopMode; /* LOOP_NONE or LOOP_REPEAT */
    int audioActive;
    int playMode; /* PLAY_STEP or PLAY_CONT */
    int advanceAudio; 
    int advanceVideo;
    int autoStop; /* 0 if wait for user input, 1 if stop automatically */
} _PlayState;

typedef struct {
    CLcompressorHdl Hdl;
    int engine;
    int singleFrame;
    int hardwareInterleave;	/* codec supports hardware interleave */
    int codecPort;		/* port of codec - for multichannel codecs */
} _Codec;

typedef struct {
    int        isTrack;                   /* is there an audio track ?*/
    ALport     outPort;                   /* audio output port */
    int        frameCount;                /* audio track length */
    int        sampleWidth;               /* audio sample width, in bits */
    int	       frameSize;		  /* bytes per audio frame */
    int        blockSize;                 /* ?? */
    int        channelCount;              /* 1=mono, 2=stereo, etc */
    int        queueSize;                 /* audio output port queue */
    double     frameRate;                 /* audio sample rate */
} _Audio;

typedef struct {
    VLServer svr;
    VLPath path;		/* video data path from Cosmo */
    VLNode drn;			/* screen out */
    VLNode voutdrn;		/* video out */
    int    dataActive;		/* vlBeginTransfer on data path? */
    int    dataFrozen;		/* has VL_FREEZE been called? */
    VLPath timingPath;
    int    timingActive;	/* is the timing path currently active? */
    int    height;
    int    width;
    int windowId;		/* window Id of screen node */
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
    char *vid_device;	          /* video device name: 'ev1' for now */
    int  vid_portnum;             /* video device port num: not used yet */
    float  zoom;                  /* zoom */
} _Options;


extern _Codec codec;
extern _PlayState playstate;
extern _Image image;
extern _Movie movie;
extern _Audio audio;
extern _Video video;
extern _Options options;

extern void setscheduling( void );
extern void stopAllThreads( void );
extern void singleFrameDecompress( void );
extern void Xinit( int* argcp, char* argv[] );
extern void clInit( int );
extern void mvInit( void );
extern void alInit( void );
extern void vlInit( void );
extern void Xgo( void );
extern void deinterlaceImage( void* from, void* to );
extern void parseargs(int , char *[]);
extern void initPlayState( void );
extern void XSetVideoOrigin(void);

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

#define ERROR(message)							      \
	{								      \
	    fprintf( stderr, "%s: %s\n", options.myname, message );	      \
	    fprintf( stderr, "%s, line %d\n", __FILE__, __LINE__); 	      \
	    stopAllThreads();						      \
	}

#endif /* __INC_DMPLAY_H__ */
