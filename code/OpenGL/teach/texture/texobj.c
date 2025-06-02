/*
 * texobj - texture object program
 *
 * Simple example for how to create, define, and bind texture objects.
 */
/* compile: cc -o texobj texobj.c -lGL -lX11 */

#include <GL/glx.h>
#include <GL/glu.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

static int attributeList[] = { GLX_RGBA, None };
unsigned int redtex[64*64], greentex[64*64], bluetex[64*64];

GLuint texnames[3];
GLclampf priorities[3] = { 0.0, 1.0, 1.0 };

static void
init(void) {
    int i;
    
    glMatrixMode(GL_PROJECTION);
    gluPerspective(60.0, 1.0, 1.0, 100.0 );
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0.,0.,-8.0);

    glClearColor(0.5, 0.5, 0.5, 1.0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (i=0; i < 64*64; i++) {
	redtex[i] = 0xff000000;
	greentex[i] = 0x00ff0000;
	bluetex[i] = 0x0000ff00;
    }

    glEnable(GL_TEXTURE_2D);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    /* get some unused texture names */
    glGenTexturesEXT(3, texnames);

    /* bind, then define, each texture */
    glBindTextureEXT(GL_TEXTURE_2D, texnames[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		 redtex);
    glBindTextureEXT(GL_TEXTURE_2D, texnames[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		 greentex);
    glBindTextureEXT(GL_TEXTURE_2D, texnames[2]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		 bluetex);

    /* optional; define a working set by giving some textures a higher */
    /* priority than others */
    glPrioritizeTexturesEXT(3, texnames, priorities);
}

static void
draw_square(void)
{
    glBegin(GL_TRIANGLE_STRIP);
    	glTexCoord2f(0,0); glVertex2f(-1,-1); 
    	glTexCoord2f(0,1); glVertex2f(-1, 1); 
    	glTexCoord2f(1,0); glVertex2f( 1,-1); 
    	glTexCoord2f(1,1); glVertex2f( 1, 1); 
    glEnd();
}

static void
draw_scene(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glPushMatrix();

    glTranslatef(-3, 0, 0);
    glBindTextureEXT(GL_TEXTURE_2D, texnames[0]);
    draw_square();
    glTranslatef( 3, 0, 0);
    glBindTextureEXT(GL_TEXTURE_2D, texnames[1]);
    draw_square();
    glTranslatef( 3, 0, 0);
    glBindTextureEXT(GL_TEXTURE_2D, texnames[2]);
    draw_square();

    glPopMatrix();
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
    win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, 400, 400,
			0, vi->depth, InputOutput, vi->visual,
			CWBorderPixel|CWColormap|CWEventMask, &swa);
    XStoreName(dpy, win, "texobj");
    XMapWindow(dpy, win);
    glXMakeCurrent(dpy, win, cx);

    init();
    while (1) process_input(dpy);
}
