/*
 * simplest - simple single buffered RGBA motif program.
 */
/* compile: cc -o simplest simplest.c -lGLw -lGL -lXm -lXt -lX11 */
#include <stdlib.h>
#include <stdio.h>
#include <Xm/Frame.h>
#include <X11/GLw/GLwMDrawA.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <GL/glx.h>

static int attribs[] = { GLX_RGBA, GLX_RED_SIZE, 1, None};

static String fallbackResources[] = {
    "*sgiMode:	True",
    "*useSchemes: all",
    "*glxwidget*width: 300", "*glxwidget*height: 300",
    "*frame*shadowType: SHADOW_IN",
    NULL};

void
draw_scene(void) {
   glClearColor(0.5, 0.5, 0.5, 1.0);
   glClear(GL_COLOR_BUFFER_BIT);
   glColor3f(1.0,0.0,0.0);
   glRectf(-.5,-.5,.5,.5);
   glColor3f(0.0,1.0,0.0);
   glRectf(-.4,-.4,.4,.4);
   glColor3f(0.0,0.0,1.0);
   glRectf(-.3,-.3,.3,.3);
   glFlush();
}

static void
input(Widget w, XtPointer client_data, XtPointer call) {
   char buf[31];
   KeySym keysym;
   XEvent *event = ((GLwDrawingAreaCallbackStruct *) call)->event;

   switch(event->type) {
   case KeyRelease:
      XLookupString(&event->xkey, buf, sizeof buf, &keysym, NULL);
      switch(keysym) {
      case XK_Escape :
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

    draw_scene();
}

main(int argc, char *argv[]) {
    Display        *dpy;
    XtAppContext    app;
    XVisualInfo    *visinfo;
    GLXContext      glxcontext;
    Widget          toplevel, frame, glxwidget;

    toplevel = XtOpenApplication(&app, "simplest", NULL, 0, &argc, argv,
		fallbackResources, applicationShellWidgetClass, NULL, 0);
    dpy = XtDisplay(toplevel);

    frame = XmCreateFrame(toplevel, "frame", NULL, 0);
    XtManageChild(frame);

    /* specify visual directly */
    if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs)))
	XtAppError(app, "no suitable RGB visual");

    glxwidget = XtVaCreateManagedWidget("glxwidget", glwMDrawingAreaWidgetClass,
	frame, GLwNvisualInfo, visinfo, NULL);
    XtAddCallback(glxwidget, GLwNexposeCallback, expose, NULL);
    XtAddCallback(glxwidget, GLwNresizeCallback, resize, NULL);
    XtAddCallback(glxwidget, GLwNinputCallback, input, NULL);

    XtRealizeWidget(toplevel);

    glxcontext = glXCreateContext(dpy, visinfo, 0, GL_TRUE);
    GLwDrawingAreaMakeCurrent(glxwidget, glxcontext);

    XtAppMainLoop(app);
}
