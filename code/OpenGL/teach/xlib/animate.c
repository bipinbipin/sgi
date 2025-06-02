/*
 * animate - simple double buffered RGBA xlib program which rotates an object.
 */
/* compile: cc -o animate animate.c -lGLU -lGL -lX11 */

#include <GL/glx.h>
#include <GL/glu.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

static int attributeList[] = { GLX_RGBA, GLX_RED_SIZE, 1, GLX_DOUBLEBUFFER, None };

static Bool animate;

static void
initialize(void) {
    glShadeModel(GL_FLAT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
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
draw_scene(void) {
    static float rot = 0.;

    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(.1, .1, .8);
    glPushMatrix();
    if (animate && (rot += 5.) > 360.) rot -= 360.;
    glRotatef(rot,0.,1.,0.);
    cube();
    glScalef(0.3,0.3,0.3);
    glColor3f(.8, .8, .1);
    cube();
    glPopMatrix();
}

static Bool
process_input(Display *dpy) {
    XEvent event;
    static Bool mapped;
    Bool redraw = 0;

    if(XPending(dpy) || !mapped || !animate) {
	char buf[31];
	KeySym keysym;

	XNextEvent(dpy, &event);
	switch(event.type) {
	case Expose:
	    redraw = 1;
	    break;
	case ConfigureNotify:
	    glViewport(0, 0, event.xconfigure.width, event.xconfigure.height);
	    redraw = 1;
	    break;
	case MapNotify:
	    mapped = 1; break;
	case UnmapNotify:
	    mapped = 0; break;
	case KeyPress:
	    (void) XLookupString(&event.xkey, buf, sizeof(buf), &keysym, NULL);
	    switch (keysym) {
	    case XK_Escape:
		exit(EXIT_SUCCESS);
	    case XK_space:
		animate ^= 1; break;
	    default:
		break;
	    }
	default:
	    break;
	}
    }
    return redraw | animate;
}

int
main(int argc, char **argv) {
    Display *dpy;
    XVisualInfo *vi;
    XSetWindowAttributes swa;
    Window win;
    GLXContext cx;
    Bool isdirect;

    dpy = XOpenDisplay(0);

    if (!(vi = glXChooseVisual(dpy, DefaultScreen(dpy), attributeList))) {
	fprintf(stderr, "animate: no suitable RGB visual available\n");
	exit(EXIT_FAILURE);
    }

    /* create a GLX context */
    cx = glXCreateContext(dpy, vi, 0, GL_TRUE);
    isdirect = glXIsDirect(dpy, cx);

    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
                                   vi->visual, AllocNone);

    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;
    win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, 300, 300,
			0, vi->depth, InputOutput, vi->visual,
			CWBorderPixel|CWColormap|CWEventMask, &swa);
    XStoreName(dpy, win, "animate");
    XMapWindow(dpy, win);

    glXMakeCurrent(dpy, win, cx);

    initialize();

    printf("hit 'spacebar' to toggle animation\n");

    while (1) {
	if (process_input(dpy)) {
	    draw_scene();
	    glXSwapBuffers(dpy, win);
	    if (!isdirect) glFinish();
	}
    }
}
