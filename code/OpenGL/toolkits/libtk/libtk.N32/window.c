#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tk.h"
#include "private.h"

/******************************************************************************/

Display *xDisplay = 0;
int xScreen = 0; 
Window wRoot = 0;
Atom deleteWindowAtom;
WINDOW_REC w = {
    0, 0, 300, 300, TK_RGB|TK_SINGLE|TK_DIRECT, TK_MINIMUM_CRITERIA
};
float colorMaps[] = {
    0.000000, 1.000000, 0.000000, 1.000000, 0.000000, 1.000000, 
    0.000000, 1.000000, 0.333333, 0.776471, 0.443137, 0.556863, 
    0.443137, 0.556863, 0.219608, 0.666667, 0.666667, 0.333333, 
    0.666667, 0.333333, 0.666667, 0.333333, 0.666667, 0.333333, 
    0.666667, 0.333333, 0.666667, 0.333333, 0.666667, 0.333333, 
    0.666667, 0.333333, 0.039216, 0.078431, 0.117647, 0.156863, 
    0.200000, 0.239216, 0.278431, 0.317647, 0.356863, 0.400000, 
    0.439216, 0.478431, 0.517647, 0.556863, 0.600000, 0.639216, 
    0.678431, 0.717647, 0.756863, 0.800000, 0.839216, 0.878431, 
    0.917647, 0.956863, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.749020, 0.749020, 0.749020, 0.749020, 
    0.749020, 0.749020, 0.749020, 0.749020, 1.000000, 1.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 
    0.749020, 0.749020, 1.000000, 1.000000, 1.000000, 1.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.749020, 0.749020, 
    0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 
    1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 
    1.000000, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.749020, 0.749020, 0.749020, 0.749020, 
    0.749020, 0.749020, 0.749020, 0.749020, 1.000000, 1.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 
    0.749020, 0.749020, 1.000000, 1.000000, 1.000000, 1.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 0.000000, 0.000000, 
    1.000000, 1.000000, 0.000000, 0.000000, 1.000000, 1.000000, 
    0.333333, 0.443137, 0.776471, 0.556863, 0.443137, 0.219608, 
    0.556863, 0.666667, 0.666667, 0.333333, 0.666667, 0.333333, 
    0.666667, 0.333333, 0.666667, 0.333333, 0.666667, 0.333333, 
    0.666667, 0.333333, 0.666667, 0.333333, 0.666667, 0.333333, 
    0.039216, 0.078431, 0.117647, 0.156863, 0.200000, 0.239216, 
    0.278431, 0.317647, 0.356863, 0.400000, 0.439216, 0.478431, 
    0.517647, 0.556863, 0.600000, 0.639216, 0.678431, 0.717647, 
    0.756863, 0.800000, 0.839216, 0.878431, 0.917647, 0.956863, 
    0.000000, 0.141176, 0.282353, 0.427451, 0.568627, 0.713726, 
    0.854902, 1.000000, 0.000000, 0.141176, 0.282353, 0.427451, 
    0.568627, 0.713726, 0.854902, 1.000000, 0.000000, 0.141176, 
    0.282353, 0.427451, 0.568627, 0.713726, 0.854902, 1.000000, 
    0.000000, 0.141176, 0.282353, 0.427451, 0.568627, 0.713726, 
    0.854902, 1.000000, 0.000000, 0.141176, 0.282353, 0.427451, 
    0.568627, 0.713726, 0.854902, 1.000000, 0.000000, 0.141176, 
    0.282353, 0.427451, 0.568627, 0.713726, 0.854902, 1.000000, 
    0.000000, 0.141176, 0.282353, 0.427451, 0.568627, 0.713726, 
    0.854902, 1.000000, 0.000000, 0.141176, 0.282353, 0.427451, 
    0.568627, 0.713726, 0.854902, 1.000000, 0.000000, 0.141176, 
    0.282353, 0.427451, 0.568627, 0.713726, 0.854902, 1.000000, 
    0.000000, 0.141176, 0.282353, 0.427451, 0.568627, 0.713726, 
    0.854902, 1.000000, 0.000000, 0.141176, 0.282353, 0.427451, 
    0.568627, 0.713726, 0.854902, 1.000000, 0.000000, 0.141176, 
    0.282353, 0.427451, 0.568627, 0.713726, 0.854902, 1.000000, 
    0.000000, 0.141176, 0.282353, 0.427451, 0.568627, 0.713726, 
    0.854902, 1.000000, 0.000000, 0.141176, 0.282353, 0.427451, 
    0.568627, 0.713726, 0.854902, 1.000000, 0.000000, 0.141176, 
    0.282353, 0.427451, 0.568627, 0.713726, 0.854902, 1.000000, 
    0.000000, 0.141176, 0.282353, 0.427451, 0.568627, 0.713726, 
    0.854902, 1.000000, 0.000000, 0.141176, 0.282353, 0.427451, 
    0.568627, 0.713726, 0.854902, 1.000000, 0.000000, 0.141176, 
    0.282353, 0.427451, 0.568627, 0.713726, 0.854902, 1.000000, 
    0.000000, 0.141176, 0.282353, 0.427451, 0.568627, 0.713726, 
    0.854902, 1.000000, 0.000000, 0.141176, 0.282353, 0.427451, 
    0.568627, 0.713726, 0.854902, 1.000000, 0.000000, 0.141176, 
    0.282353, 0.427451, 0.568627, 0.713726, 0.854902, 1.000000, 
    0.000000, 0.141176, 0.282353, 0.427451, 0.568627, 0.713726, 
    0.854902, 1.000000, 0.000000, 0.141176, 0.282353, 0.427451, 
    0.568627, 0.713726, 0.854902, 1.000000, 0.000000, 0.141176, 
    0.282353, 0.427451, 0.568627, 0.713726, 0.854902, 1.000000, 
    0.000000, 0.141176, 0.282353, 0.427451, 0.568627, 0.713726, 
    0.854902, 1.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 0.333333, 0.443137, 
    0.443137, 0.219608, 0.776471, 0.556863, 0.556863, 0.666667, 
    0.666667, 0.333333, 0.666667, 0.333333, 0.666667, 0.333333, 
    0.666667, 0.333333, 0.666667, 0.333333, 0.666667, 0.333333, 
    0.666667, 0.333333, 0.666667, 0.333333, 0.039216, 0.078431, 
    0.117647, 0.156863, 0.200000, 0.239216, 0.278431, 0.317647, 
    0.356863, 0.400000, 0.439216, 0.478431, 0.517647, 0.556863, 
    0.600000, 0.639216, 0.678431, 0.717647, 0.756863, 0.800000, 
    0.839216, 0.878431, 0.917647, 0.956863, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 
    0.000000, 0.000000, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 0.247059, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 0.498039, 
    0.498039, 0.498039, 0.498039, 0.498039, 0.749020, 0.749020, 
    0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 
    0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 
    0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 
    0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 
    0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 
    0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 0.749020, 
    0.749020, 0.749020, 1.000000, 1.000000, 1.000000, 1.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 
    1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 1.000000, 
};
float tkRGBMap[8][3] = {
    {
	0, 0, 0
    },
    {
	1, 0, 0
    },
    {
	0, 1, 0
    },
    {
	1, 1, 0
    },
    {
	0, 0, 1
    },
    {
	1, 0, 1
    },
    {
	0, 1, 1
    },
    {
	1, 1, 1
    }
};

