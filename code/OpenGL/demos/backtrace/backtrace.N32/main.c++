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
#include <GL/glu.h>
#include <GL/glx.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/CascadeB.h>
#include <Xm/Frame.h>
#include <Xm/MainW.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/RowColumn.h>
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

static Display *display;
XtAppContext app_context;
Widget glw;

const int max_args = 20;

int quick_moves = 0;
int auto_motion = 0;

XVisualInfo *find_visual() 
{
  int attr[256], i, stencil, dbuffer;
  XVisualInfo *vi;

  i = 0;
  attr[i++] = GLX_RGBA;
  attr[i++] = GLX_RED_SIZE;
  attr[i++] = 1;
  attr[i++] = GLX_GREEN_SIZE;
  attr[i++] = 1;
  attr[i++] = GLX_BLUE_SIZE;
  attr[i++] = 1;
  attr[i++] = GLX_DEPTH_SIZE;
  attr[i++] = 1;
  dbuffer = i;
  attr[i++] = GLX_DOUBLEBUFFER;
  stencil = i;
  attr[i++] = GLX_STENCIL_SIZE;
  attr[i++] = 1;
  attr[i++] = (int)None;

  vi = glXChooseVisual(display, DefaultScreen(display), attr);
  if (vi == NULL) {
    fprintf(stderr, "Unable to find visual with stencil buffer.\n");
    fprintf(stderr, "(Things won't look quite as good).\n");
    attr[stencil] = (int)None;
    vi = glXChooseVisual(display, DefaultScreen(display), attr);
    if (vi == NULL) {
      fprintf(stderr, "Unable to find double-buffered visual.\n");
      fprintf(stderr, "(Things will look even worse).\n");
      attr[dbuffer] = (int)None;
      vi = glXChooseVisual(display, DefaultScreen(display), attr);
      if (vi == NULL) fprintf(stderr, "Can't find visual at all.\n");
    }
  }
  return vi;
}

