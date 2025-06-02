/*
** A simple OpenGL stereo application.  Uses the GLw Motif DrawingArea widget
** with a popup menu.  Includes a simple API for controlling stereo.
**
** cc -o glwstereo glwstereo.c -lGLw -lXm -lXt -lGL -lXext -lX11 -lm
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/StringDefs.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/PushBG.h>
#include <Xm/SeparatoG.h>
#include <X11/GLw/GLwMDrawA.h>

/************************************************************************/

#include <X11/extensions/SGIStereo.h>

static struct stereoStateRec {
    Bool	useSGIStereo;
    Display	*currentDisplay;
    Window	currentWindow;
    GLXContext	currentContext;
    GLenum	currentDrawBuffer;
    int		currentStereoBuffer;
    Bool	enabled;
    char	*stereoCommand;
    char	*restoreCommand;
} stereo;

/* call instead of glDrawBuffer */
void
stereoDrawBuffer(GLenum mode)
{
    if (stereo.useSGIStereo) {
	stereo.currentDrawBuffer = mode;
	switch (mode) {
	  case GL_FRONT:
	  case GL_BACK:
	  case GL_FRONT_AND_BACK:
	    /*
	    ** Simultaneous drawing to both left and right buffers isn't
	    ** really possible if we don't have a stereo capable visual.
	    ** For now just fall through and use the left buffer.
	    */
	  case GL_LEFT:
	  case GL_FRONT_LEFT:
	  case GL_BACK_LEFT:
	    stereo.currentStereoBuffer = STEREO_BUFFER_LEFT;
	    break;
	  case GL_RIGHT:
	  case GL_FRONT_RIGHT:
	    stereo.currentStereoBuffer = STEREO_BUFFER_RIGHT;
	    mode = GL_FRONT;
	    break;
	  case GL_BACK_RIGHT:
	    stereo.currentStereoBuffer = STEREO_BUFFER_RIGHT;
	    mode = GL_BACK;
	    break;
	  default:
	    break;
	}
        if (stereo.currentDisplay && stereo.currentWindow) {
	    glXWaitGL();  /* sync with GL command stream before calling X */
	    XSGISetStereoBuffer(stereo.currentDisplay,
				stereo.currentWindow,
				stereo.currentStereoBuffer);
	    glXWaitX();   /* sync with X command stream before calling GL */
	}
    }
    glDrawBuffer(mode);
}

/* call instead of glClear */
void
stereoClear(GLbitfield mask)
{
    if (stereo.useSGIStereo) {
	GLenum drawBuffer = stereo.currentDrawBuffer;
	switch (drawBuffer) {
	  case GL_FRONT:
	    stereoDrawBuffer(GL_FRONT_RIGHT);
	    glClear(mask);
	    stereoDrawBuffer(drawBuffer);
	    break;
	  case GL_BACK:
	    stereoDrawBuffer(GL_BACK_RIGHT);
	    glClear(mask);
	    stereoDrawBuffer(drawBuffer);
	    break;
	  case GL_FRONT_AND_BACK:
	    stereoDrawBuffer(GL_RIGHT);
	    glClear(mask);
	    stereoDrawBuffer(drawBuffer);
	    break;
	  case GL_LEFT:
	  case GL_FRONT_LEFT:
	  case GL_BACK_LEFT:
	  case GL_RIGHT:
	  case GL_FRONT_RIGHT:
	  case GL_BACK_RIGHT:
	  default:
	    break;
	}
    }
    glClear(mask);
}

/* call after glXMakeCurrent */
void
stereoMakeCurrent(Display *dpy, Window win, GLXContext ctx)
{
    if (stereo.useSGIStereo) {
	if (dpy && (dpy != stereo.currentDisplay)) {
	    int event, error;
	    /* Make sure new Display supports SGIStereo */
	    if (XSGIStereoQueryExtension(dpy, &event, &error) == False) {
		dpy = NULL;
	    }
	}
	if (dpy && win && (win != stereo.currentWindow)) {
	    /* Make sure new Window supports SGIStereo */
	    if (XSGIQueryStereoMode(dpy, win) == X_STEREO_UNSUPPORTED) {
		win = None;
	    }
	}
	if (ctx && (ctx != stereo.currentContext)) {
	    GLint drawBuffer;
	    glGetIntegerv(GL_DRAW_BUFFER, &drawBuffer);
	    stereoDrawBuffer((GLenum) drawBuffer);
	}
	stereo.currentDisplay = dpy;
	stereo.currentWindow = win;
	stereo.currentContext = ctx;
    }
}

