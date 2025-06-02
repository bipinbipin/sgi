/*
 * vidtotex.c
 *
 * This VL program demonstrates the Impact Video board video->texture ability.
 * It must be run on an Impact Graphics system with a TRAM option card 
 * installed. 
 * 	       								
 * usage: vidtotex [-v <input>] 				
 *		-v #	video input number (as reported by vlinfo)
 * 
 * If no video input number is specified, the default video source node is
 * used. The default video source node is assumed to be a single link node.
 *
 * This program was compiled and linked with the following command:
 * 		cc vidtotex.c -lvl -lGL -lX11 -o vidtotex
 *								
 * Copyright 1996 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <stdlib.h>
#include <stdio.h>
#include <vl/vl.h>
#include <vl/dev_mgv.h>
#include <GL/glx.h>

/* VL defines */
VLServer svr;				/* VL server */
VLPath path;				/* VL path */
VLNode src,drn,dev;			/* VL nodes */
int vid_width,vid_height;		/* video size */
int vid_node;				/* video node number */

/* X window defines */
Display *dpy;				/* X window display */
Window window;				/* X window window */
GLXContext context;			/* X window context */
GLXVideoSourceSGIX glxvidsrc;		/* GLX handle for video stream */

/* texture defines */
int tex_format;				/* texture format */
int tex_width,tex_height;		/* texture size */
int tex_hoff;				/* horizontal offset for video load */
unsigned int texA,texB;			/* texture names */
float t0[2],t1[2],t2[2],t3[3];		/* texture coordinates */
float v0[2],v1[2],v2[2],v3[2];		/* vertice coordinates */

/* function prototypes */
void init(int argc, char **argv);
void initVL(void);
void initGL(void);
void createTexture(unsigned int *texname);
int errorHandlerForX(Display *dpy);
void openWindow(void);
void loadField(unsigned int texname);
void drawField(unsigned int texname);
int ESCKeyPress(void);
void processEvents(void);
void usage(int argc, char ** argv);
void cleanup(void);


/* Main Program. */
main(int argc, char **argv)
{
	/* initialize */
	init(argc,argv);

	/* load A */
    	loadField(texA);

	/* load B */
	loadField(texB);	

	/* loop and load/draw */
	while(1) {
		/* check for ESC key press by user */
		if (ESCKeyPress()) break;

		/* process VL events */
		processEvents();
		
		/* draw A */
		drawField(texA);

		/* load A */
		loadField(texA);

		/* draw B */
		drawField(texB);

		/* load B */
		loadField(texB);
		}

	/* all done */
	cleanup();
}

/* Initialize. */
void
init(int argc, char **argv)
{
	int c;

	/* initialize video node number */
	vid_node = VL_ANY;

	/* parse the input string */
	while((c = getopt(argc,argv,"v:")) != EOF) {
		switch(c) {
			case 'v':
				vid_node = atoi(optarg);
				break;
			default:
				usage(argc,argv);
				break;
			}
		}

	/* initialize VL */
	initVL();

	/* open X window */
	openWindow();

	/* initalize GL */
	initGL();
}

/* Initialize VL. */
void
initVL()
{
	int tex_node;
	VLControlValue ctlval;

	/* determine texture node number */
	if (vid_node == VL_MGV_NODE_NUMBER_VIDEO_DL)
		tex_node = VL_MGV_NODE_NUMBER_TEXTURE_DL;
	else tex_node = VL_MGV_NODE_NUMBER_TEXTURE;

	/* open video server */
	if (!(svr = vlOpenVideo(""))) {
		vlPerror("vlOpenVideo");
		exit(1);
		}

	/* get video source */
	if ((src = vlGetNode(svr,VL_SRC,VL_VIDEO,vid_node)) < 0) {
        	vlPerror("vlGetNode SRC");
		exit(1);
    		}

	/* get texture drain */
	if ((drn = vlGetNode(svr,VL_DRN,VL_TEXTURE,tex_node)) < 0) {
        	vlPerror("vlGetNode DRN");
		exit(1);
    		}

	/* get device */
    	if ((dev = vlGetNode(svr,VL_DEVICE,0,VL_ANY)) < 0) {
        	vlPerror("vlGetNode DEVICE");
		exit(1);
    		}

	/* create the path */
	if ((path = vlCreatePath(svr,VL_ANY,src,drn)) < 0) {
		vlPerror("vlCreatePath");
		exit(1);
		}

        /* add device node to path */
        if (vlAddNode(svr,path,dev) < 0) {
                vlPerror("vlAddNode");
                exit(1);
                }

	/* select events */
	if (vlSelectEvents(svr,path,VLStreamPreemptedMask) < 0) {
		vlPerror("vlSelectEvents");
		exit(1);
		}	

	/* setup the path */
	if (vlSetupPaths(svr,(VLPathList)&path,1,VL_SHARE,VL_SHARE) < 0) {
		vlPerror("vlSetupPaths");
		exit(1);
		}

        /* get timing */
        if (vlGetControl(svr,path,dev,VL_TIMING,&ctlval) < 0) {
                vlPerror("vlGetControl: VL_TIMING");
                exit(1);
                }

	/* determine video size */
       	if (ctlval.intVal == VL_TIMING_525_SQ_PIX) {
                vid_width = 640;  vid_height = 243;
		tex_width = 1024; tex_height = 256;
                }
        else if (ctlval.intVal == VL_TIMING_525_CCIR601) {
                vid_width = 720;  vid_height = 243;
		tex_width = 1024; tex_height = 256;
                }
        else if (ctlval.intVal == VL_TIMING_625_SQ_PIX) {
                vid_width = 768;  vid_height = 288;
		tex_width = 1024; tex_height = 512;
                }
        else {
                vid_width = 720;  vid_height = 288;
		tex_width = 1024; tex_height = 512;
                }

	/* there is a TRAM load restriction that forces us to load */
	/* our video field such that its right hand edge must be on */
	/* a 32 texel boundary. We will set the horizontal offset */
	/* for our sub-loads so that this restiction is satisfied */
	tex_hoff = ((vid_width & 0x1f) ? (0x20 - (vid_width & 0x1f)) : 0x00);

	/* begin the video transfer */
	if (vlBeginTransfer(svr,path,0,NULL)) {
		vlPerror("vlBeginTransfer");
		exit(1);
		}
}

