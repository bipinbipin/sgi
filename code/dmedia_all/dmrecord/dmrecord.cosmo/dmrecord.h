/**********************************************************************
*
* File: dmrecord.h
*
**********************************************************************/

#ifndef _DMRECORD_H
#define _DMRECORD_H

typedef int Boolean;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef struct
{
    int       height;		/* image height (0 = undef) */
    char*     videoDevice;	/* where to get the video */
    int       videoPort;	/* which port on the video device */
    				/* (VL_ANY = undef)*/
    char *    videoPortName;
    int       codecPort;	/* which codec port? */
    int       videoDrain;	/* Drain node type */
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
    MVfileformat format;        /* MV_FORMAT_SGI_3 or MV_FORMAT_QT */
    int	      noNewInfo;        /* no new info for 6.2 compat*/
} Options;

extern int done(void);	/* defined in main.c. It returns 1 when it is */
			/* time to stop recording. */

extern void goAhead(void);/* also in main.c.  Return TRUE when it is */
			/* time to start recording. */

extern void capture( Options* );

extern void usage(int);
extern void parseArgs(int argc, char *argv[], Options*);

extern char* g_ProgramName;
int getSpecifiedPort ( Options *options);



#define MAXNODES 128
extern VLNodeInfo UseThisNode[MAXNODES];
extern VLDevice UseThisDevice;
#endif /* _DMRECORD_H */