GLboolean runSetmon = GL_TRUE;

/* call to turn on stereo viewing */
void
stereoEnable(void)
{
    if (!stereo.enabled && stereo.stereoCommand && runSetmon) {
	system(stereo.stereoCommand);
    }
    stereo.enabled = True;
}

/* call to turn off stereo viewing */
void
stereoDisable(void)
{
    if (stereo.enabled && stereo.restoreCommand && runSetmon) {
	system(stereo.restoreCommand);
    }
    stereo.enabled = False;
}

/* call before using stereo */
void
stereoInit(GLboolean usingStereoVisual, char *stereoCmd, char *restoreCmd)
{
    stereo.useSGIStereo = !usingStereoVisual;
    stereo.currentDisplay = NULL;
    stereo.currentWindow = None;
    stereo.currentContext = NULL;
    stereo.currentDrawBuffer = GL_NONE;
    stereo.currentStereoBuffer = STEREO_BUFFER_NONE;
    stereo.enabled = False;
    if (stereo.stereoCommand) {
	free(stereo.stereoCommand);
    }
    stereo.stereoCommand = stereoCmd ? strdup(stereoCmd) : NULL;
    if (stereo.restoreCommand) {
	free(stereo.restoreCommand);
    }
    stereo.restoreCommand = restoreCmd ? strdup(restoreCmd) : NULL;
}

/* call when done using stereo */
void
stereoDone(void)
{
    stereoDisable();
    stereoInit(True, NULL, NULL);
}

/************************************************************************/

#define APP_CLASS "GLWApp"

String fallbackResources[] = {
    /*
    ** Widget resources
    */
    "*sgiMode: True",
    "*useSchemes: all",
    "*title: Simple OpenGL Stereo Demo",
    "*form.width: 300",
    "*form.height: 300",
    /*
    ** Non-Widget (application) resources
    */
    "*stereoCommand: /usr/gfx/setmon -n 640x512_120s",
    "*restoreCommand: /usr/gfx/setmon -n 72HZ",
    "*SGIStereoCommand: /usr/gfx/setmon -n STR_BOT",
    "*SGIRestoreCommand: /usr/gfx/setmon -n 60HZ",
    NULL,
};

struct appResourceRec {
    _XtString stereoCommand;
    _XtString restoreCommand;
    _XtString SGIStereoCommand;
    _XtString SGIRestoreCommand;
} appResources;

XtResource appResourceList[] = {
    { "stereoCommand",
      XtCString, XtRString, sizeof (_XtString),
      XtOffsetOf(struct appResourceRec, stereoCommand),
      XtRImmediate, (XtPointer) NULL },
    { "restoreCommand",
      XtCString, XtRString, sizeof (_XtString),
      XtOffsetOf(struct appResourceRec, restoreCommand),
      XtRImmediate, (XtPointer) NULL },
    { "SGIStereoCommand",
      XtCString, XtRString, sizeof (_XtString),
      XtOffsetOf(struct appResourceRec, SGIStereoCommand),
      XtRImmediate, (XtPointer) NULL },
    { "SGIRestoreCommand",
      XtCString, XtRString, sizeof (_XtString),
      XtOffsetOf(struct appResourceRec, SGIRestoreCommand),
      XtRImmediate, (XtPointer) NULL },
};

XtAppContext appContext;
XtWorkProcId appWorkProcId;
Widget topLevel, form, glw, popup;
GLXContext ctx;
GLboolean animationEnabled = GL_FALSE;
GLboolean stereoEnabled = GL_FALSE;
double lastTime = 0.0;
double rotationRate = 360.0 / 8.0;
double rotation = 0.0;

static void
drawCylinder(void)
{
    int numSides = 20;
    double radius = 0.7;
    double stepSize = 2.0*M_PI / numSides;
    int i;

    glBegin(GL_TRIANGLE_STRIP);
    for (i=0; i<=numSides; ++i) {
	double a = i * stepSize;
	GLfloat x = cos(a);
	GLfloat y = sin(a);

	glNormal3f(x, y, 0.0);
	glVertex3f(x * radius, y * radius, 0.7);
	glNormal3f(x, y, 0.0);
	glVertex3f(x * radius, y * radius, -0.7);
    }
    glEnd();
}