/* Initialize GL. */
void
initGL()
{
	/* initialize texture format */
	if (vid_node == VL_MGV_NODE_NUMBER_VIDEO_DL) 
		tex_format = GL_RGBA;
	else tex_format = GL_RGB; 

	/* set texture environment */
  	glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);

	/* setup for double buffering */
	glHint(GL_TEXTURE_MULTI_BUFFER_HINT_SGIX,GL_FASTEST);

	/* create texture A */
	createTexture(&texA);

	/* create texture B */
	createTexture(&texB);

	/* init viewport */
	glViewport(0,0,vid_width,vid_height * 2);

	/* set up transformation matrix */
    	glMatrixMode(GL_PROJECTION);
    	glLoadIdentity();
    	glMatrixMode(GL_MODELVIEW);
    	glLoadIdentity();

	/* set blending function */
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	/* enable blending for RGBA texture format */
	if (tex_format == GL_RGBA) glEnable(GL_BLEND);

	/* set clear color */
	glClearColor(0.0,0.0,0.0,1.0);

	/* determine color of polygons */
	glColor4f(1.0, 1.0, 1.0, 1.0);

	/* enable texturing */
	glEnable(GL_TEXTURE_2D);

	/* create read drawable for video */
	glxvidsrc = glXCreateGLXVideoSourceSGIX(dpy,DefaultScreen(dpy),svr,
						path,VL_TEXTURE,drn);

	/* make video stream the read drawable */
	glXMakeCurrentReadSGI(dpy,window,glxvidsrc,context);

	/* init texture coordinates */
	t0[0] = (float)tex_hoff / (float)tex_width; 
	t0[1] = 0.0;
	t1[0] = (float)(tex_hoff + vid_width) / (float)tex_width;
	t1[1] = 0.0;
	t2[0] = (float)(tex_hoff + vid_width) / (float)tex_width;
	t2[1] = (float)vid_height / (float)tex_height;
	t3[0] = (float)tex_hoff / (float)tex_width; 
	t3[1] = (float)vid_height / (float)tex_height;

	/* init vertice coordinates */
	v0[0] = -1.0; v0[1] = -1.0;
	v1[0] =  1.0; v1[1] = -1.0;
	v2[0] =  1.0; v2[1] =  1.0;
	v3[0] = -1.0; v3[1] =  1.0;
}

/* Create a texture and put its name into *texname. */
void
createTexture(unsigned int *texname)
{
	int i,texsize;
	unsigned char *texbuf;

	/* determine size of LOD 0 */
	texsize = tex_width * tex_height * (tex_format == GL_RGBA ? 4 : 3);

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
	glTexImage2D(GL_TEXTURE_2D,0,tex_format,tex_width,tex_height,
			0,tex_format,GL_UNSIGNED_BYTE,texbuf);

	/* free up texture buffer */
	free(texbuf);
}

/* Wait for notification for X window. */
static Bool
WaitForNotify(Display *d, XEvent *e, char *arg) {
    	return (e->type == MapNotify) && (e->xmap.window == (Window)arg);
}

/* X window error handler. */
int
errorHandlerForX(Display *dpy)
{
}

