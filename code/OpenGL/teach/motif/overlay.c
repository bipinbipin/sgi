/*
 * overlay.c - simple double buffered RGBA motif program which rotates an object.
 *	Overlay and main windows created as a children of a form widget
 *	and managed independently.  The overlay window/context
 *	creation uses the visual_info extension to find a visual with
 *	a transparent pixel.
 */
/* compile: cc -o overlay overlay.c -lGLw -lGLU -lGL -lXm -lXt -lX11 */
#include <stdlib.h>
#include <stdio.h>
#include <Xm/Frame.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <X11/GLw/GLwMDrawA.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <GL/glu.h>

static void draw_scene(Widget w, Boolean advance);
static void ov_draw_scene(void);
static void initialize(void);
static Boolean redraw_proc(XtPointer clientData);

static int attribs[] = { GLX_RGBA, GLX_RED_SIZE, 1, GLX_DOUBLEBUFFER, None};
static int ov_attribs[] = { GLX_BUFFER_SIZE, 2,
		     	    GLX_LEVEL, 1,
		    	    GLX_TRANSPARENT_TYPE_EXT, GLX_TRANSPARENT_INDEX_EXT,
			    None };

static String fallbackResources[] = {
    "*sgiMode: True",
    "*useSchemes: all",
    "*glxwidget.width: 300", "*glxwidget.height: 300",
    "*overlay.width: 300", "*overlay.height: 300",
    "*frame.shadowType: SHADOW_IN",
    NULL};

static struct state {		/* global UI variables - keep them together */
    Widget w;
    GLXContext cx;
    XtWorkProcId animate_wpid;
} state, ov_state;

static XtAppContext appctx;
static Boolean direct;

static void
input(Widget w, XtPointer client_data, XtPointer call) {
   char buf[31];
   KeySym keysym;
   XEvent *event = ((GLwDrawingAreaCallbackStruct *) call)->event;

   switch(event->type) {
   case KeyRelease:
      XLookupString(&event->xkey, buf, sizeof buf, &keysym, NULL);
      switch(keysym) {
      case XK_Escape:
	 exit(EXIT_SUCCESS);
	 break;
      default: break;
      }
      break;
   }
}

static void
resize(Widget w, XtPointer client_data, XtPointer call) {
    /*struct state *state = (struct state *)client_data;*/
    GLwDrawingAreaCallbackStruct *call_data;
    call_data = (GLwDrawingAreaCallbackStruct *) call;

    GLwDrawingAreaMakeCurrent(ov_state.w, ov_state.cx);
    glViewport(0, 0, call_data->width, call_data->height);
    GLwDrawingAreaMakeCurrent(state.w, state.cx);
    glViewport(0, 0, call_data->width, call_data->height);
}

static void
expose(Widget w, XtPointer client_data, XtPointer call) {
    GLwDrawingAreaCallbackStruct *call_data;
    call_data = (GLwDrawingAreaCallbackStruct *) call;

    draw_scene(w, False);
}

static void
ov_expose(Widget w, XtPointer client_data, XtPointer call) {
    GLwDrawingAreaCallbackStruct *call_data;
    call_data = (GLwDrawingAreaCallbackStruct *) call;

    GLwDrawingAreaMakeCurrent(w, ov_state.cx);
    ov_draw_scene();
    GLwDrawingAreaMakeCurrent(state.w, state.cx);
}

static void
menu(Widget w, XtPointer clientData, XtPointer callData) {
    int entry = (int) clientData;

    switch (entry) {
    case 0:
	if (state.animate_wpid) {
	    XtRemoveWorkProc(state.animate_wpid);
	    state.animate_wpid = 0;
	} else {
	    /* register work proc */
	    state.animate_wpid = XtAppAddWorkProc(appctx, redraw_proc, &state.w);
	}
	break;
    case 1:
	exit(EXIT_SUCCESS);
	break;
    default:
	break;
    }
}

static void
activate_menu(Widget w, XtPointer clientData, XEvent *event, Boolean *cont) {
    Widget popup = *((Widget *) clientData);

    if (event->type == ButtonPress && event->xbutton.button == Button3) {
	XmMenuPosition(popup, &event->xbutton);
	XtManageChild(popup);
    }
}

static void
create_popup(Widget parent) {
    Arg args[10];
    static Widget popup;
    int n;
    XmButtonType button_types[] = {
	XmTOGGLEBUTTON, XmSEPARATOR, XmPUSHBUTTON,
    };
    XmString button_labels[XtNumber(button_types)];

    button_labels[0] = XmStringCreateLocalized("animate");
    button_labels[1] = NULL;
    button_labels[2] = XmStringCreateLocalized("quit");

    n = 0;
    XtSetArg(args[n], XmNbuttonCount, XtNumber(button_types)); n++;
    XtSetArg(args[n], XmNbuttonType, button_types); n++;
    XtSetArg(args[n], XmNbuttons, button_labels); n++;
    XtSetArg(args[n], XmNsimpleCallback, menu); n++;
    popup = XmCreateSimplePopupMenu(parent, "popup", args, n);
    XtAddEventHandler(parent, ButtonPressMask, False, activate_menu, &popup);

    XmStringFree(button_labels[0]);
    XmStringFree(button_labels[2]);
}

static void
map_change(Widget w, XtPointer clientData, XEvent *event, Boolean *cont) {
    switch (event->type) {
    case MapNotify:
	/* resume animation if we become mapped and are in the animated state */
        if (state.animate_wpid != 0)
	     state.animate_wpid = XtAppAddWorkProc(appctx, redraw_proc, &state.w);
        break;
    case UnmapNotify:
	/* don't animate if we aren't mapped */
        if (state.animate_wpid) XtRemoveWorkProc(state.animate_wpid);
        break;
    }
}

