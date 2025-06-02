/*
 * xwindow.c
 *
 * Copyright (c) 1995 Silicon Graphics, Inc.
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
#include <GL/glx.h>
#include <GL/gl.h>
#include "xwindow.h"

/* ARGSUSED dpy */
Bool
waitForMapNotify(Display *dpy, XEvent *ev, XPointer arg)
{
    return (ev->type == MapNotify && ev->xmapping.window == *((Window *) arg));
}

void
createWindowAndContext(Display **dpy, Window *win, GLXContext *cx,
                       int xpos, int ypos, int width, int height,
                       GLboolean pref, XSizeHints *hints,
                       int *visualAttr,
                       char *title)
{
    XSetWindowAttributes swa;
    Colormap cmap;
    XVisualInfo *vi;
    int isRgba;
    int i;

    /* get a connection */
    *dpy = XOpenDisplay(NULL);
    if (*dpy == NULL) {
        fprintf(stderr, "Can't connect to display `%s'\n",
                getenv("DISPLAY"));
	exit(EXIT_FAILURE);
    }

    /* get an appropriate visual */
    vi = glXChooseVisual(*dpy, DefaultScreen(*dpy), visualAttr);
    if (vi == NULL) {
        fprintf(stderr, "No matching visual on display `%s'\n",
                getenv("DISPLAY"));
        exit(EXIT_FAILURE);
    }
    (void) glXGetConfig(*dpy, vi, GLX_RGBA, &isRgba);
    /* create a GLX context */
    if ((*cx = createContext(*dpy, vi)) == NULL) {
	exit(EXIT_FAILURE);
    }

    /* create a colormap (it's empty for rgba visuals) */
    cmap = XCreateColormap(*dpy, RootWindow(*dpy, vi->screen),
                           vi->visual, isRgba ? AllocNone : AllocAll);
    if (cmap == NULL) {
	fprintf(stderr,"Cannot create a colormap.\n");
	exit(EXIT_FAILURE);
    }

    /* fill the colormap with something simple */
    if (!isRgba) {
        static char *cnames[] = {
            "black","red","green","yellow","blue","magenta","cyan","white"
        };

	for (i = 0; i < sizeof(cnames)/sizeof(cnames[0]); i++) {
	    XStoreNamedColor(*dpy, cmap, cnames[i], i, DoRed|DoGreen|DoBlue);
	}
    }

    /* create a window */
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.event_mask = StructureNotifyMask | ButtonPressMask |
        KeyPressMask | ExposureMask;
    *win = XCreateWindow(*dpy, RootWindow(*dpy, vi->screen),
                        xpos, ypos, width, height,
                        0, vi->depth, InputOutput, vi->visual,
                        CWBorderPixel | CWColormap | CWEventMask, &swa);
    if (*win == NULL) {
	fprintf(stderr,"Cannot create a window.\n");
	exit(EXIT_FAILURE);
    }

    XStoreName(*dpy, *win, title);

    /* handle "prefposition" style window */
    if (pref) {
	XSizeHints hints;

	hints.flags = USPosition|USSize; /*was PPosition|PSize which doesn't work*/
	hints.x = xpos;
	hints.y = ypos;
	hints.width = width;
	hints.height = height;

	XSetNormalHints(*dpy, *win,&hints);
    } else if (hints != NULL)
	XSetNormalHints(*dpy, *win, hints);

    mapWindowAndWait(*dpy, *win);

    /* Connect the context to the window */
    if (!glXMakeCurrent(*dpy, *win, *cx)) {
        fprintf(stderr, "Can't make window current to context\n");
        exit(EXIT_FAILURE);
    }
}

void
mapWindowAndWait(Display *dpy, Window win)
{
    XEvent event;

    XMapWindow(dpy, win);
    XIfEvent(dpy, &event, waitForMapNotify, (XPointer) &win);
}

/*
 *  simple mouse button polling code for X
 */
int
xGetButton(int button, Display *dpy, Window win)
{
    Window root,child;
    int x,y;
    unsigned int m;
    unsigned int mask = 0;
    int ok;

    switch (button) {
	case 1:
	    mask = Button1Mask;
	    break;
	case 2:
	    mask = Button2Mask;
	    break;
	case 3:
	    mask = Button3Mask;
	    break;
    }

    ok = XQueryPointer(dpy, win, &root, &child, &x, &y, &x, &y, &m);

    return ok && (m & mask);
}


/*
 * Create a context and print stuff on failure.
 */
GLXContext
createContext(Display *dpy, XVisualInfo *vi)
{
    GLXContext cx;

    /* create a GLX context */
    if ((cx = glXCreateContext(dpy, vi, 0, GL_TRUE)) == NULL)
	fprintf(stderr,"Cannot create a context.\n");
    return cx;
}

