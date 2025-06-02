/*
 * (c) Copyright 1993, 1994, 1995, 1996 Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED 
 * Permission to use, copy, modify, and distribute this software for 
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that 
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission. 
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * US Government Users Restricted Rights 
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */
#include <X11/keysym.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/ToggleB.h>

#include <GL/glu.h>
#include <GL/glx.h>
#include <GL/GLwMDrawA.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "scene.h"
#include "callbacks.h"

extern Widget glw;
extern XtAppContext app_context;
GLXContext glx_context;

extern int quick_moves;

extern int auto_motion;
static float dtheta[nlights];
static float last_motion_update;
struct timeval starttime;

static int button_down;
static int winx, winy;

static int name_selected;

static XtWorkProcId workproc = NULL;

const float time_fudge = .0001;

inline float current_time()
{
  struct timeval time;
  gettimeofday(&time, NULL);
  return ((double)(time.tv_sec - starttime.tv_sec) + 
	  (double)(time.tv_usec - starttime.tv_usec) / 1000000.0);
}

inline float rand(float min, float max) 
{
  double r;
  r = (double)rand() / (double)RAND_MAX;
  return min + r * (max - min);
}

static void draw()
{
  GLwDrawingAreaMakeCurrent(glw, glx_context);
  scene_draw();
  GLwDrawingAreaSwapBuffers(glw);
}

void intToggleCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  int *data;
  XmToggleButtonCallbackStruct *ptr;
  ptr = (XmToggleButtonCallbackStruct *)call_data;
  data = (int *)client_data;
  *data = ptr->set;
  // This redraw may or may not be needed - do it to be safe
  draw();
}

void resetLightsCB(Widget w)
{
  scene_reset_lights();
  draw();
}

void autoMotionCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  int i;
  XmToggleButtonCallbackStruct *ptr;

  ptr = (XmToggleButtonCallbackStruct *)call_data;

  auto_motion = ptr->set;
  if (auto_motion) {
    workproc = XtAppAddWorkProc(app_context, drawWP, NULL);
    for (i = 0; i < nlights; i++) dtheta[i] = rand(-1, 1);
    last_motion_update = current_time();
  } else {
    XtRemoveWorkProc(workproc);
  }
}


void initCB(Widget w)
{
  Arg args[1];
  XVisualInfo *vi;

  XtSetArg(args[0], GLwNvisualInfo, &vi);
  XtGetValues(w, args, 1);
 
  glx_context = glXCreateContext(XtDisplay(w), vi, 0, GL_FALSE);
  GLwDrawingAreaMakeCurrent(w, glx_context);

  scene_init(); 

  gettimeofday(&starttime, NULL);
  srand(starttime.tv_usec);
}

void exposeCB(Widget w)
{
  draw();
}

void resizeCB(Widget w, XtPointer client_data, XtPointer call)
{
  GLwDrawingAreaCallbackStruct *call_data;
  call_data = (GLwDrawingAreaCallbackStruct *)call;

  GLwDrawingAreaMakeCurrent(w, glx_context);
  winx = call_data->width;
  winy = call_data->height;

  glViewport(0, 0, winx, winy);

  aspect = (GLfloat)winx / (GLfloat)winy;
}

void inputCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  int picked;
  GLwDrawingAreaCallbackStruct *call;

  char buffer[5];
  int bufsize = 5;
  KeySym key;
  XComposeStatus compose;

  static int mousex, mousey;
  /* Just to confuse everybody, I've made these go from 0-1. */
  float dmousex, dmousey;
  float r1, r2;

  call = (GLwDrawingAreaCallbackStruct *)call_data;
  
  GLwDrawingAreaMakeCurrent(w, glx_context);
  switch(call->event->type) {
  case ButtonPress:
    button_down = call->event->xbutton.button;
    mousex = call->event->xbutton.x;
    mousey = call->event->xbutton.y;
    picked = scene_pick(mousex, mousey);
    if (picked >= name_lights) name_selected = picked;
    break;
  case ButtonRelease:
    if (quick_moves) 
      scene_move_update(name_selected, button_down == Button2, 
			button_down == Button3, button_down = Button1);
    button_down = 0; 
    break;
  case MotionNotify:
    if (button_down == Button1) {
      /* This is the "default" mouse button - moves things in theta
       * since this is easy and computationally cheap */
      dmousex = (double)(call->event->xmotion.x - mousex) / (double)winx;
      scene_move(name_selected, 0, 0, dmousex, quick_moves ? 0 : 1);
    } else if (button_down == Button2) {
      /* Change the radius - figue out the component of the mouse motion
       * that's going toward the center of the screen */
      mousex = (winx / 2) - mousex;
      mousey = (winy / 2) - mousey;
      r1 = sqrt((float)(mousex*mousex) / (float)(winx*winx) + 
		(float)(mousey*mousey) / (float)(winy*winy));
      mousex = call->event->xmotion.x;
      mousey = call->event->xmotion.y;
      mousex = (winx / 2) - mousex;
      mousey = (winy / 2) - mousey;
      r2 = sqrt((float)(mousex*mousex) / (float)(winx*winx) + 
		(float)(mousey*mousey) / (float)(winy*winy));
      scene_move(name_selected, r2 - r1, 0, 0, quick_moves ? 0 : 1);
    } else if (button_down == Button3) {
      /* Change phi - this is expensive */
      dmousex = (double)(call->event->xmotion.x - mousex) / (double)winx;
      scene_move(name_selected, 0, dmousex, 0, quick_moves ? 0 : 1);
    }
    mousex = call->event->xmotion.x;
    mousey = call->event->xmotion.y;
    break;
  case KeyPress:
    XLookupString(&(call->event->xkey), buffer, bufsize, &key, &compose);
    if (key == XK_Escape) exit(0);
    break;                            
  default:
    break;
  }

  draw();
}

void drawAllCB(Widget w)
{
  draw_square = 1;
  draw_shadows = 1;
  draw_refraction = 1;
  draw_sphere = 1;
  draw_lights = 1;
  draw();
}

void drawSomethingCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  XmToggleButtonCallbackStruct *ptr;
  int *data;
  int i;

  ptr = (XmToggleButtonCallbackStruct *)call_data;
  data = (int *)client_data;
  *data = ptr->set;
  draw();
}

void refractionCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  XmToggleButtonCallbackStruct *ptr;
  GLfloat refraction;

  ptr = (XmToggleButtonCallbackStruct *)call_data;
  if (!ptr->set) return;
  refraction = *((GLfloat *)client_data);
  refraction_change(refraction);
  draw();
}

void subdivisionCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  XmToggleButtonCallbackStruct *ptr;
  int subdivisions;

  ptr = (XmToggleButtonCallbackStruct *)call_data;
  if (!ptr->set) return;
  subdivisions = *((int *)client_data);
  divisions_change(subdivisions);
  draw();
}

void light_onCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  XmToggleButtonCallbackStruct *ptr;

  ptr = (XmToggleButtonCallbackStruct *)call_data;
  lights_onoff((light *)client_data - lights, ptr->set);
  draw();
}

void exitCB(Widget w, XtPointer client_data, XtPointer call_data)
{
  exit(0);
}

Boolean drawWP(XtPointer data)
{
  float t, dt;
  int i;

  t = current_time();
  dt = t - last_motion_update;
  if (dt < time_fudge) return FALSE;

  for (i = 0; i < nlights; i++) {
    scene_move(name_lights + i, 0, 0, dtheta[i] * dt, 1);
  }

  last_motion_update = t;

  draw();
  return FALSE;
}


