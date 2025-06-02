#include "gl/image.h"
#include "GL/glx.h"
#include <X11/keysym.h>
#include "stdlib.h"

short rbuf[8192]; 
short gbuf[8192]; 
short bbuf[8192]; 

Display *dpy;
Window window;

static void make_window(int width, int height, char *name, int border);

/* convert to luminance using formula l = .3086*r + .0820*g + .114*b */
static void
bw_matrix(void) {
static GLfloat bwmatrix[] = {
    0.3086f, 0.3086f, 0.3086f, 0.0f,
    0.0820f, 0.0820f, 0.0820f, 0.0f,
    0.114f,  0.114f,  0.114f,  0.0f,
    0.0f,    0.0f,    0.0f,    1.0f
};
    glMatrixMode(GL_COLOR);
    glLoadMatrixf(bwmatrix);
    glMatrixMode(GL_MODELVIEW);
}

/* zero out all channels but red */
static void
red_matrix(void) {
static GLfloat redmatrix[] = {
    0.1f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};
    glMatrixMode(GL_COLOR);
    glLoadMatrixf(redmatrix);
    glMatrixMode(GL_MODELVIEW);
}

/* convert abgr to rgba */
static void
abgr_matrix(void) {
static GLfloat abgrmatrix[] = {
    0.0f, 0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 1.0f
};
    glMatrixMode(GL_COLOR);
    glLoadMatrixf(abgrmatrix);
    glMatrixMode(GL_MODELVIEW);
}

/* identity matrix */
static void
no_matrix(void) {
    glMatrixMode(GL_COLOR);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
}

/* adjust saturation by scaling luminance */
static void
sat_matrix(GLboolean increase) {
static GLfloat satmatrix[] = {
    0.3086f, 0.3086f, 0.3086f, 0.0f,
    0.0820f, 0.0820f, 0.0820f, 0.0f,
    0.114f,  0.114f,  0.114f,  0.0f,
    0.0f,    0.0f,    0.0f,    1.0f
};
    GLfloat tmatrix[16];
    GLint i;

    static GLfloat sat = 1.0, sat_inc = .05;
    sat += (increase)? sat_inc : -sat_inc;
    glMatrixMode(GL_COLOR);
    for(i = 0; i < 16; i++)
	tmatrix[i] = satmatrix[i]*(1.0-sat);
    tmatrix[0] += sat;
    tmatrix[5] += sat;
    tmatrix[10] += sat;
    glLoadMatrixf(tmatrix);
    glMatrixMode(GL_MODELVIEW);
}

main(int argc,char *argv[]) {
    register IMAGE *image;
    register int x, y, xsize, ysize;
    register int z, zsize;
    unsigned char *buf;
    int border = 1;

    if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'n') {
	border = 0;
	argc--; argv++;
    }
    if( argc<2 ) {
	fprintf(stderr,"usage: colormatrix [-n] inimage.rgb\n");
	exit(1);
    } 
    if( (image=iopen(argv[1],"r")) == NULL ) {
	fprintf(stderr,"colormatrix: can't open input file %s\n",argv[1]);
	exit(1);
    }
    xsize = image->xsize;
    ysize = image->ysize;
    zsize = image->zsize;
    if(zsize<3) {
	fprintf(stderr,"colormatrix: this is not an RGB image file\n");
	exit(1);
    }
    buf = (unsigned char *)malloc(xsize*ysize*3);
    for(y=0; y<ysize; y++) {
	getrow(image,rbuf,y,0);
	getrow(image,gbuf,y,1);
	getrow(image,bbuf,y,2);
	for(x = 0; x < xsize; x++) {
	    buf[(y*xsize+x)*3+0] = rbuf[x];
	    buf[(y*xsize+x)*3+1] = gbuf[x];
	    buf[(y*xsize+x)*3+2] = bbuf[x];
	}
	
    }
    make_window(xsize, ysize, argv[1], border);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glMatrixMode(GL_PROJECTION);
    glOrtho(0, image->xsize, 0, image->ysize, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glRasterPos2i(0, 0);
    glClearColor(0.5, 0.5, 0.5, 0.5);
    glClear(GL_COLOR_BUFFER_BIT);
    while(1) {
	GLboolean redraw = 0;
	XEvent ev;
	XNextEvent(dpy, &ev);
	switch(ev.type) {
	case Expose:
	    redraw = GL_TRUE;
	    break;
	case KeyPress: {
	    char buf[100];
	    int rv;
	    KeySym ks;

	    rv = XLookupString(&ev.xkey, buf, sizeof(buf), &ks, 0);
	    switch (ks) {
	    case XK_Escape:
		exit(0);
	    case XK_a:
		abgr_matrix();
		redraw = GL_TRUE;
		break;
	    case XK_b:
		bw_matrix();
		redraw = GL_TRUE;
		break;
	    case XK_n:
		no_matrix();
		redraw = GL_TRUE;
		break;
	    case XK_r:
		red_matrix();
		redraw = GL_TRUE;
		break;
	    case XK_s:
	    case XK_S:
		sat_matrix(ks == XK_s);
		redraw = GL_TRUE;
		break;
	    }
	    break;
	}
	}
	if (redraw)
	    glDrawPixels(xsize, ysize, GL_RGB, GL_UNSIGNED_BYTE, buf);
    }
}

static int attributeList[] = { GLX_RGBA, GLX_RED_SIZE, 1, None };

void
noborder(Display *dpy, Window win) {
    struct { long flags,functions,decorations,input_mode; } *hints;
    int fmt;
    unsigned long nitems,byaf;
    Atom type,mwmhints = XInternAtom(dpy,"_MOTIF_WM_HINTS",False);

    XGetWindowProperty(dpy,win,mwmhints,0,4,False, mwmhints,&type,&fmt,&nitems,&byaf,(unsigned char**)&hints);

    if (!hints) hints = (void *)malloc(sizeof *hints);
    hints->decorations = 0;
    hints->flags |= 2;

    XChangeProperty(dpy,win,mwmhints,mwmhints,32,PropModeReplace,(unsigned char *)hints,4);
    XFlush(dpy);
    free(hints);
}

static void
make_window(int width, int height, char *name, int border) {
    XVisualInfo *vi;
    Colormap cmap;
    XSetWindowAttributes swa;
    GLXContext cx;
    XSizeHints sizehints;

    dpy = XOpenDisplay(0);
    if (!(vi = glXChooseVisual(dpy, DefaultScreen(dpy), attributeList))) {
	printf("can't find requested visual\n");
	exit(1);
    }
    cx = glXCreateContext(dpy, vi, 0, GL_TRUE);

    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
                           vi->visual, AllocNone);
    sizehints.flags = 0;

    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask;
    window = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, width, height,
                        0, vi->depth, InputOutput, vi->visual,
                        CWBorderPixel|CWColormap|CWEventMask, &swa);
    if (!border) noborder(dpy, window);

    XMapWindow(dpy, window);
    XSetStandardProperties(dpy, window, name, name,
	    None, (void *)0, 0, &sizehints);


    glXMakeCurrent(dpy, window, cx);
}