/******************************************************************************/

void tkCloseWindow(void)
{

    if (xDisplay) {
	cursorNum = 0;

	ExposeFunc = 0;
	ReshapeFunc = 0;
	IdleFunc = 0;
	DisplayFunc = 0;
	KeyDownFunc = 0;
	MouseDownFunc = 0;
	MouseUpFunc = 0;
	MouseMoveFunc = 0;

	glFlush();
	glFinish();
	if (TK_HAS_OVERLAY(w.type)) {
	    XDestroyWindow(xDisplay, w.wOverlay);
	    glXDestroyContext(xDisplay, w.cOverlay);
	    XFreeColormap(xDisplay, w.cMapOverlay);
	    XFree((char *)w.vInfoOverlay);
	}
	XDestroyWindow(xDisplay, w.wMain);
	glXDestroyContext(xDisplay, w.cMain);
	XFreeColormap(xDisplay, w.cMapMain);
	XFree((char *)w.vInfoMain);
	XCloseDisplay(xDisplay);
	xDisplay = 0;
    }
}

/******************************************************************************/

static int ErrorHandler(Display *xDisplay, XErrorEvent *event)
{
    char buf[80];

    printf("\nReceived X error!\n");
    printf("\tError code   : %d\n", event->error_code);
    printf("\tRequest code : %d\n", event->request_code);
    printf("\tMinor code   : %d\n\n", event->minor_code);
    XGetErrorText(xDisplay, event->error_code, buf, 80);
    printf("\tError text : '%s'\n\n", buf);
    return 0;
}

