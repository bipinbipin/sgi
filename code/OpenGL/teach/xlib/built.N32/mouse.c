/*
 * mouse - double buffered RGBA xlib program which uses mouse motion events.
 */
/* compile: cc -o mouse mouse.c -lGLU -lGL -lX11 */

#include <GL/glx.h>
#include <GL/glu.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

static int attributeList[] = { GLX_RGBA, GLX_RED_SIZE, 1, GLX_DOUBLEBUFFER, None };

GLfloat trans[3];	/* current translation */
GLfloat rot[2];		/* current rotation */

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
draw_scene(void) {

    glClear(GL_COLOR_BUFFER_BIT);
    glPushMatrix();
    glTranslatef(trans[0], trans[1], trans[2]);
    glRotatef(rot[0], 1.0, 0.0, 0.0);
    glRotatef(rot[1], 0.0, 1.0, 0.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3f(.1, .1, .8);
    cube();
    glScalef(0.3,0.3,0.3);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColor3f(.8, .8, .1);
    cube();
    glPopMatrix();
}

static void
update_view(int mstate, int ox, int nx, int oy, int ny) {
    int dx = ox - nx;
    int dy = ny - oy;

    switch(mstate) {
    case 1:	/* pan */
	trans[0] -= dx/100.;
	trans[1] -= dy/100.;
	break;
    case 2:	/* rotate */
	rot[0] += (dy * 180.0)/500.;
	rot[1] -= (dx * 180.0)/500.;
#define clamp(x) x = x > 360. ? x-360. : x < -360. ? x += 360. : x 
	clamp(rot[0]);
	clamp(rot[1]);
	break;
    case 3:	/* zoom */
	trans[2] -= (dx+dy)/100.;
	break;
    }
}

static int
process_input(Display *dpy) {
    XEvent event;
    Bool redraw = 0;
    static int mstate, omx, omy, mx, my;

    do {
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
	case KeyPress:
	    (void) XLookupString(&event.xkey, buf, sizeof(buf), &keysym, NULL);
	    switch (keysym) {
	    case XK_Escape:
		exit(EXIT_SUCCESS);
	    default:
		break;
	    }
	case ButtonPress:
	    if (event.xbutton.button == Button2) {
		mstate |= 2;
		mx = event.xbutton.x;
		my = event.xbutton.y;
	    } else if (event.xbutton.button == Button1) {
		mstate |= 1;
		mx = event.xbutton.x;
		my = event.xbutton.y;
	    }
	    break;
	case ButtonRelease:
	    if (event.xbutton.button == Button2)
		mstate &= ~2;
	    else if (event.xbutton.button == Button1)
		mstate &= ~1;
	    break;
	case MotionNotify:
	    if (mstate) {
		omx = mx;
		omy = my;
		mx = event.xbutton.x;
		my = event.xbutton.y;
		update_view(mstate, omx,mx,omy,my);
		redraw = 1;
	    }
	    break;
	default:
	    break;
	}
    } while (XPending(dpy));
    return redraw;
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
	fprintf(stderr, "mouse: no suitable RGB visual available\n");
	exit(EXIT_FAILURE);
    }

    cx = glXCreateContext(dpy, vi, 0, GL_TRUE);
    isdirect = glXIsDirect(dpy, cx);

    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
                                   vi->visual, AllocNone);

    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask |
	ButtonPressMask | ButtonReleaseMask | ButtonMotionMask ;
    win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, 300, 300,
			0, vi->depth, InputOutput, vi->visual,
			CWBorderPixel|CWColormap|CWEventMask, &swa);
    XStoreName(dpy, win, "mouse");
    XMapWindow(dpy, win);

    glXMakeCurrent(dpy, win, cx);

    initialize();

    while (1) {
	if (process_input(dpy)) {
	    draw_scene();
	    glXSwapBuffers(dpy, win);
	    if (!isdirect) glFinish();
	}
    }
}
