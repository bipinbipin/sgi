/*
** compdecomp
** Indigo2 IMPACT Compression example program
**
** Silicon Graphics, Inc, 1996
*/

#include <stdlib.h>
#include "compdecomp.h"

static void eventThread( void *arg );
static void handleUser(Widget, XtPointer, XEvent *, Boolean *);

static Atom wmproto, delmesg;

typedef struct {
    Pixel   bg;
    Boolean sgimode;
} AppData, * AppDataPtr;

void
slider_drop( 
	Widget w,
	XtPointer client_data,
	XtPointer c_data 
    )
{
    int n;
    Arg wargs[10];
    char comp[100];
    Widget label = (Widget)client_data;
    XmScaleCallbackStruct * call_data = (XmScaleCallbackStruct *) c_data;

    int bitrate;
    static int last_set = 0;

    sprintf(comp, "%3d:1", call_data->value);

    n = 0;
    XtSetArg(wargs[n], XmNlabelString, 
	XmStringCreate(comp, XmSTRING_DEFAULT_CHARSET)), n++;

    XtSetValues( label, wargs, n );

    if (last_set != call_data->value) {
	last_set = call_data->value;

	/*
	** requested bitrate is uncompressed size (HxW) * 2 (yuv422 = 2
	** bytes per pixel) * 8 (8 bits per pixel) * 60 (fields per sec,
	** NTSC) divided by the 
	** compression ratio, which is call_data->value
	*/
	bitrate = (global.image.height * global.image.width * 2 * 8 * 60) /
		    call_data->value;

	printf("bitrate set to %d bits/second (%d bytes per field)\n",bitrate,bitrate/8/60);
	clSetParam( global.comp.handle, CL_BITRATE, bitrate );
    }
}

void
slider_drag( 
	Widget w,
	XtPointer client_data,
	XtPointer c_data 
    )
{
    int n;
    Arg wargs[10];
    char comp[100];
    Widget label = (Widget)client_data;
    XmScaleCallbackStruct * call_data = (XmScaleCallbackStruct *) c_data;

    sprintf(comp, "%3d:1", call_data->value);

    n = 0;
    XtSetArg(wargs[n], XmNlabelString, 
	XmStringCreate(comp, XmSTRING_DEFAULT_CHARSET)), n++;

    XtSetValues( label, wargs, n );
}

static String resources[] =  {
    "*background: grey70",
    "*sgimode: TRUE",
    NULL
};

gui_init(int * argc, char * argv[])
{
    int	    n;
    Arg	    wargs[10];
    Colormap cmap;
    XColor  color, ignore;
    AppData appdata;
    char    ratiostr[100];

    n=0;
    XtSetArg(wargs[n], XmNmwmDecorations,
	MWM_DECOR_ALL | MWM_DECOR_RESIZEH | MWM_DECOR_MAXIMIZE); n++;
    XtSetArg(wargs[n], XmNmwmFunctions,
	MWM_FUNC_ALL | MWM_FUNC_RESIZE | MWM_FUNC_MAXIMIZE); n++;

    global.gui.toplevel = XtAppInitialize(&global.gui.appContext, "compdecomp", 
						NULL, 0,
						argc, argv,
						resources, wargs, n);

    global.gui.dpy = XtDisplay( global.gui.toplevel );

    XtSetArg(wargs[n], XmNwidth, 370), n++;
    XtSetArg(wargs[n], XmNheight, 45), n++;

    global.gui.base = XtCreateManagedWidget( "base", xmFormWidgetClass, 
						global.gui.toplevel, wargs, n );

    n = 0;
    XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_FORM), n++;
    XtSetArg(wargs[n], XmNrightAttachment, XmATTACH_FORM), n++;
    XtSetArg(wargs[n], XmNleftAttachment, XmATTACH_FORM), n++;
    XtSetArg(wargs[n], XmNlabelString, 
	XmStringCreate("compression ratio", XmSTRING_DEFAULT_CHARSET)), n++;
    
    global.gui.label = XtCreateManagedWidget( "label", xmLabelWidgetClass,
						global.gui.base, wargs, n );

    n = 0;
    XtSetArg(wargs[n], XmNwidth, 300), n++;
    XtSetArg(wargs[n], XmNorientation, XmHORIZONTAL), n++;
    XtSetArg(wargs[n], XmNprocessingDirection, XmMAX_ON_LEFT), n++;
    XtSetArg(wargs[n], XmNshowValue, FALSE), n++;
    XtSetArg(wargs[n], XmNminimum, 2), n++;
    XtSetArg(wargs[n], XmNmaximum, 75), n++;
    XtSetArg(wargs[n], XmNvalue, INIT_COMP_RATIO), n++;
    XtSetArg(wargs[n], XmNtopWidget, global.gui.label), n++;
    XtSetArg(wargs[n], XmNbottomAttachment, XmATTACH_FORM), n++;
    XtSetArg(wargs[n], XmNrightAttachment, XmATTACH_FORM), n++;

    global.gui.slider = XtCreateManagedWidget( "slider", xmScaleWidgetClass,
						global.gui.base, wargs, n );

    n = 0;
    XtSetArg(wargs[n], XmNrightWidget, global.gui.slider), n++;
    XtSetArg(wargs[n], XmNtopWidget, global.gui.ratio), n++;
    XtSetArg(wargs[n], XmNleftAttachment, XmATTACH_FORM), n++;
    XtSetArg(wargs[n], XmNbottomAttachment, XmATTACH_FORM), n++;
    sprintf(ratiostr, "%3d:1",INIT_COMP_RATIO);
    XtSetArg(wargs[n], XmNlabelString, 
	XmStringCreate(ratiostr, XmSTRING_DEFAULT_CHARSET)), n++;

    global.gui.ratio = XtCreateManagedWidget( "ratio", xmLabelWidgetClass,
						global.gui.base, wargs, n );

    n = 0;
    XtSetArg(wargs[n], XmNleftWidget, global.gui.ratio), n++;
    XtSetValues( global.gui.slider, wargs, n );

    XtAddCallback( global.gui.slider, XmNvalueChangedCallback,
					 slider_drop, global.gui.ratio );
    XtAddCallback( global.gui.slider, XmNdragCallback,
					 slider_drag, global.gui.ratio );

    /* handle close/esc exits */
    XtAddEventHandler( global.gui.toplevel, KeyPressMask, TRUE, handleUser, 0);

    /* add requests for window manager protocol stuff */
    wmproto = XInternAtom(global.gui.dpy, "WM_PROTOCOLS", False);
    delmesg = XInternAtom(global.gui.dpy, "WM_DELETE_WINDOW", False);

    XtRealizeWidget( global.gui.toplevel );

    /*
    ** and spawn a child process to handle the X events/ui for us
    */
    if (sproc(eventThread, PR_SADDR | PR_BLOCK) == -1) {
	fprintf(stderr,"Unable to sproc to handle X events\n");
	perror("sproc");
	exit(1);
    }

    return 0;
}

static void
eventThread(void *arg)
{
    char * xrandom;
    sigset(SIGHUP, SIG_DFL);
    prctl(PR_TERMCHILD);

    unblockproc(getppid());

    XtAppMainLoop( global.gui.appContext );
}

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
			exit(0);
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
				ev->xclient.data.l[0] == delmesg) {
		if ((ppid = getppid()) == -1) {
		    perror("Unable to find video thread");
		} else
		    if (ppid != 1)
			kill ((pid_t)ppid, SIGTERM);
		exit(0);
	    }
	    break;
    }
}