/******************************************************************************/

void tkInitDisplayMode(GLenum type)
{

    w.type = type;
}

/******************************************************************************/

void tkInitDisplayModePolicy(GLenum type)
{

    w.dmPolicy = type;
}

/******************************************************************************/

GLenum tkInitDisplayModeID(GLint id)
{
    XVisualInfo sampleVis;
    int nvis;

    if (!xDisplay && !tkInitDisplay()) return(GL_FALSE);

    sampleVis.visualid = id;
    w.vInfoMain = XGetVisualInfo(xDisplay, VisualIDMask, &sampleVis, &nvis);

    if (w.vInfoMain) 
        return GL_TRUE;
    else
        return GL_FALSE;
}

/******************************************************************************/

void tkInitPosition(int x, int y, int width, int height)
{

    w.x = x;
    w.y = y;
    w.w = width;
    w.h = height;
}

/******************************************************************************/

static XVisualInfo *FindBestMainVisual(GLenum type)
{
    int list[32], i;

    i = 0;

    list[i++] = GLX_LEVEL;
    list[i++] = 0;

    if (TK_IS_DOUBLE(type)) {
	list[i++] = GLX_DOUBLEBUFFER;
    }

    if (TK_IS_RGB(type)) {
	list[i++] = GLX_RGBA;
	list[i++] = GLX_RED_SIZE;
	list[i++] = 1;
	list[i++] = GLX_GREEN_SIZE;
	list[i++] = 1;
	list[i++] = GLX_BLUE_SIZE;
	list[i++] = 1;
	if (TK_HAS_ALPHA(type)) {
	    list[i++] = GLX_ALPHA_SIZE;
	    list[i++] = 1;
	}
	if (TK_HAS_ACCUM(type)) {
	    list[i++] = GLX_ACCUM_RED_SIZE;
	    list[i++] = 1;
	    list[i++] = GLX_ACCUM_GREEN_SIZE;
	    list[i++] = 1;
	    list[i++] = GLX_ACCUM_BLUE_SIZE;
	    list[i++] = 1;
	    if (TK_HAS_ALPHA(type)) {
		list[i++] = GLX_ACCUM_ALPHA_SIZE;
		list[i++] = 1;
	    }
	}
    } else if (TK_IS_INDEX(type)) {
	list[i++] = GLX_BUFFER_SIZE;
	list[i++] = 1;
    }

    if (TK_HAS_DEPTH(type)) {
	list[i++] = GLX_DEPTH_SIZE;
	list[i++] = 1;
    }

    if (TK_HAS_STENCIL(type)) {
	list[i++] = GLX_STENCIL_SIZE;
	list[i++] = 1;
    }

    list[i] = (int)None;

    return glXChooseVisual(xDisplay, xScreen, list);
}

