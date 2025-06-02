/*
 * overlay.c - simple double buffered RGBA xlib program which rotates an object.
 *	An overlay window is created as a child of the main window and it is
 *	managed as part of the parent window.  The overlay window/context
 *	creation uses the visual_info extension to find a visual with
 *	a transparent pixel.
 */
/* compile: cc -o overlay overlay.c -lGLU -lGL -lX11 */

#include <GL/glx.h>
#include <GL/glu.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

static int attributeList[] = { GLX_RGBA, GLX_RED_SIZE, 1, GLX_DOUBLEBUFFER, None };
static int ovAttributeList[] = { GLX_BUFFER_SIZE, 2,
				 GLX_LEVEL, 1,
				 GLX_TRANSPARENT_TYPE_EXT, GLX_TRANSPARENT_INDEX_EXT,
				 None };
static Window win, ovwin;
static GLXContext cx, ovcx;
static Display *dpy;

static Bool animate;
unsigned long ov_pixel;

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

/*
 * the amount of work being done doesn't justify the need for
 * a separate overlay initialization and redraw routines so we
 * do it all together.
 */
static void
initialize_ov(void) {
    int i;
    /* simply draw a grid of 10 equally spaced lines */
    glLoadIdentity();
    glOrtho(0., 11., 0., 11., -1., 1.);
    glClearIndex(0.); /* transparent value is always zero */
    glClear(GL_COLOR_BUFFER_BIT);
    glIndexi(ov_pixel);
    glBegin(GL_LINES);
    for(i = 0; i < 11; i++) {
	glVertex2f(0.5+i, 0.0);
	glVertex2f(0.5+i, 11.0);
	glVertex2f(0.0,  0.5+i);
	glVertex2f(11.0, 0.5+i);
    }
    glEnd();
}

static Bool
process_input(void) {
    XEvent event;
    static Bool mapped;
    Bool viewport_ov = 0, redraw_ov = 0, redraw = 0;
    GLsizei w, h;

    if(XPending(dpy) || !mapped || !animate) {
	char buf[31];
	KeySym keysym;

	XNextEvent(dpy, &event);
	switch(event.type) {
	case Expose:
	    redraw_ov = redraw = 1;
	    break;
	case ConfigureNotify:
	    glViewport(0, 0, w = event.xconfigure.width, h = event.xconfigure.height);
	    redraw_ov = redraw = 1;
	    viewport_ov = 1;
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
    if (redraw_ov) {
	glXMakeCurrent(dpy, ovwin, ovcx);
	if (viewport_ov) {
	    XResizeWindow(dpy, ovwin, w, h);
	    glViewport(0, 0, w, h);
	}
	initialize_ov();
	glXMakeCurrent(dpy, win, cx);
    }
    return redraw | animate;
}

int
main(int argc, char **argv) {
    XVisualInfo *vi, *ovi;
    XSetWindowAttributes swa;
    Bool isdirect;
    XColor col;

    dpy = XOpenDisplay(0);

    if (!(vi = glXChooseVisual(dpy, DefaultScreen(dpy), attributeList))) {
	fprintf(stderr, "overlay: no suitable RGB visual available\n");
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
    XStoreName(dpy, win, "overlay");
    XMapWindow(dpy, win);

    glXMakeCurrent(dpy, win, cx);

    initialize();

    /* now set up overlay window */

    if (!(ovi = glXChooseVisual(dpy, DefaultScreen(dpy), ovAttributeList))) {
	fprintf(stderr, "overlay: no suitable overlay index visual available\n");
	exit(EXIT_FAILURE);
    }
    ovcx = glXCreateContext(dpy, ovi, 0, GL_TRUE);

    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, ovi->screen),
                                   ovi->visual, AllocNone);
    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;
    ovwin = XCreateWindow(dpy, win, 0, 0, 300, 300,
			0, ovi->depth, InputOutput, ovi->visual,
			CWBorderPixel|CWColormap|CWEventMask, &swa);
    XSetWMColormapWindows(dpy, win, &ovwin, 1);
    XMapWindow(dpy, ovwin);
    /* allocate and initialize one overlay color cell */
    if (!XAllocColorCells(dpy, swa.colormap, True, NULL, 0, &ov_pixel, 1))
	fprintf(stderr, "can't alloc overlay color cell");
    col.pixel = ov_pixel;
    col.red = 65535;
    col.green = 0;
    col.blue = 0;
    col.flags = DoRed | DoGreen | DoBlue;
    XStoreColor(dpy, swa.colormap, &col);

    glXMakeCurrent(dpy, ovwin, ovcx);
    initialize_ov();
    glXMakeCurrent(dpy, win, cx);

    printf("hit 'spacebar' to toggle animation\n");

    while (1) {
	if (process_input()) {
	    draw_scene();
	    glXSwapBuffers(dpy, win);
	    if (!isdirect) glFinish();
	}
    }
}
