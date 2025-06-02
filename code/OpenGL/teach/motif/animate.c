/*
 * animate.c - double buffered RGBA motif program with a work proc for animation
 */
/* compile: cc -o animate animate.c -lGLw -lGLU -lGL -lXm -lXt -lX11 */
#include <stdlib.h>
#include <stdio.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <X11/GLw/GLwMDrawA.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <GL/glu.h>

static void draw_scene(Widget w);
static void initialize(void);
static Boolean redraw_proc(XtPointer clientData);

static int attribs[] = { GLX_RGBA, GLX_RED_SIZE, 1, GLX_DOUBLEBUFFER, None};

static String fallbackResources[] = {
    "*sgiMode: True",
    "*useSchemes: all",
    "*glxwidget.width: 300", "*glxwidget.height: 300",
    "*frame.shadowType: SHADOW_IN",
    NULL};

static struct {			/* global UI variables - keep them together */
    XtAppContext appctx;
    Widget glxwidget;
    Boolean direct;
    XtWorkProcId animate_wpid;
} state;

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
    GLwDrawingAreaCallbackStruct *call_data;
    call_data = (GLwDrawingAreaCallbackStruct *) call;

    glViewport(0, 0, call_data->width, call_data->height);
}

static void
expose(Widget w, XtPointer client_data, XtPointer call) {
    GLwDrawingAreaCallbackStruct *call_data;
    call_data = (GLwDrawingAreaCallbackStruct *) call;

    draw_scene(w);
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
	    state.animate_wpid = XtAppAddWorkProc(state.appctx, redraw_proc, &state.glxwidget);
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
	XmPUSHBUTTON, XmSEPARATOR, XmPUSHBUTTON,
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
	     state.animate_wpid = XtAppAddWorkProc(state.appctx, redraw_proc, &state.glxwidget);
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
    Widget          toplevel, frame;

    toplevel = XtOpenApplication(&state.appctx, "animate", NULL, 0, &argc, argv,
		fallbackResources, applicationShellWidgetClass, NULL, 0);
    dpy = XtDisplay(toplevel);

    frame = XmCreateFrame(toplevel, "frame", NULL, 0);
    XtManageChild(frame);

    /* specify visual directly */
    if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs)))
	XtAppError(state.appctx, "no suitable RGB visual");

    state.glxwidget = XtVaCreateManagedWidget("glxwidget", glwMDrawingAreaWidgetClass,
	frame, GLwNvisualInfo, visinfo, NULL);
    XtAddCallback(state.glxwidget, GLwNexposeCallback, expose, NULL);
    XtAddCallback(state.glxwidget, GLwNresizeCallback, resize, NULL);
    XtAddCallback(state.glxwidget, GLwNinputCallback, input, NULL);

    create_popup(frame);

    XtAddEventHandler(toplevel, StructureNotifyMask, False, map_change, NULL);
    XtRealizeWidget(toplevel);

    glxcontext = glXCreateContext(dpy, visinfo, 0, GL_TRUE);
    state.direct = glXIsDirect(dpy, glxcontext);
    GLwDrawingAreaMakeCurrent(state.glxwidget, glxcontext);

    initialize();

    XtAppMainLoop(state.appctx);
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
draw_scene(Widget w) {
    static float rot = 0.;

    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(.1, .1, .8);
    glPushMatrix();
    if ((rot += 5.) > 360.) rot -= 360.;
    glRotatef(rot,0.,1.,0.);
    cube();
    glScalef(0.3,0.3,0.3);
    glColor3f(.8, .8, .1);
    cube();
    glPopMatrix();
    GLwDrawingAreaSwapBuffers(w);
    if (!state.direct) glFinish();	/* hack to improve net interactivity */
}

static Boolean
redraw_proc(XtPointer clientData) {
    Widget *w = (Widget *)clientData;
    draw_scene(*w);
    return False;
}
