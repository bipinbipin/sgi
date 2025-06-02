#ifdef DMKUDZU
#define GL_YCRCB_422_SGIX	0x8193
#endif

#include <stdlib.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include "dmplay.h"


static int attributeList[] = { GLX_RGBA, 
				GLX_RED_SIZE, 8,
				GLX_GREEN_SIZE, 8,
				GLX_BLUE_SIZE, 8,
				GLX_DEPTH_SIZE, 1,
				None };

static int impactgfx_attributeList[] = { GLX_RGBA,
					GLX_DOUBLEBUFFER,
					GLX_RED_SIZE, 1,
					GLX_GREEN_SIZE, 1,
					GLX_BLUE_SIZE, 1,
					GLX_ALPHA_SIZE, 1,
					None };

static Bool WaitForNotify(Display *d, XEvent *e, char *arg) {
	    d = d;  /* this is to make the compiler warning shut up */
            return (e->type == MapNotify) && (e->xmap.window == (Window)arg);
}



/* Made the Display global so streamDecompress.c can handle events 
 * during it's main loop - eda */
Display *dpy = NULL;

static XVisualInfo *vi;
static Colormap cmap;
static XSetWindowAttributes swa;
static Window win;
static GLXContext cx;
static XEvent event;
static gfx_wid, gfx_ht;
static gfx_draw_wid;

static Atom  delWindowAtom;

/*
 * The following are specific to Octane and Impact gfx because 
 * GL_YCRCB_422_SGIX and interlace extension are not supported. 
 * For Impact Compression boards, if the display option is graphics,
 * the decoded data in RGBA packing format will be sent to Octane gfx.
 * In field mode, we use gfx texture to display the images if the system
 * has TRAMs (texture h/w memory). If there is no TRAMs, we use glPixelZoom 
 * in field mode which its performance will be slower than real time in 
 * 1 GE configuration. In frame mode, we directly draw the frames to the
 * gfx screen. The functions defined here that are only used for Impact 
 * Compression all start with "impactgfx_" in their function names.
 */
static impactgfx_tex_wid, impactgfx_tex_ht; /* texture size */
static impactgfx_subtex_wid, impactgfx_subtex_ht; /* sub-texture size */
static impactgfx_subtex_hoff;		/* horizontal offset for video load */
unsigned int texA,texB;			/* texture names */
float t0[2],t1[2],t2[2],t3[3];          /* texture coordinates */
float v0[2],v1[2],v2[2],v3[2];          /* vertice coordinates */
static has_tex_memory;

static void
glx_open(int width, int height)
{
	if ((dpy = XOpenDisplay(getenv("DISPLAY"))) == NULL) {
		fprintf(stderr, "XOpenDisplay failed\n");
		exit(1);
	}

	if (HASIMPACTCOMP && DO_FIELD) {
		if ((vi = glXChooseVisual(dpy, DefaultScreen(dpy), 
			impactgfx_attributeList)) == NULL) {
                	fprintf(stderr, "glXChooseVisual failed\n");
                	exit(1);
        	}
	}
	else {
		if ((vi = glXChooseVisual(dpy, DefaultScreen(dpy), 
			attributeList)) == NULL) {
			fprintf(stderr, "glXChooseVisual failed\n");
			exit(1);
		}
	}

	if ((cx = glXCreateContext(dpy, vi, 0, GL_TRUE)) == NULL) {
		fprintf(stderr, "glXCreateContext failed\n");
		exit(1);
	}

	cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
			   vi->visual, AllocNone);

	swa.colormap = cmap;
	swa.border_pixel = 0;
	swa.event_mask = StructureNotifyMask;
	win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0,0, width,height,
			0, vi->depth, InputOutput, vi->visual,
			CWBorderPixel|CWColormap|CWEventMask, &swa);



	XMapWindow(dpy, win);
	XIfEvent(dpy, &event, WaitForNotify, (char*)win);

	glXMakeCurrent(dpy, win, cx);




	/* Make sure that we get a chance to clean up after ourselves *
	 * when the window manager closes the window.  These next 2 lines
	 * make sure that we get a ClientMessage when the window manager
	 * wants to delete us
	 */
	XSelectInput(dpy, 
		     win, 
		     KeyPressMask | 
		     ButtonPressMask | 
		     ButtonReleaseMask);

	delWindowAtom = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &delWindowAtom, 1);

	/* check if Impact gfx has h/w texture memory */
        if (HASIMPACTCOMP && DO_FIELD) {
		char *s1, *s2;

		strtok((char *)glGetString(GL_RENDERER), "/");
		while ((s1 = strtok(NULL, "/")) != NULL)
			s2 = s1;

		/* The last token is the texture size */
		if (!strcmp(s2, "0"))
			has_tex_memory = 0;
		else
			has_tex_memory = 1;
	}
}