/******************************************************************************/

static XVisualInfo *FindExactMainVisual(GLenum type)
{
    int i, nvis, val, rval, gval, bval, aval;
    XVisualInfo *vis_list, *this_vis, *best_vis, sampleVis;
    int this_score, best_score;

    /* Get list of visuals for this screen */
    sampleVis.screen = xScreen;
    vis_list = XGetVisualInfo( xDisplay, VisualScreenMask, &sampleVis, &nvis);

    /* 
     * Loop through the visuals; find first one that matches the attr 
     * specified in type
     */
    best_score = -1; best_vis = NULL;
    for ( i = 0; i < nvis; i++ ) {
      this_vis = &vis_list[i];

      /* Visual must be supported by GLX */
      if ( glXGetConfig(xDisplay, this_vis, GLX_USE_GL, &val) ) continue;
      if ( !val ) continue;

      /* Visual must be in main planes which is level 0 */
      glXGetConfig(xDisplay, this_vis, GLX_LEVEL, &val);
      if ( val != 0 ) continue;

      /* Color Index or RGBA? It must match the requested value */
      glXGetConfig(xDisplay, this_vis, GLX_RGBA, &val);
      if ( TK_IS_RGB(type) && !val ) continue;
      if ( TK_IS_INDEX(type) && val ) continue;

      /* Double buffered or Single buffered? */
      glXGetConfig( xDisplay, this_vis, GLX_DOUBLEBUFFER, &val);
      if ( TK_IS_DOUBLE(type) && !val ) continue;
      if ( TK_IS_SINGLE(type) && val ) continue;

      /* If accum requested then accum rgb size must be > 0 */
      /* If alpha requested then alpha size must be > 0 */
      /* if accum & alpha requested then accum alpha size must be > 0 */
      if ( TK_IS_RGB(type) ) {
        glXGetConfig(xDisplay, this_vis, GLX_ACCUM_RED_SIZE, &rval);
        glXGetConfig(xDisplay, this_vis, GLX_ACCUM_GREEN_SIZE, &gval);
        glXGetConfig(xDisplay, this_vis, GLX_ACCUM_BLUE_SIZE, &bval);
        glXGetConfig(xDisplay, this_vis, GLX_ACCUM_ALPHA_SIZE, &aval);
        if ( TK_HAS_ACCUM(type) ) {
            if ( rval <= 0 || gval <= 0 || bval <= 0 ) continue;
        } else {
            if ( rval > 0 || gval > 0 || bval > 0 || aval > 0 ) continue;
        }

        glXGetConfig(xDisplay, this_vis, GLX_ALPHA_SIZE, &val);
        if ( TK_HAS_ALPHA(type) ) {
            if ( val <= 0 ) continue;
            if ( TK_HAS_ACCUM(type) && aval <= 0 ) continue;
        } else {
            if ( val > 0 ) continue;
        }

      }

      /* Check depth buffer */
      glXGetConfig(xDisplay, this_vis, GLX_DEPTH_SIZE, &val);
      if ( TK_HAS_DEPTH(type) ) {
            if ( val <= 0 ) continue;
      } else {
            if ( val > 0 ) continue;
      }

      /* Check stencil buffer */
      glXGetConfig( xDisplay, this_vis, GLX_STENCIL_SIZE, &val);
      if ( TK_HAS_STENCIL(type) ) {
            if ( val <= 0 ) continue;
      } else {
            if ( val > 0 ) continue;
      }

      glXGetConfig(xDisplay, this_vis, GLX_BUFFER_SIZE, &this_score);

      if (this_score > best_score ) {
          best_score = this_score;
          best_vis = this_vis;
      }

    }

    if ( best_vis ) {
        sampleVis.visualid = best_vis->visualid;
        sampleVis.screen = xScreen;
        if ( nvis > 0 ) XFree(vis_list);
        return XGetVisualInfo(xDisplay, VisualIDMask|VisualScreenMask, 
              &sampleVis, &nvis);
    } else {
        if ( nvis > 0 ) XFree(vis_list);
        return None;
    }

}

