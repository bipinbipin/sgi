#include <GL/glc.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


static int glctest_visAttrib[] = {GLX_RED_SIZE, 1, GLX_RGBA, None,};


static Bool glctest_awaitMap(Display* inDpy, XEvent* inEv, char* inArg)
{

    return inEv->type == MapNotify && inEv->xmap.window == (Window)inArg;
}

static void glctest_fatal(const char *inError)
{

    fprintf(stderr, "glctest fatal: %s\n", inError);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    GLint buf[4];
    GLXContext ctx;
    Display *dpy;
    XEvent ev;
    XSizeHints hints;
    GLint none = 0;
    XSetWindowAttributes swa;
    XVisualInfo *vis;
    Window win;

    dpy = XOpenDisplay(0);
    if (!dpy) {
	glctest_fatal("XOpenDisplay");
    }
    vis = glXChooseVisual(dpy, DefaultScreen(dpy), glctest_visAttrib);
    if (!vis) {
	glctest_fatal("glXChooseVisual");
    }
    swa.border_pixel = 0;
    swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vis->screen),
				   vis->visual, AllocNone);
    if (!swa.colormap) {
	glctest_fatal("XCreateColormap");
    }
    swa.event_mask = StructureNotifyMask;
    win = XCreateWindow(dpy, RootWindow(dpy, vis->screen), 0, 0, 400, 400,
			0, vis->depth, InputOutput, vis->visual,
			CWBorderPixel|CWColormap|CWEventMask, &swa);
    if (!win) {
	glctest_fatal("XCreateWindow");
    }
    hints.flags = USPosition | USSize;
    hints.x = hints.y = 0;
    hints.width = hints.height = 400;
    XSetStandardProperties(dpy, win, "test", "test", None, argv, argc, &hints);
    XMapWindow(dpy, win);
    XIfEvent(dpy, &ev, glctest_awaitMap, (char *)win);
    ctx = glXCreateContext(dpy, vis, None, GL_TRUE);
    if (!ctx) {
	glctest_fatal("glXCreateContext");
    }
    if (!glXMakeCurrent(dpy, win, ctx)) {
	glctest_fatal("glXMakeCurrent");
    }
    glMatrixMode(GL_PROJECTION);
    glOrtho(0., 400., 0., 400., -1., 1.);
    glMatrixMode(GL_MODELVIEW);
    glClearColor(0.f, 0.f, 1.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glcContext(glcGenContext());
    glcFont(glcNewFontFromFamily(1, "Palatino"));
    glcFontFace(1, "Bold Italic");
    glcScale(48.f, 48.f);
    glcRotate(30.f);
    glColor3f(1.f, 0.f, 0.f);
    glRasterPos2f(40.f, 40.f);
    glcRenderStyle(GLC_BITMAP);
    glcRenderString("Hello, world!");
    glTranslatef(40.f, 140.f, 0.f);
    glScalef(48.f, 48.f, 1.f);
    glRotatef(30.f, 0.f, 0.f, 1.f);
    glColor3f(0.f, 1.f, 0.f);
    glcRenderStyle(GLC_TRIANGLE);
    glcRenderString("Hello, world!");
    sleep(4);
    return 0;
}