/* Create a texture and put its name into *texname. */
void
impactgfx_createTexture(unsigned int *texname)
{
        int i,texsize;
        unsigned char *texbuf;

        /* determine size of LOD 0 */
        texsize = impactgfx_tex_wid * impactgfx_tex_ht * 4;

        /* init texture buffer */
        texbuf = (unsigned char *)malloc(texsize);
        for(i = 0; i < texsize; i++) texbuf[i] = 0x00;

        /* generate texture name */
        glGenTexturesEXT(1,texname);

        /* bind to the texture */
        glBindTextureEXT(GL_TEXTURE_2D,*texname);

        /* set up filtering */
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

        /* create LOD 0 */
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,impactgfx_tex_wid,impactgfx_tex_ht,
                        0,GL_RGBA,GL_UNSIGNED_BYTE,texbuf);

        /* free up texture buffer */
        free(texbuf);
}

/* set up Octane Impact gfx environment */ 
void
impactgfx_setup(int wid, int ht)
{
    if (!has_tex_memory) {
	/* init viewport */
       	glViewport(0,0,wid,ht);
       	glOrtho((GLdouble)0, (GLdouble)wid, (GLdouble)0, (GLdouble)ht,
                                                (GLdouble)-1, (GLdouble)1);

	glPixelZoom(1, -2);
    }
    else {
	impactgfx_tex_wid 	= (wid/512+1)*512;
	impactgfx_tex_ht 	= (ht/512+1)*256;
	impactgfx_subtex_wid 	= wid;
	impactgfx_subtex_ht 	= ht/2;

	/*
	** there is a TRAM load restriction that forces us to load 
        ** our video field such that its right hand edge must be on
        ** a 32 texel boundary. We will set the horizontal offset
        ** for our sub-loads so that this restiction is satisfied 
	*/
	impactgfx_subtex_hoff	= ((impactgfx_subtex_wid & 0x1f) ?
				 (0x20 - (impactgfx_subtex_wid & 0x1f)) : 0x00);

	/* set texture environment */
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);

        /* setup for double buffering */
        glHint(GL_TEXTURE_MULTI_BUFFER_HINT_SGIX,GL_FASTEST);

        /* create texture A */
        impactgfx_createTexture(&texA);

        /* create texture B */
        impactgfx_createTexture(&texB);

        /* init viewport */
        glViewport(0,0,wid,ht);

        /* set up transformation matrix */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        /* set clear color */
        glClearColor(0.0,0.0,0.0,1.0);

        /* determine color of polygons */
        glColor4f(1.0, 1.0, 1.0, 1.0);

        /* enable texturing */
        glEnable(GL_TEXTURE_2D);

        /* init texture coordinates */
        t0[0] = (float)impactgfx_subtex_hoff / (float)impactgfx_tex_wid;
        t0[1] = 0.0;
        t1[0] = (float)(impactgfx_subtex_hoff + impactgfx_subtex_wid) / 
		(float)impactgfx_tex_wid;
        t1[1] = 0.0;
        t2[0] = (float)(impactgfx_subtex_hoff + impactgfx_subtex_wid) / 
		(float)impactgfx_tex_wid;
        t2[1] = (float)impactgfx_subtex_ht / (float)impactgfx_tex_ht;
        t3[0] = (float)impactgfx_subtex_hoff / (float)impactgfx_tex_wid;
        t3[1] = (float)impactgfx_subtex_ht / (float)impactgfx_tex_ht;

        /* init vertice coordinates */
        v0[0] = -1.0; v0[1] = -1.0;
        v1[0] =  1.0; v1[1] = -1.0;
        v2[0] =  1.0; v2[1] =  1.0;
        v3[0] = -1.0; v3[1] =  1.0;
    }
}

