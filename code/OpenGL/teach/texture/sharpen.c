/*
 * sharpen - simple program for texture sharpening
 *
 * Press l key for bilinear magnification
 *       a key to sharpen alpha
 *       c key to sharpen color
 *       s key to sharpen alpha and color
 */
/* compile: cc -o sharpen sharpen.c -lGLU -lGL -lX11 */

#include <GL/glx.h>
#include <GL/glu.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

/* tree texture: high alpha in foreground, zero alpha in background */
#define B 0x00000000
#define F 0xA0A0A0ff
unsigned int tex[] = {
    B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,F,F,B,B,B,B,B,B,B,
    B,B,B,B,B,B,B,F,F,B,B,B,B,B,B,B,
    B,B,B,B,B,B,F,F,F,F,B,B,B,B,B,B,
    B,B,B,B,B,B,F,F,F,F,B,B,B,B,B,B,
    B,B,B,B,B,F,F,F,F,F,F,B,B,B,B,B,
    B,B,B,B,B,F,F,F,F,F,F,B,B,B,B,B,
    B,B,B,B,F,F,F,F,F,F,F,F,B,B,B,B,
    B,B,B,B,F,F,F,F,F,F,F,F,B,B,B,B,
    B,B,B,F,F,F,F,F,F,F,F,F,F,B,B,B,
    B,B,B,F,F,F,F,F,F,F,F,F,F,B,B,B,
    B,B,F,F,F,F,F,F,F,F,F,F,F,F,B,B,
    B,B,F,F,F,F,F,F,F,F,F,F,F,F,B,B,
    B,B,B,B,B,B,F,F,F,F,B,B,B,B,B,B,
    B,B,B,B,B,B,F,F,F,F,B,B,B,B,B,B,
    B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,B,
};

static void
init(void) {
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    gluPerspective(60.0, 1.0, 1.0, 10.0 );
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0.,0.,-2.5);

    glColor4f(0,0,0,1);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    /* sharpening just alpha is useful for keeping the tree outline crisp */
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		    GL_LINEAR_SHARPEN_ALPHA_SGIS);
    /* generate mipmaps; levels 0 and 1 are needed for sharpening */
    gluBuild2DMipmaps(GL_TEXTURE_2D, 4, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE,
		      tex);
}

static void
draw_scene(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLE_STRIP);
    	glTexCoord2f( 0, 1); glVertex2f(-1,-1); 
    	glTexCoord2f( 0, 0); glVertex2f(-1, 1); 
    	glTexCoord2f( 1, 1); glVertex2f( 1,-1); 
    	glTexCoord2f( 1, 0); glVertex2f( 1, 1); 
    glEnd();
    glFlush();
}

static void
process_input(Display *dpy) {
    XEvent event;
    Bool redraw = 0;

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
	    redraw = 1;
	    switch (keysym) {
	      case XK_s:
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_LINEAR_SHARPEN_SGIS);
		break;
	      case XK_a:
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_LINEAR_SHARPEN_ALPHA_SGIS);
		break;
	      case XK_c:
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_LINEAR_SHARPEN_COLOR_SGIS);
		break;
	      case XK_l:
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
				GL_LINEAR);
		break;
	      case XK_Escape:
		exit(EXIT_SUCCESS);
	      default:
		break;
	    }
	  default:
	    break;
	}
    } while (XPending(dpy));
    if (redraw) draw_scene();
}

static void
error(const char *prog, const char *msg) {
    fprintf(stderr, "%s: %s\n", prog, msg);
    exit(EXIT_FAILURE);
}

static int attributeList[] = { GLX_RGBA, None };

int
main(int argc, char **argv) {
    Display *dpy;
    XVisualInfo *vi;
    XSetWindowAttributes swa;
    Window win;
    GLXContext cx;

    dpy = XOpenDisplay(0);
    if (!dpy) error(argv[0], "can't open display");
    vi = glXChooseVisual(dpy, DefaultScreen(dpy), attributeList);
    if (!vi) error(argv[0], "no suitable visual");
    cx = glXCreateContext(dpy, vi, 0, GL_TRUE);

    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
                                   vi->visual, AllocNone);
    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;
    win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, 800, 800,
			0, vi->depth, InputOutput, vi->visual,
			CWBorderPixel|CWColormap|CWEventMask, &swa);
    XStoreName(dpy, win, "sharpen");
    XMapWindow(dpy, win);
    glXMakeCurrent(dpy, win, cx);

    init();
    while (1) process_input(dpy);
}
