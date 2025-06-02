#include <GL/glx.h>
#include <unistd.h>

static int attributeList[] = { GLX_RGBA, None };

static Bool WaitForNotify(Display *d, XEvent *e, char *arg) {
    return (e->type == MapNotify) && (e->xmap.window == (Window)arg);
}

int main(int argc, char **argv) {
    Display *dpy;
    XVisualInfo *vi;
    Colormap cmap;
    XSetWindowAttributes swa;
    Window win;
    GLXContext cx;
    XEvent event;

    /* get a connection */
    dpy = XOpenDisplay(0);

    /* get an appropriate visual */
    vi = glXChooseVisual(dpy, DefaultScreen(dpy), attributeList);

    /* create a GLX context */
    cx = glXCreateContext(dpy, vi, 0, GL_TRUE);

    /* create a color map */
    cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
                                   vi->visual, AllocNone);

    /* create a window */
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.event_mask = StructureNotifyMask;
    win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, 100, 100,
			0, vi->depth, InputOutput, vi->visual,
			CWBorderPixel|CWColormap|CWEventMask, &swa);
    XMapWindow(dpy, win);
    XIfEvent(dpy, &event, WaitForNotify, (char*)win);

    /* connect the context to the window */
    glXMakeCurrent(dpy, win, cx);

    /* clear the buffer */
    glClearColor(1,1,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();

    /* wait a while */
    pause();
}
