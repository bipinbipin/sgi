/*
 * multi-widget.c - double buffered RGBA using two drawing area widgets
 */
/* compile: cc -o multi-widget multi-widget.c -lGLw -lGLU -lGL -lXm -lXt -lX11 */
#include <stdlib.h>
#include <stdio.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/PushBG.h>
#include <X11/GLw/GLwMDrawA.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <GL/glu.h>

static int attribs[] = { GLX_RGBA, GLX_RED_SIZE, 1, GLX_DOUBLEBUFFER, None};

static String fallbackResources[] = {
    "*sgiMode: True",
    "*useSchemes: all",
    "*glxwidget.width: 200", "*glxwidget.height: 200",
    "*glxwidget2.width: 200", "*glxwidget2.height: 200",
    "*frame.shadowType: SHADOW_IN",
    NULL};

static struct state {			/* per widget UI variables - keep them together */
    Widget w;
    GLXContext cx;
    short instance;
    short mstate, omx, omy, mx, my;
    short width, height;	/* viewport */
    GLfloat trans[3];		/* current translation */
    GLfloat rot[2];		/* current rotation */
} state, state2;

static Boolean direct;

static void draw_scene(struct state *);
static void update_view(struct state *);
static void initialize(void);

static void
input(Widget w, XtPointer client_data, XtPointer call) {
    char buf[31];
    KeySym keysym;
    XEvent *event = ((GLwDrawingAreaCallbackStruct *) call)->event;
    struct state *state = (struct state *)client_data;

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
    case ButtonPress:
	if (event->xbutton.button == Button2) {
	    state->mstate |= 2;
	    state->mx = event->xbutton.x;
	    state->my = event->xbutton.y;
	} else if (event->xbutton.button == Button1) {
	    state->mstate |= 1;
	    state->mx = event->xbutton.x;
	    state->my = event->xbutton.y;
	}
	break;
    case ButtonRelease:
	if (event->xbutton.button == Button2)
	    state->mstate &= ~2;
	else if (event->xbutton.button == Button1)
	    state->mstate &= ~1;
	break;
    case MotionNotify:
	if (state->mstate) {
	    state->omx = state->mx;
	    state->omy = state->my;
	    state->mx = event->xbutton.x;
	    state->my = event->xbutton.y;
	    update_view(state);
	}
	break;
    }
}

static void
resize(Widget w, XtPointer client_data, XtPointer call) {
    struct state *state = (struct state *)client_data;
    GLwDrawingAreaCallbackStruct *call_data;
    call_data = (GLwDrawingAreaCallbackStruct *) call;

    /* need to rebind context & reset viewport */
    GLwDrawingAreaMakeCurrent(w, state->cx);
    state->width = call_data->width;
    state->height = call_data->height;
    glViewport(0, 0, call_data->width, call_data->height);
}

static void
expose(Widget w, XtPointer client_data, XtPointer call) {
    struct state *state = (struct state *)client_data;
    GLwDrawingAreaCallbackStruct *call_data;
    call_data = (GLwDrawingAreaCallbackStruct *) call;

    /* need to rebind context & reset viewport */
    GLwDrawingAreaMakeCurrent(w, state->cx);
    glViewport(0, 0, state->width, state->height);
    draw_scene(state);
}

static void
quit(Widget w, XtPointer client_data, XtPointer call) {
    exit(EXIT_SUCCESS);
}

