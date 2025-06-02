/*
 * nettotex.c
 *
 * Copyright (c) 1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee,
 * provided that (i) the above copyright notices and this permission
 * notice appear in all copies of the software and related documentation,
 * and (ii) the name of Silicon Graphics may not be used in any
 * advertising or publicity relating to the software without the specific,
 * prior written permission of Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SILICON GRAPHICS BE LIABLE FOR ANY SPECIAL,
 * INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * $Revision: 1.1 $
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <device.h>
#include <GL/glx.h>
#include "xwindow.h"
#include <X11/keysym.h>

#include <dmedia/dmedia.h>
#include <dmedia/dm_buffer.h>
#include <dmedia/dm_params.h>
#include <sys/dmcommon.h>
#include <dmedia/dmnet.h>
#include <dmnet/dmnet_striped_hippi.h>

#define XSIZE 1280
#define YSIZE 720
#define BYTES_PER_PIXEL 3

GLfloat texture_matrix[4][4] = {
    1, 0, 0, 0,
    0, -1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
};

char *_progName = 0;
int DEBUG=0;

int F1_is_first=0;   /* Which field is first (i.e., spacially higher) */

/* OpenGL/X variables */
Display *dpy;
Window window;
GLXContext ctx;
GLfloat zoom = 1;
/* Interlace extension is (partially or fully) implemented */
GLboolean hasInterlace;

int frames = 0;
int frameSize =0;
struct timeval startTime;

DMbuffer dmbuf;
DMbufferpool pool;
DMnetconnection cnp;
static char** HippiDevs=0;
static int NHippiDevs=0;

int xsize= XSIZE, ysize=YSIZE;
int bytesPerPixel=BYTES_PER_PIXEL;
int bFields=0;

/*
 * function prototypes
 */
void InitGfx(void);
void DisplayNewFrame(void);
void loop(void);
void cleanup(void);
void printStats(void);

#define USAGE \
"%s: [-s wxh] [-q <port>] [-H <dev#>] [-B <bytes/pix>]\n\
/t/t[-c <conntype>] [-N <plugin>] [-i] [-h]\n\
\n\
\t-s wxh		frame dimensions\n\
\t-i			NonInterlaced (fields instead of default frames\n\
\t-c conntype	DmNet connection type\n\
\t-N plugin		DmNet plugin tag\n\
\t-q port		Port number, default 5555\n\
\t-H dev#list	string of hippi dev #'s\n\
\t\t\t-H 38 stripes across /dev/hippi3 and /dev/hippi8\n\
\t-B bytes/pix	bytes per pixel. 3 (RGB) & 4 (RGBA) are only acceptable values\n\
\t-h			This message\n\
\nInteractively, `h/?' keys provide description of interface\n"

void 
err(char* str)
{
	fprintf(stderr, "%s: %s", _progName, str);
	exit(1);
}

void
usage(void)
{
	fprintf(stderr, USAGE, _progName);
}