/* Open X window. */
void
openWindow() {

    	int vinfo[] = {
        	GLX_RGBA,
		GLX_DOUBLEBUFFER,
        	GLX_RED_SIZE, 1,
        	GLX_GREEN_SIZE, 1,
        	GLX_BLUE_SIZE, 1,
		GLX_ALPHA_SIZE, 1,
        	None
    		};
    	XVisualInfo *v;
    	XSetWindowAttributes wa;
    	XEvent event;
    	XSizeHints sh;

    	if ((dpy = XOpenDisplay(0)) == 0) {
        	fprintf(stderr, "XOpenDisplay failed\n");
        	exit(EXIT_FAILURE);
    		}

    	if ((v = glXChooseVisual(dpy, DefaultScreen(dpy), vinfo)) == 0) {
        	fprintf(stderr, "glXChooseVisual failed\n");
        	exit(EXIT_FAILURE);
    		}

    	if ((context = glXCreateContext(dpy, v, 0, GL_TRUE)) == 0) {
        	fprintf(stderr, "glXCreateContext failed\n");
        	exit(EXIT_FAILURE);
    		}

    	wa.colormap = XCreateColormap(dpy, RootWindow(dpy,v->screen), 
				v->visual, AllocNone);
    	wa.border_pixel = 0;
    	wa.event_mask = StructureNotifyMask | KeyPressMask; 
    	window = XCreateWindow(dpy, RootWindow(dpy, v->screen), 0, 0, 
                      		vid_width, vid_height * 2, 0, v->depth, 
				InputOutput, v->visual,
                      		CWBorderPixel | CWEventMask | CWColormap, &wa);
    	sh.flags = USPosition | USSize;
    	XSetWMProperties(dpy, window, 0, 0, 0, 0, &sh, 0, 0);
    	XMapWindow(dpy, window);
    	XIfEvent(dpy, &event, WaitForNotify, (char *)window);

	XSetIOErrorHandler(errorHandlerForX);

    	if (!glXMakeCurrent(dpy, window, context)) {
        	fprintf(stderr, "glXMakeCurrent failed\n");
        	exit(EXIT_FAILURE);
    		}
}

/* Start a video load into TRAM for the texture whose name is texname. */
void 
loadField(unsigned int texname)
{
	int val;

	/* bind to the texture */
	glBindTextureEXT(GL_TEXTURE_2D,texname);

	/* copy the video sub-image into the texture */
	glCopyTexSubImage2DEXT(GL_TEXTURE_2D,0,tex_hoff,0,0,0,
				vid_width,vid_height);

	/* check for GL errors */
        while ((val = glGetError()) != GL_NO_ERROR) {
                fprintf(stderr,"******* GL ERROR ******* 0x%x\n",val);
		}
}

/* Draw a field for texture whose name is texname. */
void
drawField(unsigned int texname)
{
	/* clear the frame buffer */
	glClear(GL_COLOR_BUFFER_BIT);

	/* bind to the texture */
	glBindTextureEXT(GL_TEXTURE_2D,texname);

	/* draw the polygon */
        glBegin(GL_QUADS); 
        glTexCoord2f(t0[0],t0[1]);
        glVertex2f(v0[0],v0[1]);
        glTexCoord2f(t1[0],t1[1]);
        glVertex2f(v1[0],v1[1]);
        glTexCoord2f(t2[0],t2[1]);
        glVertex2f(v2[0],v2[1]);
        glTexCoord2f(t3[0],t3[1]);
        glVertex2f(v3[0],v3[1]);
        glEnd();

	/* swap the frame buffers */
	glXSwapBuffers(dpy,window);
}

/* Check for ESC key press by user. */
int
ESCKeyPress()
{
	XEvent myevent;

	/* check for keypress */
        if (XCheckWindowEvent(dpy,window,0xffffff,&myevent)) {
		if (myevent.type == KeyPress) 
			return(myevent.xkey.keycode == 16);
		else return(0);
		}	
	else return(0);
}

/* Process events from VL. */
void
processEvents()
{
	VLEvent ev;

	/* check to see if the video path has been pre-empted */
	if (vlCheckEvent(svr,VLStreamPreemptedMask,&ev) != -1) {
		cleanup();
		exit(0);
		}
}

/* Show command line syntax. */
void
usage(int argc, char ** argv)
{
	fprintf(stderr,"Usage: %s: [-v <input>]\n",argv[0]);
 	fprintf(stderr,"\t-v#    video input number (as reported by vlinfo)\n");
	exit(1);
}

/* cleanup routine. */
void
cleanup()
{
	/* tear down VL setup */
	vlEndTransfer(svr,path);
	vlDestroyPath(svr,path);
	vlCloseVideo(svr);

	/* tear down GL setup */
	glDeleteTexturesEXT(1,&texA);
	glDeleteTexturesEXT(1,&texB);

	/* destroy GLX handle for video input stream */
	glXDestroyGLXVideoSourceSGIX(dpy,glxvidsrc);
}