main(int argc, char *argv[]) {
    Display        *dpy;
    XVisualInfo    *visinfo;
    Widget          toplevel, frame, form, button;
    XtAppContext    appctx;
    Arg		    args[20];
    int		    n;

    toplevel = XtOpenApplication(&appctx, "multi-widget", NULL, 0, &argc, argv,
		fallbackResources, applicationShellWidgetClass, NULL, 0);
    dpy = XtDisplay(toplevel);

    form = XmCreateForm(toplevel, "form", NULL, 0);
    XtManageChild(form);

    /* attach first drawing area at top of form so it grows in X, but
     * doesn't grow in Y when the form resizes */
    n = 0;
    XtSetArg(args[n], XtNx, 10); n++;
    XtSetArg(args[n], XtNy, 10); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, 10); n++;
    XtSetArg(args[n], XmNtopOffset, 10); n++;
    XtSetArg(args[n], XmNrightOffset, 10); n++;
    frame = XmCreateFrame(form, "frame", args, n);
    XtManageChild(frame);

    /* specify visual directly */
    if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs)))
	XtAppError(appctx, "no suitable RGB visual");

    state.w = XtVaCreateManagedWidget("glxwidget", glwMDrawingAreaWidgetClass,
	frame, GLwNvisualInfo, visinfo, NULL);
    XtAddCallback(state.w, GLwNexposeCallback, expose, &state);
    XtAddCallback(state.w, GLwNresizeCallback, resize, &state);
    XtAddCallback(state.w, GLwNinputCallback, input, &state);

    /* attach second widget at bottom so it grows in both X & Y with
     * the rest of the form */
    n = 0;
    XtSetArg(args[n], XtNx, 10); n++;
    XtSetArg(args[n], XtNy, 10); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(args[n], XmNtopWidget, frame); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, 10); n++;
    XtSetArg(args[n], XmNtopOffset, 10); n++;
    XtSetArg(args[n], XmNbottomOffset, 50); n++;
    XtSetArg(args[n], XmNrightOffset, 10); n++;
    frame = XmCreateFrame(form, "frame", args, n);
    XtManageChild(frame);

    state2.w = XtVaCreateManagedWidget("glxwidget2", glwMDrawingAreaWidgetClass,
	    frame, GLwNvisualInfo, visinfo, NULL);
    XtAddCallback(state2.w, GLwNexposeCallback, expose, &state2);
    XtAddCallback(state2.w, GLwNresizeCallback, resize, &state2);
    XtAddCallback(state2.w, GLwNinputCallback, input, &state2);

    /* attach Quit button at bottom left corner */
    n = 0;
    XtSetArg(args[n], XtNx, 10); n++;
    XtSetArg(args[n], XtNy, 10); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftOffset, 10); n++;
    XtSetArg(args[n], XmNbottomOffset, 10); n++;
    XtSetArg(args[n], XmNtraversalOn, False); n++; /* no keyboard traversal */
    button = XmCreatePushButtonGadget(form, "Quit", args, n);
    XtAddCallback(button, XmNactivateCallback, quit, NULL);
    XtManageChild(button);

    XtRealizeWidget(toplevel);

    /* retrieve initial viewports */
    XtVaGetValues(state.w, XmNheight, &state.height,
				   XmNwidth, &state.width, NULL);
    XtVaGetValues(state2.w, XmNheight, &state2.height,
				    XmNwidth, &state2.width, NULL);

    state.cx = state2.cx = glXCreateContext(dpy, visinfo, 0, GL_TRUE);
    state.instance = 0; state2.instance = 1;
    direct = glXIsDirect(dpy, state.cx);

    GLwDrawingAreaMakeCurrent(state.w, state.cx);
    initialize();

    XtAppMainLoop(appctx);
}

static void
initialize(void) {	/* initialize context */
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
draw_scene(struct state *state) {

    glClear(GL_COLOR_BUFFER_BIT);
    glPushMatrix();
    glTranslatef(state->trans[0], state->trans[1], state->trans[2]);
    glRotatef(state->rot[0], 1.0, 0.0, 0.0);
    glRotatef(state->rot[1], 0.0, 1.0, 0.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (state->instance == 0)
	glColor3f(.1, .1, .8);
    else
	glColor3f(.1, .8, .1);
    cube();
    glScalef(0.3,0.3,0.3);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if (state->instance == 0)
	glColor3f(.8, .8, .1);
    else
	glColor3f(.8, .1, .1);
    cube();
    glPopMatrix();
    GLwDrawingAreaSwapBuffers(state->w);
    if (direct) glFinish();	/* hack to improve net interactivity */
}

static void
update_view(struct state *state) {
    int dx = state->omx - state->mx;
    int dy = state->my - state->omy;

    GLwDrawingAreaMakeCurrent(state->w, state->cx);
    glViewport(0, 0, state->width, state->height);
    switch(state->mstate) {
    case 1:	/* pan */
	state->trans[0] -= dx/100.;
	state->trans[1] -= dy/100.;
	break;
    case 2:	/* rotate */
	state->rot[0] += (dy * 180.0)/500.;
	state->rot[1] -= (dx * 180.0)/500.;
#define clamp(x) x = x > 360. ? x-360. : x < -360. ? x += 360. : x 
	clamp(state->rot[0]);
	clamp(state->rot[1]);
	break;
    case 3:	/* zoom */
	state->trans[2] -= (dx+dy)/100.;
	break;
    }
    draw_scene(state);
}