static XVisualInfo *FindOverlayVisual(void)
{
    int list[3];

    list[0] = GLX_LEVEL;
    list[1] = 1;
    list[2] = (int)None;

    return glXChooseVisual(xDisplay, xScreen, list);
}

static GLenum GetMainWindowType(XVisualInfo *vi)
{
    GLenum mask;
    int x, y, z;

    mask = 0;

    glXGetConfig(xDisplay, vi, GLX_DOUBLEBUFFER, &x);
    if (x) {
	mask |= TK_DOUBLE;
    } else {
	mask |= TK_SINGLE;
    }

    glXGetConfig(xDisplay, vi, GLX_RGBA, &x);
    if (x) {
	mask |= TK_RGB;
	glXGetConfig(xDisplay, vi, GLX_ALPHA_SIZE, &x);
	if (x > 0) {
	    mask |= TK_ALPHA;
	}
	glXGetConfig(xDisplay, vi, GLX_ACCUM_RED_SIZE, &x);
	glXGetConfig(xDisplay, vi, GLX_ACCUM_GREEN_SIZE, &y);
	glXGetConfig(xDisplay, vi, GLX_ACCUM_BLUE_SIZE, &z);
	if (x > 0 && y > 0 && z > 0) {
	    mask |= TK_ACCUM;
	}
    } else {
	mask |= TK_INDEX;
    }

    glXGetConfig(xDisplay, vi, GLX_DEPTH_SIZE, &x);
    if (x > 0) {
	mask |= TK_DEPTH;
    }

    glXGetConfig(xDisplay, vi, GLX_STENCIL_SIZE, &x);
    if (x > 0) {
	mask |= TK_STENCIL;
    }

    if (glXIsDirect(xDisplay, w.cMain)) {
	mask |= TK_DIRECT;
    } else {
	mask |= TK_INDIRECT;
    }

    return mask;
}

static int WaitForMainWindow(Display *d, XEvent *e, char *arg)
{

    if (e->type == MapNotify && e->xmap.window == w.wMain) {
	return GL_TRUE;
    } else {
	return GL_FALSE;
    }
}

static int WaitForOverlayWindow(Display *d, XEvent *e, char *arg)
{

    if (e->type == MapNotify && e->xmap.window == w.wOverlay) {
	return GL_TRUE;
    } else {
	return GL_FALSE;
    }
}

GLenum tkInitDisplay(void)
{
    int erb, evb;
 
    xDisplay = XOpenDisplay(0);
    if (!xDisplay) {
       fprintf(stderr, "Can't connect to xDisplay!\n");
       return GL_FALSE;
    }
    if (!glXQueryExtension(xDisplay, &erb, &evb)) {
       fprintf(stderr, "No glx extension!\n");
       return GL_FALSE;
    }
    xScreen = DefaultScreen(xDisplay);
    wRoot = RootWindow(xDisplay, xScreen);
    XSetErrorHandler(ErrorHandler);
    return(GL_TRUE);
}