int
main(int argc, char **argv)
{
    int		c, j;
    DMparams* plist;
	short port = 5555;
	int conntype=DMNET_TCP;
	char* pluginName=NULL;

    /* Parse the input string */
    while ((c = getopt(argc, argv, "s:ic:q:H:B:h")) != EOF ) {
        switch (c) {
          case 'q':
            port = atoi(optarg);
            break;

		  case 'i':
			bFields =1;
			break;

		  case 'c':
			conntype = atoi(optarg);
			break;

		  case 'N':
			pluginName = optarg;
			if(!pluginName) {
				fprintf(stderr, "Must supply plugin tag with -N\n");
				usage(); exit(1);
			}
			break;

		  case 'B':
			bytesPerPixel = atoi(optarg);
			if ((bytesPerPixel != 4) && (bytesPerPixel != 3))
				fprintf(stderr, "Bytes Per Pixel cannot be %d, using default %d\n",
					bytesPerPixel, bytesPerPixel = BYTES_PER_PIXEL);
			break;

		  case 'H':
			conntype = DMNET_STRIPED_HIPPI;
			NHippiDevs = strlen(optarg);
			HippiDevs = (char**)malloc(NHippiDevs*sizeof(char*));
			for(j=0; j<NHippiDevs; j++){
				char* devname="/dev/hippi";
				int n;
				n = strlen(devname);
				HippiDevs[j] = (char*)malloc(n+2);
				sprintf(HippiDevs[j], "%s%c", devname, optarg[j]);
				/* fprintf(stderr, "%s, strlen=%d\n", HippiDevs[j], n); */
			}
			break;
		
		  case 's':
			{
				int len, xlen, ylen;
				char *numbuf;
				char *xloc;

				len = strlen(optarg);
				xloc = strchr(optarg, 'x');
				if (!xloc) {
					fprintf(stderr, "Error: invalid geometry format, using default size\n");
					break;
				}
				xlen = len - strlen(xloc);
				if (xlen < 1 || xlen > 4)
				{
					fprintf(stderr, "Error: invalid x size, using default size\n");
					break;
				}
				numbuf = strtok(optarg,"x");
				xsize = atoi(numbuf);
				if ( xsize < 0 ) {
					fprintf (stderr, "Error: negative w size not allowed, using default w and h sizes\n");
					xsize = 0;  /* use default size */
					break;
				}

				ylen = len - xlen -1;
				if (ylen < 1 || ylen > 4)
				{
					fprintf(stderr, "Error: invalid y size, using default size\n");
					break;
				}
				numbuf = strtok(NULL,"");
				ysize = atoi(numbuf);
				if ( ysize < 0 ) {
					fprintf (stderr, "Error: negative h size not allowed, using default w and h sizes\n");
					xsize = 0;  /* use default size */
					break;
				}
				break;
			}
		  case 'h':
			usage();
			exit(0);
          default:
            usage();
			exit(EXIT_FAILURE);
        }
    }

	_progName = argv[0];
	DEBUG = ( int ) getenv( "DEBUG_NETTOVID" );

	if(dmNetOpen(&cnp) != DM_SUCCESS)
		err("dmNetOpen failed\n");
		
	if(dmParamsCreate(&plist) == DM_FAILURE)
		err("Create params failed\n");

	if (pluginName) {
		conntype = DMNET_PLUGIN;
		if (dmParamsSetString(plist, DMNET_PLUGIN_NAME, pluginName)
				== DM_FAILURE)
			err("Couldn't set DMNET_PLUGIN_NAME");
	}

	if(dmParamsSetInt(plist, DMNET_CONNECTION_TYPE, conntype)
		== DM_FAILURE)
		err("Couldn't set connection type\n");

	if (conntype == DMNET_STRIPED_HIPPI){
		register int i;
		char paramName[128];

		if(dmParamsSetInt(plist, DMNET_HIPPI_NDEV, NHippiDevs)==DM_FAILURE)
			err("Couldn't set DMNET_HIPPI_NDEV\n");

		for (i=0; i<NHippiDevs; i++) {
			sprintf(paramName, DMNET_HIPPI_DEV, i);
			if(dmParamsSetString(plist, paramName, HippiDevs[i]) == DM_FAILURE)
				err("Couldn't set DMNET_HIPPI_DEV\n");
		}
	}

	if(dmParamsSetInt(plist, DMNET_PORT, port) == DM_FAILURE)
		err("Couldn't set port number\n");

	if (dmNetListen(cnp, plist) != DM_SUCCESS)
		err("dmNetListen failed\n");

	if(dmNetAccept(cnp, plist) != DM_SUCCESS)
		err("dmNetAccept failed\n");
	if (DEBUG)
		fprintf(stderr, "%s: accept\n", _progName);


    frameSize = bytesPerPixel*xsize*ysize/(bFields+1);
    fprintf(stderr,
		"xsize : %d, ysize : %d, bytesPerPixel : %d, frameSize : %d\n",
		xsize, ysize, bytesPerPixel, frameSize);

    dmBufferSetPoolDefaults(plist, 2, frameSize, DM_TRUE, DM_FALSE);
	if(dmNetGetParams(cnp, plist) == DM_FAILURE)
		err("dmNetGetParams failed\n");

    if (dmBufferCreatePool(plist, &pool) != DM_SUCCESS)
        err("Create Pool failed\n");

    dmParamsDestroy(plist);

	if (dmNetRegisterPool(cnp, pool) != DM_SUCCESS)
		err("dmNetRegisterPool failed\n");

    InitGfx();

    loop();
    /*NOTREACHED*/
}

void
loop(void)
{
    XEvent event;
    KeySym key;

	gettimeofday(&startTime, 0);
    for(;;) {
        while(XPending(dpy)) {
            XNextEvent(dpy, &event);
            switch(event.type) {
              case KeyPress:
                XLookupString(&event.xkey, NULL, 0, &key, NULL);
                switch (key) {
                  case XK_Escape:
                    exit(EXIT_SUCCESS);
		    /*NOTREACHED*/
                  case XK_z:
                    zoom = 3 - zoom;		       /* 2 -> 1, 1 -> 2 */
		    XMoveResizeWindow(dpy, window, 50, 0,
				      zoom * xsize,
				      zoom * ysize);
                    glXWaitX();
		    glMatrixMode(GL_PROJECTION);
		    glLoadIdentity();
		    glViewport(0, 0, zoom * xsize - 1,
                               zoom * ysize - 1);
		    glOrtho(0, zoom * xsize - 1,
                            0, zoom * ysize - 1, -1, 1);
                    break;
                  case XK_question:
                  case XK_h:
                    printf("Keys:\tz - toggle zoom (of 2)\n");
                    printf("\th/? - print help\n");
                    printf("\tEsc - exit\n");
                }
		break;
            }
        }

		if(dmNetRecv(cnp, &dmbuf) == DM_SUCCESS) {
			if(DEBUG)
				fprintf(stderr, "%s: received frame %d\n", _progName, frames+1);
			/* frames++; */
	    	DisplayNewFrame();
	    	dmBufferFree(dmbuf);
		} else {
			cleanup();
    	}
	}
}

/*
 * CreateTexture: Create a texture map with correct attributes.
 */

