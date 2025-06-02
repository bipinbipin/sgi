/*
 * Xinit.c
 *
 * This file contains two main entry points: Xinit() and Xgo().  Xinit()
 * does Xt toolkit initialization and scans the command line for general
 * X options (described in X(1)).  For instance, you can cause the program
 * to start up as an icon with dmplay -iconic.  The second entry point is
 * Xgo() which is the final initialization step.  It creates the window,
 * sets up Xt call backs, and handles X events.
 * 
 * Silicon Graphics Inc., June 1994
 */

#include "dmplay.h"
#include <ulocks.h>

static void handleGinit(Widget, caddr_t, caddr_t);
static void doIt(void);
static void eventThread(void *arg);
static void handleUser(Widget, XtPointer, XEvent *, Boolean *);
static void handleExpose(Widget, XtPointer, XEvent *, Boolean *);
static void handleMove(Widget, XtPointer, XEvent *, Boolean *);


/*
 * X globals.
 */
static Widget		toplevel;

static XtAppContext	appContext;

static Atom		wmproto,
			delmesg;

static int		winWidth,
			winHeight;

static GLXconfig glxConfig [] = {
    { GLX_NORMAL, GLX_DOUBLE, FALSE },
    { GLX_NORMAL, GLX_RGB, TRUE },
    { GLX_NORMAL, GLX_ZSIZE,  GLX_NOCONFIG },
    { 0, 0, 0 }
};

static glZoom;

/* 
 * Initialize X toplevel widget
 * Parse X and User options
 * Load resources
 */
void
Xinit(int *argc, char *argv[])
{
    Arg args[20];
    int n;

    n=0;

    XtSetArg(args[n], XmNmwmDecorations,
	     MWM_DECOR_ALL | MWM_DECOR_RESIZEH | MWM_DECOR_MAXIMIZE); n++;
    XtSetArg(args[n], XmNmwmFunctions,
	     MWM_FUNC_ALL | MWM_FUNC_RESIZE | MWM_FUNC_MAXIMIZE); n++;
	     
    toplevel = XtAppInitialize(&appContext, "dmplay", 
			       (XrmOptionDescList)NULL, 0,
			       argc, (String*)argv, 
			       NULL,
			       args, n);
}

/*
 * Create a window for display
 *
 * This routine creates a window with a GL drawing context and then
 * starts a new thread to handle interaction with X.
 */
void
Xgo(void)
{
    Arg args[20];
    int n;
    Display *dpy;

    dpy = XtDisplay(toplevel);

    /*
     * We want to get a chance to clean up if the user picks
     * quit/close from the window manager menu.  By setting
     * these flags, we'll get sent a message if that happens.
     */
    wmproto = XInternAtom(dpy, "WM_PROTOCOLS", False);
    delmesg = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

    /*
     * Clipping and zooming for the video case is handled by the vl.
     * For the GL case, we figure it out here.  We also create the GL
     * drawing context if needed.
     */
    if (image.display != GRAPHICS) {
	winHeight = video.height;
	winWidth  = video.width;

	XtAddEventHandler(toplevel, ExposureMask, FALSE, handleExpose, 0);
	XtAddEventHandler(toplevel, SubstructureNotifyMask, FALSE,handleMove,0);
	XtAddEventHandler(toplevel, KeyPressMask, TRUE, handleUser, 0);
    } else {
	Widget		glw;
	/*
	 * impact handles arbitrary scaling down.
	 * The Iris GL can only handle rectzoom of 1 and 2
	 */
	if (options.zoom < 1 && !strcmp(options.image_engine, "impact")) {
	    winHeight = image.mem_height - image.cliptop - image.clipbottom;
	    winWidth = image.mem_width;
	    glZoom = 1;
	} else { 
	    if (options.zoom < 2)
	    	glZoom = 1;
	    else if (options.zoom >= 2)
	        glZoom = 2;
	    winHeight = 
		(image.mem_height - image.cliptop - image.clipbottom) * glZoom;
	    winWidth = image.mem_width * glZoom;
	}

	n = 0;
	XtSetArg(args[n], GlxNglxConfig, glxConfig); n++;
	XtSetArg(args[n], XtNwidth,  winWidth);  n++;
	XtSetArg(args[n], XtNheight, winHeight); n++;
	glw = XtCreateManagedWidget("glx",glxDrawWidgetClass,toplevel,args,n);

	XtAddCallback(glw, GlxNginitCallback, (XtCallbackProc)handleGinit, 0);
	XtAddEventHandler(glw, ExposureMask, FALSE, handleExpose, 0);
	XtAddEventHandler(glw, KeyPressMask, TRUE, handleUser, 0);
    }

    n = 0;
    XtSetArg(args[n], XtNtitle,  movie.title); n++;
    XtSetArg(args[n], XtNwidth,  winWidth);    n++;
    XtSetArg(args[n], XtNheight, winHeight);   n++;
    XtSetValues(toplevel, args, n);

    XtRealizeWidget(toplevel);

    /*
     * Create a child thread to handle X events.  By starting this
     * thread with the "PR_BLOCK" flag, we cause ourselves (the parent)
     * to block (i.e. stop executing) until the child lets us continue
     * via the unblockproc() call in handleExpose()
     */
    if (sproc(eventThread, PR_SADDR | PR_BLOCK | PR_SFDS) == -1) { 
	fprintf(stderr, "Unable to sproc to handle X events\n");
	perror("sproc");
	stopAllThreads();
    }
}