Widget create_widgets(Widget parent)
{
  Widget main_window;
  Widget menu_bar;
  Widget menu_pane; 
  Widget button;
  Widget cascade;
  Widget frame;
  XVisualInfo *vi;

  Arg args[max_args];
  int argcount;

  char buffer[128];

  int i;

  main_window = XmCreateMainWindow(parent, "main1", NULL, 0);
  XtManageChild(main_window);
 
  menu_bar = XmCreateMenuBar(main_window, "menu_bar", NULL, 0);
  XtManageChild(menu_bar);
    
  menu_pane = XmCreatePulldownMenu(menu_bar, "menu_pane", NULL, 0);
  button = XmCreatePushButton(menu_pane, "Exit", NULL, 0);
  XtManageChild(button);
  XtAddCallback(button, XmNactivateCallback, (XtCallbackProc)exitCB, NULL);
      
  XtSetArg(args[0], XmNsubMenuId, menu_pane);
  cascade = XmCreateCascadeButton(menu_bar, "File", args, 1);
  XtManageChild(cascade);


  argcount = 0;
  XtSetArg(args[argcount], XmNradioBehavior, True); argcount++;
  menu_pane = XmCreatePulldownMenu(menu_bar, "menu_pane", args, argcount);
  XtSetArg(args[0], XmNset, TRUE);
  for (i = 0; i < nindices; i++) {
    if (i == def_refraction_index) argcount = 1;
    else argcount = 0;
    button = XmCreateToggleButton(menu_pane, (char *)indices[i].name, 
	args, argcount);
    XtManageChild(button);
    XtAddCallback(button, XmNvalueChangedCallback, 
		  (XtCallbackProc)refractionCB, 
		  (XtPointer)(&indices[i].index));
  }
  XtSetArg(args[0], XmNsubMenuId, menu_pane);
  cascade = XmCreateCascadeButton(menu_bar, "Material", args, 1);
  XtManageChild(cascade);


  menu_pane = XmCreatePulldownMenu(menu_bar, "menu_pane", NULL, 0);

  XtSetArg(args[0], XmNset, draw_square);
  button = XmCreateToggleButton(menu_pane, "Draw Square", args, 1);
  XtManageChild(button);
  XtAddCallback(button, XmNvalueChangedCallback, 
                (XtCallbackProc)drawSomethingCB, &draw_square);

  XtSetArg(args[0], XmNset, draw_shadows);
  button = XmCreateToggleButton(menu_pane, "Draw Shadows", args, 1);
  XtManageChild(button);
  XtAddCallback(button, XmNvalueChangedCallback, 
                (XtCallbackProc)drawSomethingCB, &draw_shadows);

  XtSetArg(args[0], XmNset, draw_refraction);
  button = XmCreateToggleButton(menu_pane, "Draw Refraction", args, 1);
  XtManageChild(button);
  XtAddCallback(button, XmNvalueChangedCallback, 
                (XtCallbackProc)drawSomethingCB, &draw_refraction);

  XtSetArg(args[0], XmNset, draw_sphere);
  button = XmCreateToggleButton(menu_pane, "Draw Sphere", args, 1);
  XtManageChild(button);
  XtAddCallback(button, XmNvalueChangedCallback, 
                (XtCallbackProc)drawSomethingCB, &draw_sphere);

  XtSetArg(args[0], XmNset, draw_lights);
  button = XmCreateToggleButton(menu_pane, "Draw Lights", args, 1);
  XtManageChild(button);
  XtAddCallback(button, XmNvalueChangedCallback, 
                (XtCallbackProc)drawSomethingCB, &draw_lights);

#ifdef TEXTURE
  XtSetArg(args[0], XmNset, draw_texture);
  button = XmCreateToggleButton(menu_pane, "Texture Map", args, 1);
  XtManageChild(button);
  XtAddCallback(button, XmNvalueChangedCallback, 
                (XtCallbackProc)drawSomethingCB, &draw_texture);
#endif

  XtSetArg(args[0], XmNsubMenuId, menu_pane);
  cascade = XmCreateCascadeButton(menu_bar, "Draw", args, 1);
  XtManageChild(cascade);

  argcount = 0;
  XtSetArg(args[argcount], XmNradioBehavior, True); argcount++;
  menu_pane = XmCreatePulldownMenu(menu_bar, "menu_pane", args, argcount);
  XtSetArg(args[0], XmNset, TRUE);
  for (i = 0; i < npossible_divisions; i++) {
    if (i == def_divisions_index) argcount = 1;
    else argcount = 0;
    sprintf(buffer, "%d", possible_divisions[i]);
    button = XmCreateToggleButton(menu_pane, buffer, args, argcount);
    XtManageChild(button);
    XtAddCallback(button, XmNvalueChangedCallback, 
		  (XtCallbackProc)subdivisionCB, &possible_divisions[i]);
  }
  XtSetArg(args[0], XmNsubMenuId, menu_pane);
  cascade = XmCreateCascadeButton(menu_bar, "Subdivision", args, 1);
  XtManageChild(cascade);


  menu_pane = XmCreatePulldownMenu(menu_bar, "menu_pane", args, argcount);
  button = XmCreatePushButton(menu_pane, "Reset Position", NULL, 0);
  XtManageChild(button);
  XtAddCallback(button, XmNactivateCallback, 
		(XtCallbackProc)resetLightsCB, NULL);

  XtSetArg(args[0], XmNset, TRUE);
  for (i = 0; i < nlights; i++) {
    button = XmCreateToggleButton(menu_pane, lights[i].name, args, 
				  lights[i].on ? 1 : 0);
    XtManageChild(button);
    XtAddCallback(button, XmNvalueChangedCallback, 
		  (XtCallbackProc)light_onCB, &lights[i]);
  }
  XtSetArg(args[0], XmNsubMenuId, menu_pane);
  cascade = XmCreateCascadeButton(menu_bar, "Lights", args, 1);
  XtManageChild(cascade);


  menu_pane = XmCreatePulldownMenu(menu_bar, "menu_pane", args,
				   argcount);
  XtSetArg(args[0], XmNset, quick_moves);
  button = XmCreateToggleButton(menu_pane, "Quick Motion",
				args, 1);
  XtManageChild(button);
  XtAddCallback(button, XmNvalueChangedCallback, 
		(XtCallbackProc)intToggleCB, &quick_moves);

  XtSetArg(args[0], XmNset, auto_motion);
  button = XmCreateToggleButton(menu_pane, "Rotate Automatically",
				args, 1);
  XtManageChild(button);
  XtAddCallback(button, XmNvalueChangedCallback, 
		(XtCallbackProc)autoMotionCB, NULL);

  XtSetArg(args[0], XmNsubMenuId, menu_pane);
  cascade = XmCreateCascadeButton(menu_bar, "Motion", args, 1);
  XtManageChild(cascade);


  argcount = 0;
  XtSetArg(args[argcount], XmNmarginWidth, 0); argcount++;
  XtSetArg(args[argcount], XmNmarginHeight, 0); argcount++;
  XtSetArg(args[argcount], XmNshadowThickness, 1); argcount++;
  XtSetArg(args[argcount], XmNshadowType, XmSHADOW_OUT); argcount++;
  frame = XmCreateFrame(main_window, "frame", args, argcount);
  XtManageChild(frame);


  argcount = 0;
  vi = find_visual();
  if (vi) {
    XtSetArg(args[argcount], GLwNvisualInfo, vi); argcount++;
  }
  else {
    XtSetArg(args[argcount], GLwNrgba, TRUE); argcount++;
    XtSetArg(args[argcount], GLwNdepthSize, 1); argcount++;
    XtSetArg(args[argcount], GLwNdoublebuffer, TRUE); argcount++;
  }
  XtSetArg(args[argcount], XmNbottomAttachment, XmATTACH_FORM); argcount++;
  XtSetArg(args[argcount], XmNtopAttachment, XmATTACH_FORM); argcount++;
  XtSetArg(args[argcount], XmNleftAttachment, XmATTACH_FORM); argcount++;
  XtSetArg(args[argcount], XmNrightAttachment, XmATTACH_FORM); argcount++;
  glw = GLwCreateMDrawingArea(frame, "glwidget", args, argcount);
  XtManageChild(glw);
  XtAddCallback(glw, GLwNginitCallback, (XtCallbackProc)initCB, 0);
  XtAddCallback(glw, GLwNexposeCallback, (XtCallbackProc)exposeCB, 0);
  XtAddCallback(glw, GLwNresizeCallback, (XtCallbackProc)resizeCB, 0);
  XtAddCallback(glw, GLwNinputCallback, (XtCallbackProc)inputCB, 0);

  return main_window;
}

void main(int argc, char **argv)
{
  Widget app_shell;
  Arg args[max_args];
  int argcount;

  scene_load_texture((char *)def_texfile);

  XtToolkitInitialize();
  app_context = XtCreateApplicationContext();
  display = XtOpenDisplay(app_context, NULL, argv[0],
                          "XMdemos", NULL, 0, &argc, argv);
  if (!display) {
    XtWarning("Can't open display.");
    exit(0);
  }

  argcount = 0;
  XtSetArg(args[argcount], XmNmaxAspectX, 1); argcount++;
  XtSetArg(args[argcount], XmNmaxAspectY, 1); argcount++;
  XtSetArg(args[argcount], XmNminAspectX, 1); argcount++;
  XtSetArg(args[argcount], XmNminAspectY, 1); argcount++;
  app_shell = 
    XtAppCreateShell(argv[0], "XMdemos", applicationShellWidgetClass,
                     display, args, argcount);
  
  create_widgets(app_shell);

  XtRealizeWidget(app_shell);

  XtAppMainLoop(app_context);
  
}