main(int argc, char *argv[]) {
    Display        *dpy;
    XVisualInfo    *visinfo;
    GLXContext      glxcontext;
    Widget          toplevel, frame, form;
    Arg		    args[20];
    int		    n;
    unsigned long   pixel;
    XColor	    col;
    Colormap	    cmap;

    toplevel = XtOpenApplication(&appctx, "overlay", NULL, 0, &argc, argv,
		fallbackResources, applicationShellWidgetClass, NULL, 0);
    dpy = XtDisplay(toplevel);

    frame = XmCreateFrame(toplevel, "frame", NULL, 0);
    XtManageChild(frame);

    form = XmCreateForm(frame, "form", NULL, 0);
    XtManageChild(form);

    /* specify visual directly */
    if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs)))
	XtAppError(appctx, "no suitable RGB visual");

    /* attach to form on all 4 sides */
    n = 0;
    XtSetArg(args[n], XtNx, 0); n++;
    XtSetArg(args[n], XtNy, 0); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], GLwNvisualInfo, visinfo); n++;
    state.w = XtCreateManagedWidget("glxwidget", glwMDrawingAreaWidgetClass,
	form, args, n);
    XtAddCallback(state.w, GLwNexposeCallback, expose, NULL);
    XtAddCallback(state.w, GLwNresizeCallback, resize, &state);
    XtAddCallback(state.w, GLwNinputCallback, input, NULL);
    state.cx = glXCreateContext(dpy, visinfo, 0, GL_TRUE);
    direct = glXIsDirect(dpy, state.cx);

    /* create overlay widget */
    if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), ov_attribs)))
	XtAppError(appctx, "no suitable overlay visual");
    XtSetArg(args[n-1], GLwNvisualInfo, visinfo);
    ov_state.w = XtCreateManagedWidget("overlay", glwMDrawingAreaWidgetClass,
	form, args, n);
    XtAddCallback(ov_state.w, GLwNexposeCallback, ov_expose, NULL);
    XtAddCallback(ov_state.w, GLwNresizeCallback, resize, &ov_state);
    XtAddCallback(ov_state.w, GLwNinputCallback, input, NULL);
    ov_state.cx = glXCreateContext(dpy, visinfo, 0, GL_TRUE);

    create_popup(frame);

    XtAddEventHandler(toplevel, StructureNotifyMask, False, map_change, NULL);
    XtRealizeWidget(toplevel);
    XRaiseWindow(dpy, XtWindow(ov_state.w));

    GLwDrawingAreaMakeCurrent(state.w, state.cx);

    initialize();

    /* allocate a color cell from the overlay planes */
    XtVaGetValues(ov_state.w, XmNcolormap, &cmap, NULL);
    if (!XAllocColorCells(dpy, cmap, True, NULL, 0, &pixel, 1))
	XtAppError(appctx, "can't alloc overlay color cell");
    col.pixel = pixel;
    col.red = 65535;
    col.green = 65535;
    col.blue = 65535;
    col.flags = DoRed | DoGreen | DoBlue;
    XStoreColor(dpy, cmap, &col);

    XtAppMainLoop(appctx);
}

static void
initialize(void) {
    glShadeModel(GL_FLAT);
    gluPerspective(40., 3.0/2.0, 0.001, 100000.0);
    glTranslatef(0.0, 0.0, -3.0);
    glClearColor(0.2,0.2,0.2,0.);
}

static void
side(void) { /* make a square translated 0.5 in the z direction */
    glPushMatrix();
    glTranslatef(0.0,0.0,0.5);
    glRectf(-0.5,-0.5,0.5,0.5);
    glPopMatrix();
}

static void
cube(void) { /* make a cube out of 4 squares */
    glPushMatrix();
    side();
    glRotatef(90.,1.,0.,0.);
    side();
    glRotatef(90.,1.,0.,0.);
    side();
    glRotatef(90.,1.,0.,0.);
    side();
    glPopMatrix();
}

static void
draw_scene(Widget w, Boolean advance) {
    static float rot = 0.;

    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(.1, .1, .8);
    glPushMatrix();
    if (advance && (rot += 5.) > 360.) rot -= 360.;
    glRotatef(rot,0.,1.,0.);
    cube();
    glScalef(0.3,0.3,0.3);
    glColor3f(.8, .8, .1);
    cube();
    glPopMatrix();
    GLwDrawingAreaSwapBuffers(w);
    if (!direct) glFinish();	/* hack to improve net interactivity */
}

/*
 * the amount of work being done doesn't justify the need for
 * a separate overlay initialization and redraw routines so we
 * do it all together. XXX doesn't allocate and initialize any
 * color cells.
 */
static void
ov_draw_scene(void) {
    int i;
    /* simply draw a grid of 10 equally spaced lines */
    glLoadIdentity();
    glOrtho(0., 11., 0., 11., -1., 1.);
    glClearIndex(0.); /* transparent value is always zero */
    glClear(GL_COLOR_BUFFER_BIT);
    glIndexi(1);
    glBegin(GL_LINES);
    for(i = 1; i < 10; i++) {
	glVertex2f(0.5+i, 1.0);
	glVertex2f(0.5+i, 10.0);
	glVertex2f(1.0,  0.5+i);
	glVertex2f(10.0, 0.5+i);
    }
    glEnd();
}

static Boolean
redraw_proc(XtPointer clientData) {
    Widget *w = (Widget *)clientData;
    draw_scene(*w, True);
    return False;
}
