/*
 * animate.c - double buffered RGBA motif program with a work proc for animation
 */
/* compile: cc -o animate animate.c -lGLw -lGLU -lGL -lXm -lXt -lX11 */
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
static void initialize(void);
static Boolean redraw_proc(XtPointer clientData);

static int db_attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None};
static int sb_attribs[] = { GLX_RGBA, GLX_RED_SIZE, 1, None};

static String fallbackResources[] = {
    "*sgiMode: True",
    "*useSchemes: all",
    "*single.width: 300", "*single.height: 300",
    "*double.width: 300", "*double.height: 300",
    "*frame.shadowType: SHADOW_IN",
    NULL};

static struct {			/* global UI variables - keep them together */
    XtAppContext appctx;
    Widget sw, dw;		/* single & double buffered widgets */
    GLXContext scx, dcx;	/* single & double buffered contexts */
    Boolean single;		/* single buffered */
    Boolean direct;
    Dimension width, height;	/* dimensions for viewport */
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
    state.width = call_data->width; state.height = call_data->height;
}

static void
expose(Widget w, XtPointer client_data, XtPointer call) {
    GLwDrawingAreaCallbackStruct *call_data;
    call_data = (GLwDrawingAreaCallbackStruct *) call;

    draw_scene(w, False);
}

static void
submenu(Widget w, XtPointer clientData, XtPointer callData) {
    int entry = (int) clientData;

    switch (entry) {
    case 0:
	XtManageChild(state.sw);
	XRaiseWindow(XtDisplay(state.sw), XtWindow(state.sw));
	GLwDrawingAreaMakeCurrent(state.sw, state.scx);
	glViewport(0, 0, state.width, state.height);
	state.single = True;
	XtUnmanageChild(state.dw);
	break;
    case 1:
	XtManageChild(state.dw);
	XRaiseWindow(XtDisplay(state.dw), XtWindow(state.dw));
	GLwDrawingAreaMakeCurrent(state.dw, state.dcx);
	glViewport(0, 0, state.width, state.height);
	state.single = False;
	XtUnmanageChild(state.sw);
	break;
    default:
	break;
    }
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
	    state.animate_wpid = XtAppAddWorkProc(state.appctx, redraw_proc, &state.dw);
	}
	break;
    case 2:
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
create_pulldown(Widget parent, int which) {
    Arg args[10];
    Widget pulldown;
    int n;
    XmButtonType button_types[] = {
	XmRADIOBUTTON, XmRADIOBUTTON,
    };
    XmString button_labels[XtNumber(button_types)];

    button_labels[0] = XmStringCreateLocalized("single");
    button_labels[1] = XmStringCreateLocalized("double");

    n = 0;
    XtSetArg(args[n], XmNbuttonCount, XtNumber(button_types)); n++;
    XtSetArg(args[n], XmNbuttonType, button_types); n++;
    XtSetArg(args[n], XmNbuttons, button_labels); n++;
    XtSetArg(args[n], XmNpostFromButton, 1); n++;
    XtSetArg(args[n], XmNradioBehavior, True); n++;
    XtSetArg(args[n], XmNsimpleCallback, submenu); n++;
    pulldown = XmCreateSimplePulldownMenu(parent, "pulldown", args, n);

    XtSetArg(args[0], XmNset, True); /* initially double buffered */
    XtSetValues(XtNameToWidget(pulldown, "button_1"), args, 1);

    XmStringFree(button_labels[0]);
    XmStringFree(button_labels[1]);
}

static void
create_popup(Widget parent) {
    Arg args[10];
    static Widget popup;
    int n;
    XmButtonType button_types[] = {
	XmCHECKBUTTON, XmCASCADEBUTTON, XmSEPARATOR, XmPUSHBUTTON,
    };
    XmString button_labels[XtNumber(button_types)];

    button_labels[0] = XmStringCreateLocalized("animate");
    button_labels[1] = XmStringCreateLocalized("visual");
    button_labels[2] = NULL;
    button_labels[3] = XmStringCreateLocalized("quit");

    n = 0;
    XtSetArg(args[n], XmNbuttonCount, XtNumber(button_types)); n++;
    XtSetArg(args[n], XmNbuttonType, button_types); n++;
    XtSetArg(args[n], XmNbuttons, button_labels); n++;
    XtSetArg(args[n], XmNsimpleCallback, menu); n++;
    popup = XmCreateSimplePopupMenu(parent, "foo", args, n);
    XtAddEventHandler(parent, ButtonPressMask, False, activate_menu, &popup);
    create_pulldown(popup, 0);

    XmStringFree(button_labels[0]);
    XmStringFree(button_labels[1]);
    XmStringFree(button_labels[3]);
}