GLenum tkInitWindow(char *title)
{
    XSetWindowAttributes wa;
    XTextProperty tp;
    XSizeHints sh;
    XEvent e;
    GLenum overlayFlag;

    if (!xDisplay && !tkInitDisplay()) return(GL_FALSE);

    if (TK_HAS_OVERLAY(w.type)) {
	overlayFlag = GL_TRUE;
    } else {
	overlayFlag = GL_FALSE;
    }
    w.type &= ~TK_OVERLAY;

    if (w.dmPolicy == TK_MINIMUM_CRITERIA)
        w.vInfoMain = FindBestMainVisual(w.type);
    else if (w.dmPolicy == TK_EXACT_MATCH)
        w.vInfoMain = FindExactMainVisual(w.type);
    if (!w.vInfoMain) {
	fprintf(stderr, "Window type not found!\n");
	return GL_FALSE;
    }

    w.cMain = glXCreateContext(xDisplay, w.vInfoMain, None,
			       (TK_IS_DIRECT(w.type))?GL_TRUE:GL_FALSE);
    if (!w.cMain) {
	fprintf(stderr, "Can't create a context!\n");
	return GL_FALSE;
    }

    w.type = GetMainWindowType(w.vInfoMain);

    if (TK_IS_INDEX(w.type)) {
	if (w.vInfoMain->class != StaticColor &&
	    w.vInfoMain->class != StaticGray) {
	    w.cMapMain = XCreateColormap(xDisplay, wRoot, w.vInfoMain->visual,
				         AllocAll);
	} else {
	    w.cMapMain = XCreateColormap(xDisplay, wRoot, w.vInfoMain->visual,
				         AllocNone);
	}
    } else {
	w.cMapMain = XCreateColormap(xDisplay, wRoot, w.vInfoMain->visual,
				     AllocNone);
    }
    tkSetRGBMap(256, colorMaps);
    wa.colormap = w.cMapMain;
    wa.background_pixmap = None;
    wa.border_pixel = 0;
    wa.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask |
		    ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    w.wMain = XCreateWindow(xDisplay, wRoot, w.x, w.y, w.w, w.h, 0,
			    w.vInfoMain->depth, InputOutput,
			    w.vInfoMain->visual,
			    CWBackPixmap|CWBorderPixel|CWEventMask|CWColormap,
			    &wa);

    XStringListToTextProperty(&title, 1, &tp);
    sh.flags = USPosition | USSize;
    XSetWMProperties(xDisplay, w.wMain, &tp, &tp, 0, 0, &sh, 0, 0);
    deleteWindowAtom = XInternAtom(xDisplay, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(xDisplay, w.wMain, &deleteWindowAtom, 1);
    XMapWindow(xDisplay, w.wMain);
    drawAllowFlag = GL_FALSE;
    XIfEvent(xDisplay, &e, WaitForMainWindow, 0);

    if (overlayFlag == GL_TRUE) {
	w.vInfoOverlay = FindOverlayVisual();
	if (w.vInfoOverlay) {
	    w.cOverlay = glXCreateContext(xDisplay, w.vInfoOverlay, None,
					  GL_TRUE);
	    w.cMapOverlay = XCreateColormap(xDisplay, wRoot,
					    w.vInfoOverlay->visual, AllocNone);
	    tkSetOverlayMap(256, colorMaps);
	    wa.colormap = w.cMapOverlay;
	    wa.background_pixmap = None;
	    wa.border_pixel = 0;
	    w.wOverlay = XCreateWindow(xDisplay, w.wMain, 0, 0, w.w, w.h, 0,
				       w.vInfoOverlay->depth, InputOutput,
				       w.vInfoOverlay->visual,
				       CWBackPixmap|CWBorderPixel|CWColormap,
				       &wa);
	    XMapWindow(xDisplay, w.wOverlay);
	    XSetWMColormapWindows(xDisplay, w.wMain, &w.wOverlay, 1);
	    w.type |= TK_OVERLAY;
	} else {
	    fprintf(stderr, "Can't create a overlay plane!\n");
	}
    }

    if (!glXMakeCurrent(xDisplay, w.wMain, w.cMain)) {
	fprintf(stderr, "Can't make window current drawable!\n");
	return GL_FALSE;
    }
    XFlush(xDisplay);

    return GL_TRUE;
}

/******************************************************************************/

void tkQuit(void)
{

    tkCloseWindow();
    exit(0);
}

/******************************************************************************/

void tkSwapBuffers(void)
{

    if (xDisplay) {
	glXSwapBuffers(xDisplay, w.wMain);
    }
}

/******************************************************************************/
