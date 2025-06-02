
#include <GL/glx.h>
#include <GL/glu.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_TREES       25

#define TEXSIZE 64
#define COMPONENTS 4

static GLint azimuth = 0;
static GLint elevation = 60;
static GLfloat distance  = 40.0;

static GLfloat treePos[MAX_TREES][3];
static GLint ntrees = 0;

static GLint spriteMode = GL_SPRITE_AXIAL_SGIX;

static GLfloat axis[3];
static GLfloat pos[3];

static GLubyte texture[TEXSIZE*TEXSIZE*COMPONENTS];

Display *dpy;
Window win;
/***********************************************************************/
static void
initTrees(void)
{

    GLint i;

    for (i = 0; i < MAX_TREES; i ++) {
        treePos[i][0] = (drand48() - 0.5) * 20.0;
        treePos[i][1] = (drand48() - 0.5) * 20.0;
        treePos[i][2] = 0.0;
    }

}

/***********************************************************************/
static void
defineTexture(void)
{

    int i, j;
    int xsize = TEXSIZE;
    int ysize = TEXSIZE;

    for(j = 0; j < TEXSIZE; j++){
        for (i = 0; i < TEXSIZE; i++){
	    texture[j*TEXSIZE*COMPONENTS+i*COMPONENTS+0] = random() & 0xff; 
	    texture[j*TEXSIZE*COMPONENTS+i*COMPONENTS+1] = random() & 0xff;
	    texture[j*TEXSIZE*COMPONENTS+i*COMPONENTS+2] = random() & 0xff;
	    texture[j*TEXSIZE*COMPONENTS+i*COMPONENTS+3] = random() & 0xff;
        }
    }

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,
                    GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gluBuild2DMipmaps(GL_TEXTURE_2D,
                    4, xsize, ysize, GL_RGBA, GL_UNSIGNED_BYTE, texture);

}

/***********************************************************************/
static void
initGraphics(void) {

    glMatrixMode(GL_PROJECTION);
    gluPerspective(40.0, 1.0, 1.0, 3000.0 );
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(0.,0.,-2.5);

    glColor4f(0,0,0,1);
    glClearColor(0.0, 0.0, 0.0, 1.0);

    defineTexture();
}


/***********************************************************************/
static void
drawScene(void) {

    int i;

    glClearColor(0.2f, 0.3f, 0.7f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPushMatrix();
    glTranslatef(0.0, 0.0, -distance);
    glRotatef(-(float) elevation, 1.0, 0.0, 0.0);
    glRotatef((float) azimuth, 0.0, 0.0, 1.0);

    glColor4f(0.0, 1.0, 0.0, 1.0);

    glBegin(GL_POLYGON);
	glVertex2f(-10.0, -10.0);
	glVertex2f(10.0, -10.0);
	glVertex2f(10.0, 10.0);
	glVertex2f(-10.0, 10.0);
    glEnd();

    glEnable(GL_TEXTURE_2D);

    glColor4f(1.0, 1.0, 1.0, 1.0);

    glEnable(GL_SPRITE_SGIX);

    axis[0] = 0.0;
    axis[1] = 0.0;
    axis[2] = 1.0;
    glSpriteParameterfvSGIX(GL_SPRITE_AXIS_SGIX, axis);

    for (i = 0; i < ntrees; i ++) {
	glSpriteParameteriSGIX (GL_SPRITE_MODE_SGIX, spriteMode);
        glSpriteParameterfvSGIX (GL_SPRITE_TRANSLATION_SGIX, treePos[i]);
	glBegin(GL_TRIANGLE_STRIP);
	    glTexCoord2d (0.0, 0.0);
	    glVertex3f (-1.0, 0.0, 0.0);
	    glTexCoord2d (0.0, 1.0);
	    glVertex3f (-1.0, 0.0, 3.0);
	    glTexCoord2d (1.0, 0.0);
	    glVertex3f (1.0, 0.0, 0.0);
	    glTexCoord2d (1.0, 1.0);
	    glVertex3f (1.0, 0.0, 3.0);
	glEnd();
    }

    glDisable (GL_TEXTURE_2D);
    glDisable (GL_SPRITE_SGIX);

    glFlush();
    glPopMatrix();

}

/***********************************************************************/
static void
processInput(Display *dpy) {
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
              case XK_Up:
		ntrees ++;
		if (ntrees >= MAX_TREES) ntrees = MAX_TREES - 1;
		fprintf(stderr, "drawing %d trees...\n", ntrees);
                break;
              case XK_Down:
		ntrees --;
		if (ntrees >= MAX_TREES) ntrees = MAX_TREES - 1;
		fprintf(stderr, "drawing %d trees...\n", ntrees);
                break;
              case XK_m:
		switch(spriteMode) {
		case GL_SPRITE_AXIAL_SGIX:
		    spriteMode = GL_SPRITE_OBJECT_ALIGNED_SGIX;
		    fprintf(stderr, 
			"sprite mode is GL_SPRITE_OBJECT_ALIGNED_SGIX\n");
		    break;
		case GL_SPRITE_OBJECT_ALIGNED_SGIX:
		    spriteMode = GL_SPRITE_EYE_ALIGNED_SGIX;
		    fprintf(stderr, 
			"sprite mode is GL_SPRITE_EYE_ALIGNED_SGIX\n");
		    break;
		case GL_SPRITE_EYE_ALIGNED_SGIX:
		    spriteMode = GL_SPRITE_AXIAL_SGIX;
		    fprintf(stderr, 
			"sprite mode is GL_SPRITE_AXIAL_SGIX\n");
		    break;
		}
                break;
              case XK_a:
		azimuth++;
		azimuth %= 360;
		break;
              case XK_question:
		fprintf(stderr, "key stroke usage:\n");
		fprintf(stderr, 
		    "\t'up arrow' key to increment number of trees\n");
		fprintf(stderr, 
		    "\t'down arow' key to decrement number of trees\n");
		fprintf(stderr, "\t'a' key for azimuth (rotation) \n");
		fprintf(stderr, "\t'm' switch sprite rendering modes\n");
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
    if(redraw) drawScene();
    glXSwapBuffers(dpy, win);
}

/***********************************************************************/
static void
error(const char *prog, const char *msg) {
    fprintf(stderr, "%s: %s\n", prog, msg);
    exit(EXIT_FAILURE);
}

static int attributeList[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };

/***********************************************************************/
int
main(int argc, char **argv) {
    XVisualInfo *vi;
    XSetWindowAttributes swa;
    GLXContext cx;
    char *str;

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
    XStoreName(dpy, win, "sprite");
    XMapWindow(dpy, win);
    glXMakeCurrent(dpy, win, cx);

    if(!glGetString(GL_EXTENSIONS) ||
          !strstr((const char *)glGetString(GL_EXTENSIONS), "GL_SGIX_sprite")) {
	fprintf(stderr, "SGI_sprite extension is unsupported\n");
	exit(1);
    }

    initTrees();
    initGraphics();

    fprintf(stderr, "hit '?' to get key stroke options\n");

    while (1) processInput(dpy);
}

