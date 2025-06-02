/*
 * tex3d - simple 3D texturing program
 *
 * Shows a 3D texture by drawing slices through it.
 */
/* compile: cc -o tex3d tex3d.c -lGLU -lGL -lX11 */

#include <GL/glx.h>
#include <GL/glu.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

static int attributeList[] = { GLX_RGBA, None };

unsigned int tex[64][64][64];

/* generate a simple 3D texture */
static void
make_texture(void) {
    int i, j, k;
    unsigned int *p = &tex[0][0][0];

    for (i=0; i<64; i++) {
	for (j=0; j<64; j++) {
	    for (k=0; k<64; k++) {
		if (i < 10 || i > 48 ||
		    j < 10 || j > 48 ||
		    k < 10 || k > 48) {
		    if (i < 2 || i > 62 ||
			j < 2 || j > 62 ||
			k < 2 || k > 62) {
			*p++ = 0x00000000;
		    } else {
			*p++ = 0xff80ffff;
		    }
		} else {
		    *p++ = 0x000000ff;
		}
	    }
	}
    }
}

static void
init(void) {
    make_texture();
    glEnable(GL_TEXTURE_3D_EXT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glClearColor(0.2,0.2,0.5,1.0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glMatrixMode(GL_PROJECTION);
    gluPerspective(60.0, 1.0, 1.0, 100.0 );
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0.,0.,-3.0);
    glMatrixMode(GL_TEXTURE);

    /* Similar to defining a 2D texture, but note the setting of the */
    /* wrap parameter for the R coordinate.  Also, for 3D textures   */
    /* you probably won't need mipmaps, hence the linear min filter. */
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_3D_EXT, GL_TEXTURE_WRAP_R_EXT, GL_CLAMP);
    glTexImage3DEXT(GL_TEXTURE_3D_EXT, 0, 4, 64, 64, 64, 0,
		    GL_RGBA, GL_UNSIGNED_BYTE, tex);
}

#define NUMSLICES 256

static void
draw_scene(void) {
    int i;
    float r, dr, z, dz;
    
    glColor4f(1, 1, 1, 1.4/NUMSLICES);
    glClear(GL_COLOR_BUFFER_BIT);

    /* Display the entire 3D texture by drawing a series of quads that  */
    /* slice through the texture coordinate space.  Note that the       */
    /* transformations below are applied to the texture matrix, not the */
    /* modelview matrix. */
       
    glLoadIdentity();
    /* center the texture coords around the [0,1] cube */
    glTranslatef(.5,.5,.5);
    /* a rotation just to make the picture more interesting */
    glRotatef(45.,1.,1.,.5);

    /* to make sure that the texture coords, after arbitrary rotations, */
    /* still fully contain the [0,1] cube, make them span a range       */
    /* sqrt(3)=1.74 wide */
    r = -0.87; dr = 1.74/NUMSLICES;
    z = -1.00; dz = 2.00/NUMSLICES;
    for (i=0; i < NUMSLICES; i++) {
	glBegin(GL_TRIANGLE_STRIP);
	glTexCoord3f(-.87,-.87,r); glVertex3f(-1,-1,z); 
	glTexCoord3f(-.87, .87,r); glVertex3f(-1, 1,z); 
	glTexCoord3f( .87,-.87,r); glVertex3f( 1,-1,z); 
	glTexCoord3f( .87, .87,r); glVertex3f( 1, 1,z); 
	glEnd();
	r += dr;
	z += dz;
    }
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
    XStoreName(dpy, win, "tex3d");
    XMapWindow(dpy, win);
    glXMakeCurrent(dpy, win, cx);

    init();
    while (1) process_input(dpy);
}
