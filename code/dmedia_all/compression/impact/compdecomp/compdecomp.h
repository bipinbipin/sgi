/*
** compdecomp
** Indigo2 IMPACT Compression example program
**
** Silicon Graphics, Inc, 1996
*/

#ifndef _H_COMPDECOMP
#define _H_COMPDECOMP

#include <sys/types.h>
#include <sys/prctl.h>
#include <signal.h>

#include <stdio.h>
#include <unistd.h>

#include <X11/X.h>
#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <X11/keysym.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Scale.h>
#include <Xm/Label.h>
#include <Xm/RowColumn.h>
#include <Xm/MwmUtil.h>

#include <dmedia/cl.h>
#include <dmedia/cl_impactcomp.h>

#include <dmedia/vl.h>

struct gui_s {
    Display *dpy;
    Widget  toplevel;		    /* whole thing */
    Widget  base;		    /* Form */
    Widget  slider;		    /* Scale */
    Widget  label;		    /* title */
    Widget  ratio;		    /* compression ratio label */
    Widget  actual;		    /* compression ratio label */
    XtAppContext appContext;
};

struct video_s {
    VLServer	svr;		    /* video daemon */
    VLPath	path;		    /* video path */
    VLNode	src;
    VLNode	drn;
};

struct codec_s {
    CLhandle	handle;
    int	    video_port;
    int	    bitrate;
    CLbufferHdl buffer;
};

struct image_s {
    int	    height;
    int	    width;
};

struct global_s {
    struct gui_s gui;
    struct video_s capture;
    struct video_s playback;
    struct codec_s comp;
    struct codec_s decomp;
    struct image_s image;
};

extern struct global_s global;
extern int bytes;
extern int fields;

#define INIT_COMP_RATIO	    7
#define QUEUE_BUFFER_SIZE   (1024 * 1024 * 4)

/* prototypes */
int gui_init(int * argc, char * argv[]);
int cl_comp_open( void );
int cl_decomp_open( void );
int cl_comp_init( void );
int cl_decomp_init( void );
int vl_capture_init( void );
int vl_play_init( void );


#endif	/* _H_COMPDECOMP */