int 
gfx_open(int wid, int ht, int zoomx, int zoomy, int interlaced, char *title)
{
	title = title;  /* this is to make the compiler warning shut up */
	gfx_draw_wid = wid;

	gfx_wid = wid * zoomx;
	gfx_ht = ht * zoomy;

	/* OpenGL */
	glx_open(gfx_wid, gfx_ht);

        if (HASIMPACTCOMP && DO_FIELD)
               	impactgfx_setup(gfx_wid, gfx_ht);
	else {
		glViewport(0, 0, gfx_wid, gfx_ht);
		glOrtho((GLdouble)0, (GLdouble)gfx_wid, (GLdouble)0, 
			(GLdouble)gfx_ht, (GLdouble)-1, (GLdouble)1);
		/* video draws top to bottom */
		glPixelZoom(1.0 * (GLdouble)zoomx, -1.0 * (GLdouble)zoomy);

		if(interlaced)
	    		glEnable(GL_INTERLACE_SGIX);
	}
	return (gfx_ht -1); /* top line of window */
}

/* Start a video load into TRAM for the texture whose name is texname. */
void
impactgfx_loadField(unsigned int texname, char *cp)
{
        int val;

        /* bind to the texture */
        glBindTextureEXT(GL_TEXTURE_2D,texname);

        glTexSubImage2D(GL_TEXTURE_2D, 0, impactgfx_subtex_hoff, 0, 
		impactgfx_subtex_wid, impactgfx_subtex_ht, GL_RGBA, 
		GL_UNSIGNED_BYTE, cp);

        /* check for GL errors */
        while ((val = glGetError()) != GL_NO_ERROR) {
                fprintf(stderr,"******* GL ERROR ******* 0x%x\n",val);
                }
}

/* Draw a field for texture whose name is texname. */
void
impactgfx_drawField(unsigned int texname)
{
        /* clear the frame buffer */
        glClear(GL_COLOR_BUFFER_BIT);

        /* bind to the texture */
        glBindTextureEXT(GL_TEXTURE_2D,texname);

        /* draw the polygon */
        glBegin(GL_QUADS);
        glTexCoord2f(t0[0],t0[1]);
        glVertex2f(v3[0],v3[1]);
        glTexCoord2f(t1[0],t1[1]);
        glVertex2f(v2[0],v2[1]);
        glTexCoord2f(t2[0],t2[1]);
        glVertex2f(v1[0],v1[1]);
        glTexCoord2f(t3[0],t3[1]);
        glVertex2f(v0[0],v0[1]);
        glEnd();

}

void
impactgfx_show(int topline, int height, char *cp, int field_count)
{
    if (image.display == GRAPHICS) {
	if (DO_FRAME) {
		glRasterPos2i(0, topline);
		glDrawPixels(gfx_draw_wid, height, GL_RGBA,
					GL_UNSIGNED_BYTE, cp);
	}
        else if (!has_tex_memory) {
		glRasterPos2i(0, topline);
		glDrawPixels(gfx_draw_wid, height, GL_RGBA, 
					GL_UNSIGNED_BYTE, cp);
                /* swap the frame buffers */
                glXSwapBuffers(dpy,win);
	}
	else {
		if (field_count==1)
			/* load the first field */
                        impactgfx_loadField(texA, cp);
		else if (field_count&1) {
			/* draw the previous field and load the current field */
			impactgfx_drawField(texB);
			impactgfx_loadField(texA, cp);
		}
		else {
			/* draw the previous field and load the current field */
			impactgfx_drawField(texA);
			impactgfx_loadField(texB, cp);
		}

        	/* swap the frame buffers */
        	glXSwapBuffers(dpy,win);
	}
    }	
}

void
gfx_show(int topline, int height, char *cp, int field_count)
{
	if (HASIMPACTCOMP) {
		/* Octane or Impact gfx */
		impactgfx_show(topline, height, cp, field_count);
	}
	else {
		/* O2 gfx */
        	glRasterPos2i(0, topline);
        	glDrawPixels(gfx_draw_wid, height, GL_YCRCB_422_SGIX,
                                                        GL_UNSIGNED_BYTE, cp);
	}
}
