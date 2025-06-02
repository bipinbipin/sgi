/*
 * This file is for Impact Compression boards. Like dmplay.cosmo, 
 * if the display option is not graphics, a VL path (memory to video out)
 * will be created. The screen node will be added to the path as another
 * drain node if the same system has Impact Video board, so the decoded
 * images will be displayed on both video output and the gfx screen.
 * On the screen, a X window will be open. The routines in this file are 
 * for creating the X window, handling X events and updating VL_ORIGIN 
 * control based on the position of the X window specified in the events.  
 */

#include "dmplay.h"
#include <ulocks.h>

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

/* 
 * Initialize X toplevel widget
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
 * This routine creates a window and then
 * starts a new thread to handle interaction with X.
 */
void
x_open(int width, int height)
{
    Arg args[20];
    int n;
    Display *dpy;

    Xinit(NULL, NULL);

    dpy = XtDisplay(toplevel);

    /*
     * We want to get a chance to clean up if the user picks
     * quit/close from the window manager menu.  By setting
     * these flags, we'll get sent a message if that happens.
     */
    wmproto = XInternAtom(dpy, "WM_PROTOCOLS", False);
    delmesg = XInternAtom(dpy, "WM_DELETE_WINDOW", False);

    winHeight = height;
    winWidth  = width;

    XtAddEventHandler(toplevel, ExposureMask, FALSE, handleExpose, 0);
    XtAddEventHandler(toplevel, SubstructureNotifyMask, FALSE,handleMove,0);
    XtAddEventHandler(toplevel, KeyPressMask, TRUE, handleUser, 0);

    n = 0;
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
    video.windowId = (int ) XtWindow(w);

    /*
     * Once the window is mapped, vl can begin transfering and we can start the
     * video application, i.e. streamDecompress()
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

    dpy = XtDisplay(w);
    XTranslateCoordinates(dpy, XtWindow(w),
                          RootWindow(dpy, DefaultScreen(dpy)),
                          0, 0,
                          &val.xyVal.x, &val.xyVal.y,
                          &dummyWin);

    vlSetControl(video.svr, video.path, video.scrdrn, VL_ORIGIN, &val);
}