static void
map_change(Widget w, XtPointer clientData, XEvent *event, Boolean *cont) {
    switch (event->type) {
    case MapNotify:
	/* resume animation if we become mapped and are in the animated state */
        if (state.animate_wpid != 0)
	     state.animate_wpid = XtAppAddWorkProc(state.appctx, redraw_proc, &state.dw);
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
    Arg		    args[30];
    int		    n;

    toplevel = XtOpenApplication(&state.appctx, "switch-visual", NULL, 0, &argc, argv,
		fallbackResources, applicationShellWidgetClass, NULL, 0);
    dpy = XtDisplay(toplevel);

    frame = XmCreateFrame(toplevel, "frame", NULL, 0);
    XtManageChild(frame);
    form = XmCreateForm(frame, "form", NULL, 0);
    XtManageChild(form);

    /* create single buffered widget & context */
    if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), sb_attribs)))
	XtAppError(state.appctx, "no suitable single buffered RGB visual");
    /* attach to form on all 4 sides */
    n = 0;
    XtSetArg(args[n], XtNx, 10); n++;
    XtSetArg(args[n], XtNy, 10); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], GLwNvisualInfo, visinfo); n++;
    state.sw = XtCreateWidget("single", glwMDrawingAreaWidgetClass,
	form, args, n);
    XtAddCallback(state.sw, GLwNexposeCallback, expose, NULL);
    XtAddCallback(state.sw, GLwNresizeCallback, resize, NULL);
    XtAddCallback(state.sw, GLwNinputCallback, input, NULL);
    state.scx = glXCreateContext(dpy, visinfo, 0, GL_TRUE);

    /* create double buffered widget */
    if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), db_attribs)))
	XtAppError(state.appctx, "no suitable double buffered RGB visual");
    XtSetArg(args[n-1], GLwNvisualInfo, visinfo);
    state.dw = XtCreateManagedWidget("double", glwMDrawingAreaWidgetClass,
	form, args, n);
    XtAddCallback(state.dw, GLwNexposeCallback, expose, NULL);
    XtAddCallback(state.dw, GLwNresizeCallback, resize, NULL);
    XtAddCallback(state.dw, GLwNinputCallback, input, NULL);
    state.dcx = glXCreateContext(dpy, visinfo, 0, GL_TRUE);

    create_popup(frame);

    XtAddEventHandler(toplevel, StructureNotifyMask, False, map_change, NULL);
    XtRealizeWidget(toplevel);

    /* double buffered window on top */
    XRaiseWindow(dpy, XtWindow(state.dw));

    state.direct = glXIsDirect(dpy, state.dcx);

    GLwDrawingAreaMakeCurrent(state.sw, state.scx);
    initialize();
    GLwDrawingAreaMakeCurrent(state.dw, state.dcx);
    initialize();

    /* extract window dimensions for viewport */
    XtVaGetValues(state.dw, XmNwidth, &state.width, XmNheight, &state.height,
		  NULL);

    XtAppMainLoop(state.appctx);
}

static void
initialize(void) {
    GLUquadricObj *cyl;

    glShadeModel(GL_FLAT);
    gluPerspective(40., 1.0, 0.01, 1000.0);
    glTranslatef(0.0, 0.0, -3.0);
    glClearColor(0.2,0.2,0.2,0.);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    cyl = gluNewQuadric();
    glNewList(1, GL_COMPILE);
    glPushMatrix();
    glTranslatef(0., 0., -.5);
    gluCylinder(cyl, 0.0, 0.4, 1.0, 25, 25);
    glPopMatrix();
    glEndList();
    gluDeleteQuadric(cyl);
}

static void
draw_scene(Widget w, Boolean advance) {
    static float rot = 90.;

    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(.1, .1, .8);
    glPushMatrix();
    if (advance && (rot += 5.) > 360.) rot -= 360.;
    glRotatef(rot,0.,1.,0.);
    glCallList(1);
    glPopMatrix();
    if (!state.single) GLwDrawingAreaSwapBuffers(w);
    if (!state.direct) glFinish();	/* hack to improve net interactivity */
}

static Boolean
redraw_proc(XtPointer clientData) {
    Widget *w = (Widget *)clientData;
    draw_scene(*w, True);
    return False;
}
