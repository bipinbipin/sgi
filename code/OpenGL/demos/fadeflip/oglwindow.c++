#include "oglwindow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int RGBA_SB_attributes[] = {
  GLX_RGBA,
  GLX_RED_SIZE, 1,
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_DEPTH_SIZE, 1,
  None,
};

static int RGBA_DB_attributes[] = {
  GLX_RGBA,
  GLX_RED_SIZE, 1,
  GLX_GREEN_SIZE, 1,
  GLX_BLUE_SIZE, 1,
  GLX_DOUBLEBUFFER,
  GLX_DEPTH_SIZE, 1,
  None,
};

oglwindow::oglwindow()
{
  doublebuffer = 1;
  minx = miny = maxx = maxy = 0;
  window_x = window_y = window_w = window_h = 0;
  event_mask = ExposureMask | StructureNotifyMask | KeyPressMask |
    ButtonPressMask;
  strncpy(title, "OpenGL", max_titlelength);
}

void oglwindow::set_doublebuffer(int new_doublebuffer)
{
  doublebuffer = new_doublebuffer;
}

void oglwindow::set_position(int new_x, int new_y) 
{
  window_x = new_x;
  window_y = new_y;
}

void oglwindow::set_size(int new_x, int new_y) 
{
  window_w = new_x;
  window_h = new_y;
}

void oglwindow::set_minsize(int new_minx, int new_miny) 
{
  minx = new_minx;
  miny = new_miny;
}

void oglwindow::set_maxsize(int new_maxx, int new_maxy)
{
  maxx = new_maxx;
  maxy = new_maxy;
}

void oglwindow::set_title(char *new_title)
{
  strncpy(title, new_title, max_titlelength);
}

void oglwindow::set_event_mask(unsigned long new_event_mask) 
{
  event_mask = new_event_mask;
}

void oglwindow::add_event_mask(unsigned long new_flag)
{
  event_mask |= new_flag;
}

unsigned long oglwindow::get_event_mask()
{
  return event_mask;
}

int oglwindow::get_width() 
{
  XWindowAttributes winattr;
  XGetWindowAttributes(dpy, window, &winattr);
  return winattr.width;
}

int oglwindow::get_height() 
{
  XWindowAttributes winattr;
  XGetWindowAttributes(dpy, window, &winattr);
  return winattr.height;
}

char *oglwindow::get_title()
{
  return title;
}

void oglwindow::open() 
{
  int can_ogl, can_zbuffer, can_rgba;
  int x,y,w,h;
  XSetWindowAttributes swa;
  XSizeHints hints;

  dpy = XOpenDisplay(0);
  if (dpy == NULL) {
    fprintf(stderr, "Can't connect to display \"%s\"\n", getenv("DISPLAY"));
    exit(1);
  }

  vi = glXChooseVisual(dpy, DefaultScreen(dpy), 
		       doublebuffer ? RGBA_DB_attributes : 
		       RGBA_SB_attributes);
  if (vi == NULL) {
    fprintf(stderr, "No matching visual on \"%s\"\n",
	    getenv("DISPLAY"));
    exit(1);
  }

  glXGetConfig(dpy, vi, GLX_USE_GL, &can_ogl);
  if (!can_ogl) {
    system("inform 'Your system must support OpenGL to run this program'");
    exit(1);
  }
  glXGetConfig(dpy, vi, GLX_RGBA, &can_rgba);
  if (!can_rgba) {
    system("inform 'Your system must support RGB mode to run this program'");
    exit(1);
  }
  glXGetConfig(dpy, vi, GLX_DEPTH_SIZE, &can_zbuffer);
  if (can_zbuffer == 0) {
    system("inform 'Your system must have a z-buffer to run this program'");
    exit(1);
  }

  hints.flags = 0;
  if (window_x && window_y) {
    hints.x = window_x;
    hints.y = window_y;
    hints.flags |= USPosition;
    x = window_x;
    y = window_y;
  } else {
    x = 10;
    y = 10;
  }
  if (window_w && window_h) {
    hints.width = window_w;
    hints.height = window_h;
    hints.flags |= PSize;
    w = window_w;
    h = window_h;
  } else {
    w = 50;
    h = 50;
  }
  if (minx && miny) {
    hints.min_width = minx;
    hints.min_height = miny;
    hints.flags |= PMinSize;
  }
  if (maxx && maxy) {
    hints.max_width = maxx;
    hints.max_height = maxy;
    hints.flags |= PMaxSize;
  } 
  if (!hints.flags) {
    hints.min_aspect.x = hints.min_aspect.y =
      hints.max_aspect.x = hints.max_aspect.y = 1;
    hints.flags = PAspect;
  }

  swa.border_pixel = 0;
  swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
				 vi->visual, AllocNone);
  swa.event_mask = event_mask;
  window = XCreateWindow(dpy, RootWindow(dpy, vi->screen), x, y, w, h,
			 0, vi->depth, InputOutput, vi->visual,
			 CWBorderPixel | CWColormap | CWEventMask, &swa);
  XStoreName(dpy, window, title);

  XSetNormalHints(dpy, window, &hints);

}

static Bool WaitForMapNotify(Display *d, XEvent *e, char *arg)
{
  if ((e->type == MapNotify) && (e->xmap.window == (Window)arg)) {
    return GL_TRUE;
  }
  return GL_FALSE;
}

void oglwindow::map() 
{
  XEvent ev;

  XMapWindow(dpy, window);
  XIfEvent(dpy, &ev, WaitForMapNotify, (char *)window);

  ctx = glXCreateContext(dpy, vi, 0, GL_TRUE);
}

void oglwindow::winset()
{
  glXMakeCurrent(dpy, window, ctx);
}

void oglwindow::swapbuffers()
{
  glXSwapBuffers(dpy, window);
}

void oglwindow::resize() 
{
  XWindowAttributes windowattr;

  XGetWindowAttributes(dpy, window, &windowattr);
  glViewport(0, 0, windowattr.width, windowattr.height);
}

Display *oglwindow::get_display() 
{
  return dpy;
}

Window oglwindow::get_window()
{
  return window;
}

int oglwindow::get_doublebuffer() 
{
  return doublebuffer;
}