float   s_scale, t_scale;

void
CreateTexture(void)
{
/*    float   s_scale, t_scale;*/
    float vheight = ysize;
    int t_width, t_height;

    /*
     * texture dimensions must be powers of two.  Find the smallest powers
     * of two that are greater than or equal to image width and height.
     */
    t_width = 1;
    while (t_width < xsize) {
	t_width <<= 1;
    }
    t_height = 1;
    while (t_height < vheight) {
	t_height <<= 1;
    }

    s_scale = (float)xsize / (float)t_width;
    t_scale = vheight / (float)t_height;

    glEnable(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_EXT, t_width, t_height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, 0);

#if 1
    texture_matrix[0][0] =  s_scale;
    texture_matrix[1][1] = -t_scale;
    texture_matrix[3][1] =  t_scale;
#else
    texture_matrix[1][1] = -1;
#endif
}


/*
 * Open an X window of appropriate size and create context.
 */
void
InitGfx(void)
{
    XSizeHints hints;
    int visualAttr[] = {GLX_RGBA, GLX_DOUBLEBUFFER, 
				GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
			    GLX_DEPTH_SIZE, 0, GLX_STENCIL_SIZE, 0,
			    None};
    const char *extensions;

    hints.x = 50; hints.y = 0;
    hints.min_aspect.x = hints.max_aspect.x = xsize;
    hints.min_aspect.y = hints.max_aspect.y = ysize;
    hints.min_width = xsize;
    hints.max_width = 4 * xsize;
    hints.base_width = hints.width = xsize;
    hints.min_height = ysize;
    hints.max_height = 4 * ysize;
    hints.base_height = hints.height = ysize;
    hints.flags = USSize | PAspect | USPosition | PMinSize | PMaxSize;
    createWindowAndContext(&dpy, &window, &ctx, 50, 0, xsize,
                           ysize, GL_FALSE, &hints, visualAttr,
			   _progName);
    extensions = glXQueryExtensionsString(dpy, DefaultScreen(dpy));
    if(extensions == NULL){
	printf("Warning : no extensions found on OpenGL device\n");
    }

    glMatrixMode(GL_TEXTURE);
    glLoadMatrixf((GLfloat *) texture_matrix);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glViewport(0, 0, zoom * xsize - 1, zoom * ysize - 1);
    glOrtho(0, zoom * xsize - 1, 0, zoom * ysize - 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -0.5);
    glDisable(GL_DITHER);

    CreateTexture();

    /*
     * Check for interlace extension.
     */
    hasInterlace =
        strstr((const char *) extensions, "GL_SGIX_interlace") != NULL;
    if (hasInterlace)
	printf("interlace extension advertised \n");

    if (!hasInterlace) {
        /*
         * HACK - Interlace is not formally advertised.  Try getting it anyhow.
         */
        glDisable(GL_INTERLACE_SGIX);
        hasInterlace = glGetError() == GL_NO_ERROR;
        if (hasInterlace)
            fprintf(stderr,
		    "Assuming SGIX_Interlace supported but not advertised\n");
    }
    printf("interlace extension: %d\n",hasInterlace);
}

void
DisplayNewFrame(void)
{
    char *dataPtr;
    USTMSCpair pair;

	frames++;
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_TEXTURE);
    glLoadMatrixf((GLfloat*) texture_matrix);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -0.5);

    if ((dataPtr = dmBufferMapData(dmbuf)) <= 0) {
	printf("dmBufferMapData failed %s\n", dmGetError(0,0));
	cleanup();
    }
    pair = dmBufferGetUSTMSCpair(dmbuf);

    if (bFields) glEnable(GL_INTERLACE_SGIX);
    glTexSubImage2DEXT(GL_TEXTURE_2D, 0,
		       0, bFields ? 1 - F1_is_first ^ (pair.msc & 1) : 0,
		       xsize, ysize/(bFields+1),
		       (bytesPerPixel == 3)?GL_RGB:GL_RGBA, GL_UNSIGNED_BYTE,
		       dataPtr);
    if (bFields) glDisable(GL_INTERLACE_SGIX);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(0, 0);
    glTexCoord2f(1, 0); glVertex2f(xsize*zoom, 0);
    glTexCoord2f(1, 1); glVertex2f(xsize*zoom, zoom * ysize);
    glTexCoord2f(0, 1); glVertex2f(0, zoom * ysize);
    glEnd();

    glXSwapBuffers(dpy, window);
}

void
cleanup(void)
{
	printStats();
	dmNetClose(cnp);
    dmBufferDestroyPool(pool);
    exit(EXIT_SUCCESS);
}

void
printStats(void)
{
	struct timeval stopTime;
	long sec, usec;
	double diff;
	
	gettimeofday(&stopTime, 0);
	sec = stopTime.tv_sec - startTime.tv_sec;
	usec = stopTime.tv_usec - startTime.tv_usec;
	diff = sec + usec/1000000.0;

	printf("\n%d frames received in %.3f real seconds, %.3f KB/sec\n",
		frames, diff, frameSize/(diff*1000)*frames);
}