static void
initialize(void)
{
    GLfloat lightAmb[4] = { 0.2, 0.2, 0.2, 1.0 };
    GLfloat lightPos[4] = { 1.5, 1.5, 1.5, 1.0 };
    GLfloat matAmbDiff[4] = { 0.30, 0.40, 0.80, 1.0 };

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -2);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmb);
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, matAmbDiff);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void
stereoFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
	      GLfloat near, GLfloat far, GLfloat eyeDist, GLfloat eyeOffset)
{
    GLfloat eyeShift = (eyeDist - near) * (eyeOffset / eyeDist);

    glFrustum(left+eyeShift, right+eyeShift, bottom, top, near, far);
    glTranslatef(-eyeShift, 0.0, 0.0);
}

static void
redraw(void)
{
    GLfloat eyeDist = 2.0;
    GLfloat eyeOffset = 0.025;

    glPushMatrix();
    glRotated(rotation, 0, 1, 0);

    if (stereoEnabled) {
	/*
	** Draw right-eye view.
	*/
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	stereoFrustum(-1, 1, -1, 1, 1, 3, eyeDist, eyeOffset);
	glMatrixMode(GL_MODELVIEW);

	stereoDrawBuffer(GL_BACK_RIGHT);
	stereoClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	drawCylinder();
    } else {
	eyeOffset = 0.0;
    }

    /*
    ** Draw left-eye view.
    */
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    stereoFrustum(-1, 1, -1, 1, 1, 3, eyeDist, -eyeOffset);
    glMatrixMode(GL_MODELVIEW);

    if (stereoEnabled || stereo.useSGIStereo)
	stereoDrawBuffer(GL_BACK_LEFT);
    else
	stereoDrawBuffer(GL_BACK);
    stereoClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawCylinder();

    glPopMatrix();
    GLwDrawingAreaSwapBuffers(glw);
}

double
getTime(void)
{
    struct timeval time;
    gettimeofday(&time);
    return ((double) time.tv_sec + (double) time.tv_usec * 1E-6);
}

static Boolean
animateWP(XtPointer clientData)
{
    double currentTime = getTime();
    double elapsed = currentTime - lastTime;

    lastTime = currentTime;

    rotation += rotationRate * ((elapsed <= 1.0) ? elapsed : 1.0);
    if (rotation >= 360) rotation -= 360;

    redraw();
    return False;
}

static void
popupCB(Widget w, XtPointer clientData, XtPointer callData)
{
    int button = (int) clientData;
    Bool needsRedraw = False;

    switch (button) {
      case 0:
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	needsRedraw = True;
	break;
      case 1:
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	needsRedraw = True;
	break;
      case 2:
	animationEnabled = !animationEnabled;
	if (animationEnabled) {
	    lastTime = getTime();
	    appWorkProcId = XtAppAddWorkProc(appContext, animateWP, NULL);
	} else {
	    XtRemoveWorkProc(appWorkProcId);
	}
	break;
      case 3:
	stereoEnabled = !stereoEnabled;
	if (stereoEnabled) {
	    stereoEnable();
	} else {
	    stereoDisable();
	}
	needsRedraw = True;
	break;
      case 4:
	stereoDone();
	exit(EXIT_SUCCESS);
	break;
      default:
	break;
    }
    
    if (needsRedraw) {
	redraw();
    }
}

static void
popupEH(Widget w, XtPointer clientData, XEvent *event, Boolean *cont)
{
    Widget popup = *((Widget *) clientData);

    if ((event->type == ButtonPress) && (event->xbutton.button == Button3)) {
	XmMenuPosition(popup, &event->xbutton);
	XtManageChild(popup);
    }
}

static void
exposeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    redraw();
}

static void
resizeCB(Widget w, XtPointer clientData, XtPointer callData)
{
    GLwDrawingAreaCallbackStruct *glwcallData = 
	(GLwDrawingAreaCallbackStruct *) callData;

    glViewport(0, 0, glwcallData->width, glwcallData->height);
    redraw();
}

