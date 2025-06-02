/*
 * detail - simple program for detail texturing
 *
 * Press n key to move the polygon nearer
 *       f key to move the polygon farther
 */
/* compile: cc -o detail detail.c -lGLU -lGL -lX11 */

#include <GL/glx.h>
#include <GL/glu.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

unsigned int tex[128][128];
unsigned int detailtex[256][256];

static void
make_textures(void) {
    int i, j;
    unsigned int *p;

    /* base texture is solid gray */
    p = &tex[0][0];
    for (i=0; i<128*128; i++) *p++ = 0x808080ff;

    /* detail texture is a yellow grid over a gray background */
    p = &detailtex[0][0];
    for (i=0; i<256; i++) {
	for (j=0; j<256; j++) {
	    if (i%8 == 0 || j%8 == 0) {
		*p++ = 0xffff00ff;
	    } else {
		*p++ = 0x808080ff;
	    }
	}
    }
}

static void
init(void) {
    make_textures();
    
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    gluPerspective(90.0, 1.0, 0.3, 10.0 );
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0.,0.,-1.5);
    
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    
    /* note that parameters are applied to the base texture, not the detail */
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		    GL_LINEAR_DETAIL_SGIS);
    glTexParameteri(GL_TEXTURE_2D, GL_DETAIL_TEXTURE_LEVEL_SGIS, -1);
    glTexImage2D(GL_TEXTURE_2D,
		 0, 4, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);
    glTexImage2D(GL_DETAIL_TEXTURE_2D_SGIS,
		 0, 4, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, detailtex);
}

static void
draw_scene(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_TRIANGLE_STRIP);
    	glTexCoord2f( 0, 0); glVertex3f(-1,-0.4, 1); 
    	glTexCoord2f( 0, 1); glVertex3f(-1,-0.4,-1); 
    	glTexCoord2f( 1, 0); glVertex3f( 1,-0.4, 1); 
    	glTexCoord2f( 1, 1); glVertex3f( 1,-0.4,-1); 
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
	    switch (keysym) {
	      case XK_n:
		glTranslatef(0.,0.,.05);
		redraw = 1;
		break;
	      case XK_f:
		glTranslatef(0.,0.,-.05);
		redraw = 1;
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
    XStoreName(dpy, win, "detail");
    XMapWindow(dpy, win);
    glXMakeCurrent(dpy, win, cx);

    init();
    while (1) process_input(dpy);
}