/*
 * Input thread
 *
 * This is a separate thread that receives events from X
 *
 */
/*ARGSUSED*/
static void
eventThread(void *arg)
{
    /*
     * Tell kernel to send us SIGHUP when our parent goes
     * away.
     */
    sigset(SIGHUP, SIG_DFL);
    prctl(PR_TERMCHILD);

    XtAppMainLoop(appContext);
}

/*
 * handleUser - handle input from the user
 *
 * This routine handles keyboard and window manager menu interaction
 */
/*ARGSUSED*/
static void
handleUser(Widget w, XtPointer p, XEvent *ev, Boolean *cflag)
{
    pid_t ppid;

    switch(ev->type) {
      case KeyPress:
	  {
	  KeySym keysym;
	  int buf;
	  
	  XLookupString((XKeyEvent *)ev, (char *)&buf, 1, &keysym, 0);
	  switch (keysym) {
	    case XK_s: /* Single Step */
	      doIt();
	      break;

	    case XK_Escape:
	      if ((ppid = getppid()) == -1) {
		  perror("Unable to find video thread");
	      } else
		  if (ppid != 1)
		      kill ((pid_t)ppid, SIGTERM);
	      stopAllThreads();
	      break;
	  }
	}
	break;

      case ClientMessage:
	  /*
	   * The only messages we expect are ones from the window
	   * manager, and the only one of those we expect is WM_DELETE
	   */
	  if (ev->xclient.message_type == wmproto && 
	      ev->xclient.data.l[0]    == delmesg) {
	      if ((ppid = getppid()) == -1) {
		  perror("Unable to find video thread");
	      } else
		  if (ppid != 1)
		      kill ((pid_t)ppid, SIGTERM);
	      stopAllThreads();
	  }
	  break;
    }
}

/*
 * handleExpose: called during an expose event.
 *
 * This routine is called via XtAppMainLoop() in the routine
 * eventThread() whenever the window needs to be redrawn.  Note that we
 * are now running in a separate (child) thread than the
 * {stream,singleFrame}Decompress code.  We ignore all but the first
 * instance of this event.  On the first expose we complete the set up of
 * the video -> window path by attaching the vl screen node to the X
 * window and then we release the main playback thread.
 */
/*ARGSUSED*/
static void
handleExpose(Widget w, XtPointer p, XEvent *ev, Boolean *cflag)
{
    static int beenhere;
    pid_t ppid;

    /*
     * clear window to black right away
     */
    {
    Display *dpy;
    XGCValues	gcvalue;		/* graphics context value */
    GC gc;				/* X11 graphics context */

    dpy = XtDisplay(w);

    gcvalue.foreground = 0;		/* black */
    gcvalue.fill_style = FillSolid;
    gc = XCreateGC(dpy, XtWindow(w), GCForeground|GCFillStyle, &gcvalue);
    (void) XFillRectangle(dpy, XtWindow(w), gc,0,0, winWidth, winHeight);
    XFlush(dpy);
    }

    if (beenhere)
	return;

    beenhere = 1;

    if (image.display == GRAPHICS)
	doIt();
    else
	video.windowId = (int ) XtWindow(w);
    
    /*
     * Once the window is mapped, vl can begin transfering and we can start the
     * video application, i.e. singleFrameDecompress() or streamDecompress()
     */
    if ((ppid = getppid()) == -1) {
	perror("Unable to find the video thread");
	exit(1);
    }

    if (unblockproc(ppid) == -1) {
	perror("Unable to start the video thread");
	exit(1);
    }
}

/*
 * handleMove: Callback to track the video display, with the displacement
 *		of the X window on the screen.
 */
/*ARGSUSED*/
static void
handleMove(Widget w, XtPointer p, XEvent *ev, Boolean *cflag)
{
    Window dummyWin;
    Display *dpy;
    VLControlValue val;

    if (image.display == GRAPHICS)
	return;

    dpy = XtDisplay(w);
    XTranslateCoordinates(dpy, XtWindow(w),
                          RootWindow(dpy, DefaultScreen(dpy)),
                          0, 0,
                          &val.xyVal.x, &val.xyVal.y,
                          &dummyWin);

    vlSetControl(video.svr, video.path, video.drn, VL_ORIGIN, &val);
}

/*
 * handleGinit: initializes GL widget
 */
/*ARGSUSED*/
static void
handleGinit(Widget w, caddr_t client_data, caddr_t call_data) 
{
    GLXwinset(XtDisplay(w), XtWindow(w));
    pixmode(PM_TTOB, 1);	/* top to bottom mode for lrectwrite() */
    rectzoom(glZoom,glZoom);
    RGBcolor(0,0,0);
    clear();
}

static void
doIt( void )
{
    playstate.advanceVideo = 1;
    playstate.advanceAudio = 1;
}	

/*
 * stopAllThreads(): Attempts to gracefully exit all the threads.
 */
void stopAllThreads(void) 
{
    if (video.timingActive == 1) {
	video.timingActive = 0;
	vlDestroyPath(video.svr, video.timingPath);
    }
    
    exit(0);
}
