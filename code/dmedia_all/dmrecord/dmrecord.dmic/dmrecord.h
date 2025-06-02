/**********************************************************************
*
* File: dmrecord.h
*
**********************************************************************/

#ifndef _DMRECORD_H
#define _DMRECORD_H

/*
#define CACHEHACK
*/

typedef int Boolean;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define SUCCESS 0
#define FAILURE -1
#define INTERRUPTED 1

#include <assert.h>
#include <bstring.h>    /* for bcopy */
#include <math.h>       /* for floor */
#include <stdlib.h>     /* for exit, atoi */
#include <string.h>     /* for strcmp */
#include <unistd.h>     /* for sginap */
#include <sys/statvfs.h>
#include <sys/schedctl.h>
#include <libgen.h>     /* for basename */
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>

#include <sys/dmcommon.h>
#include <dmedia/dm_buffer.h>
#include <dmedia/audio.h>
#include <dmedia/vl.h>
#include <dmedia/vl_ev1.h>
#include <dmedia/cl.h>
#include <dmedia/cl_cosmo.h>
#include <dmedia/moviefile.h>
#include <dmedia/dm_image.h>
#include <dmedia/dmedia.h>
#include <sys/prctl.h>
#include <dmedia/dm_imageconvert.h>

#define BILLION	1000000000.0

typedef struct
{
    int       height;		/* image height (0 = undef) */
    char*     videoDevice;	/* where to get the video */
    char*     videoPortName;	/* which port on the video device */
    int	      videoPortNum;
    int	      nodeKind;		/* the kind of video device node */
				/* VL_VIDEO for now. */
    int       critical;		/* abort if any frames are dropped */
    int       verbose;		/* print interesting messages */
    char*     fileName;		/* movie file name to create */
    char*     movieTitle;	/* title to put in movie file (NULL = undef) */
    Boolean   video;		/* record video? */
    Boolean   audio;		/* record audio with the video? */
    int       audioChannels;	/* number of audio channels to record */
    long long seconds;		/* How long to record */
    Boolean   batchMode;	/* use batch mode? */
    char*     compressionScheme;/* video compression */
    char*     compressionEngine;/* what does the compression? */
    int	      qualityFactor;	/* from 0 (bad) to 100 (good) */
    int       halfx;		/* half width flag */
    int       halfy;		/* half height flag */
    int	      avrFrameRate;	/* Avrage Frame Rate */
    int	      frames;
    int	      unit;		/* UNIT_FRAME or UNIT_FIELD */
    int		inbufs;
    int		outbufs;
    int		hiPriority;
    int		nowrite;
    int		timing;
    int		use30000;       /* True if we should do 30000/1001 */
} Options;

#define UNIT_FRAME 1
#define UNIT_FIELD 0

typedef struct dummyFrame_t{
	int field1size;
	int field2size;
} dummyFrame_s;

typedef struct
{
    int		channels;
    int		width;
    double	rate;
    ALport	port;
    long long	startTime;	/* The time at which the first audio */
				/* frame was captured. */
    int		framesToDrop;	/* For syncronization at the beginning */
				/* of the movie.  This is the number */
				/* of audio frames to drop before */
				/* recording begins.  Can be negative */
				/* to indicate that silent frames */
				/* should be inserted. */
} AudioState;

typedef struct
{
    VLServer	server;
    VLDev	device;
    int		videoPort;
    VLPath	path;
    VLNode	source;
    VLNode	drain;
    int		width;
    int		height;
    double	rate;
    int		frames;
    int		xferbytes;
    DMimageconverter	ic; 
    int		icfd;
    int		fdominance;
    int		timing;
    int		originX;
    int		originY;
    long	waitPerBuffer;	/* microseconds to wait for a buffer (frame */
				/* or field) to arrive */
} VideoState;

typedef struct
{
    MVid	movie;
    MVid	imageTrack;
    MVid	audioTrack;
    int		audioFrameSize;
    int		lastOddFieldSize;
    int		lastEvenFieldSize;
    MVtimescale timeScale;
    MVtime perFrameTime;
    MVfileformat format; 

} MovieState;

/* name of the new video device */
#define MVP     "mvp"
#define EV1	"ev1"
#define VICE    "ice"
#define IMPACT  "impact"

extern VLDevice UseThisDevice;
#define HAS_GOOD_DEVICE (UseThisDevice.numNodes != -1)
#define HAS_GOOD_NODES (UseThisDevice.numNodes > 0)

#define MAXNODES 50
extern VLNodeInfo UseThisNode[MAXNODES];

extern char *UseThisEngine; /* "dmIC" or "cosmo" */
extern int usedmIC;
#define USEDMIC	(usedmIC == 1)
#define USECOSMO	(usedmIC == 0)
#define USECL	(usedmIC == 0)

extern int UseThisdmIC; /* the dmIC number of the realtime jpeg compressor */
extern int dmICStatus;
#define HAS_GOOD_DMIC (dmICStatus == DM_SUCCESS)

extern int CosmoStatus;
#define HAS_GOOD_COSMO (CosmoStatus == SUCCESS)

extern int useMVP;
extern int useIMPACT;
#define USEMVP	(useMVP == 1)
#define USEEV1	(useMVP == 0)
#define USEIMPACT (useIMPACT == 1)

extern int screenPortNum;
extern char *screenPortName;

extern void capture( void );

extern void usage(int);
extern void parseArgs(int, char **);
extern void vexit(int);

extern int getSpecifiedPort(void);

extern int videoTransferStarted;
extern Options options;
extern MovieState movie;
extern AudioState audio;
extern VideoState video;

#define DO_FRAME	(options.unit == UNIT_FRAME)
#define DO_FIELD	(options.unit != UNIT_FRAME)

#define NEWFORMAT       ((movie.format == MV_FORMAT_QT) && DO_FIELD)
#define OLDFORMAT       ((movie.format == MV_FORMAT_SGI_3) || DO_FRAME)

#define F1DOMINANT	(video.fdominance == 1)
#define F2DOMINANT	(video.fdominance == 2)

#define INBUFS 10
#define OUTBUFS 10

#endif /* _DMRECORD_H */
