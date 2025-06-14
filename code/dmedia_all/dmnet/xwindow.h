/*
 * Copyright (c) 1994 Silicon Graphics, Inc.
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
 */
/*
 * Utility functions exported by xwindow.c
 *
 * $Revision: 1.1 $
 */
#include <GL/glx.h>
#include <X11/Xlib.h>

extern Bool waitForMapNotify(Display *dpy, XEvent *ev, XPointer arg); 
extern void mapWindowAndWait(Display *dpy, Window win);
extern void createWindowAndContext(Display **dpy, Window *win, GLXContext *cx,
                                  int xpos, int ypos, int width, int height,
                                  GLboolean pref, XSizeHints *hints,
                                  int *attr, 
                                  char *title);
extern void initXInput(Display *dpy, Window win);
extern int xGetButton(int button, Display *dpy, Window win);
extern GLXContext createContext(Display *dpy, XVisualInfo *vi);