int
main(int argc, char **argv)
{
    int stereoAttrs[] = {
	GLX_STEREO,
	GLX_RGBA,
	GLX_DOUBLEBUFFER,
	GLX_RED_SIZE, 1,
	GLX_DEPTH_SIZE, 1,
	None,
    };
    int visualAttrs[] = {
	GLX_RGBA,
	GLX_DOUBLEBUFFER,
	GLX_RED_SIZE, 1,
	GLX_DEPTH_SIZE, 1,
	None,
    };
    Display *dpy;
    int scrn;
    XVisualInfo *vis;
    Arg args[10];
    int n;

    if (argc > 1 && strcmp(argv[1], "-nosetmon") == 0)
	runSetmon = GL_FALSE;

    topLevel = XtOpenApplication(&appContext, APP_CLASS, NULL, 0,
				 &argc, argv, fallbackResources,
				 applicationShellWidgetClass, NULL, 0);
    
    XtGetApplicationResources(topLevel, &appResources, appResourceList,
			      XtNumber(appResourceList), NULL, 0);

    dpy = XtDisplay(topLevel);
    scrn = XScreenNumberOfScreen(XtScreen(topLevel));

    if ((vis = glXChooseVisual(dpy, scrn, stereoAttrs)) != NULL) {
	/* initialize for use with a stereo capable visual */
	stereoInit(True,
		   appResources.stereoCommand,
		   appResources.restoreCommand);

    } else if ((vis = glXChooseVisual(dpy, scrn, visualAttrs)) != NULL) {
	/* initialize for use without a stereo capable visual */
	stereoInit(False,
		   appResources.SGIStereoCommand,
		   appResources.SGIRestoreCommand);

	/* Force the topLevel widget to maintain a 2:1 aspect ratio */
	n = 0;
	XtSetArg(args[n], XmNminAspectX, 2); n++;
	XtSetArg(args[n], XmNminAspectY, 1); n++;
	XtSetArg(args[n], XmNmaxAspectX, 2); n++;
	XtSetArg(args[n], XmNmaxAspectY, 1); n++;
	XtSetValues(topLevel, args, n);

    } else {
	fprintf(stderr, "can't find appropriate visual\n");
	exit(EXIT_FAILURE);
    }

    if ((ctx = glXCreateContext(dpy, vis, 0, True)) == NULL) {
	fprintf(stderr, "can't create rendering context\n");
	exit(EXIT_FAILURE);
    }

    form = XtCreateManagedWidget("form", xmFormWidgetClass, topLevel, NULL, 0);

    n = 0;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], GLwNvisualInfo, vis); n++;
    glw = XtCreateManagedWidget("glw", glwMDrawingAreaWidgetClass,
                                form, args, n);
    XtAddCallback(glw, GLwNexposeCallback, exposeCB, 0);
    XtAddCallback(glw, GLwNresizeCallback, resizeCB, 0);

    {
	XmButtonType buttonTypes[] = {
	    XmPUSHBUTTON, XmPUSHBUTTON, XmSEPARATOR, XmPUSHBUTTON,
	    XmSEPARATOR, XmPUSHBUTTON, XmSEPARATOR, XmPUSHBUTTON,
	};
	XmString buttonLabels[XtNumber(buttonTypes)];

	buttonLabels[0] = XmStringCreateLocalized("draw filled");
	buttonLabels[1] = XmStringCreateLocalized("draw lines");
	buttonLabels[2] = NULL;
	buttonLabels[3] = XmStringCreateLocalized("toggle animation");
	buttonLabels[4] = NULL;
	buttonLabels[5] = XmStringCreateLocalized("toggle stereo mode");
	buttonLabels[6] = NULL;
	buttonLabels[7] = XmStringCreateLocalized("quit");

	n = 0;
	XtSetArg(args[n], XmNbuttonCount, XtNumber(buttonTypes)); n++;
	XtSetArg(args[n], XmNbuttonType, buttonTypes); n++;
	XtSetArg(args[n], XmNbuttons, buttonLabels); n++;
	XtSetArg(args[n], XmNsimpleCallback, popupCB); n++;
	popup = XmCreateSimplePopupMenu(form, "popup", args, n);
	XtAddEventHandler(form, ButtonPressMask, False, popupEH, &popup);

	XmStringFree(buttonLabels[0]);
	XmStringFree(buttonLabels[1]);
	XmStringFree(buttonLabels[3]);
	XmStringFree(buttonLabels[5]);
	XmStringFree(buttonLabels[7]);
    }

    XtRealizeWidget(topLevel);

    GLwDrawingAreaMakeCurrent(glw, ctx);
    stereoMakeCurrent(dpy, XtWindow(glw), ctx);
    initialize();

    XtAppMainLoop(appContext);
}
