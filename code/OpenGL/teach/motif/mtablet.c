
/* mtablet.c - OpenGL Motif program using tablet events */

/* Note that this program relies on Xt interfaces for handling X
   extension events introduced with X11R6. */

/* compile: cc -o mouse mouse.c -lGLw -lGLU -lGL -lXm -lXt -Xi
   -lXext -lX11 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <X11/GLw/GLwMDrawA.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XIproto.h>  /* For IEVENTS,
                                        unfortunately not in
                                        XInput.h */

int attribs[] = { GLX_RGBA, GLX_RED_SIZE, 1, GLX_DOUBLEBUFFER, None};
String fallbackResources[] = {
  "*glxwidget*width: 300",
  "*glxwidget*height: 300",
  "*sgiMode: True",
  NULL
};

Display *dpy;
XtAppContext appctx;
Widget glxwidget;
XDevice *tablet;
int tablet_motion_notify, tablet_press_notify,
  tablet_release_notify;
int win_width, win_height;
int axis_min[2];
int axis_range[2];
GLenum btn_state[4] = {GL_LINE, GL_LINE, GL_LINE, GL_LINE};
int tablet_pos[2] = {2000, 2000};
Bool direct, redisplay_posted = False;

void expose(Widget w, XtPointer client_data, XtPointer call);
void resize(Widget w, XtPointer client_data, XtPointer call);
void initialize_tablet(void);

void
main(int argc, char *argv[])
{
  XVisualInfo *visinfo;
  GLXContext glxcontext;
  Widget toplevel, frame;

  toplevel = XtOpenApplication(&appctx, "tablet", NULL, 0, &argc, argv,
    fallbackResources, applicationShellWidgetClass, NULL, 0);
  dpy = XtDisplay(toplevel);

  frame = XmCreateFrame(toplevel, "frame", NULL, 0);
  XtManageChild(frame);

  /* specify visual directly */
  if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs)))
    XtAppError(appctx, "no suitable RGB visual");

  glxwidget = XtVaCreateManagedWidget("glxwidget", glwMDrawingAreaWidgetClass,
    frame, GLwNvisualInfo, visinfo, XtNwidth, 300, XtNheight, 300, NULL);
  XtAddCallback(glxwidget, GLwNexposeCallback, expose, NULL);
  XtAddCallback(glxwidget, GLwNresizeCallback, resize, NULL);

  initialize_tablet();

  XtRealizeWidget(toplevel);

  glxcontext = glXCreateContext(dpy, visinfo, 0, GL_TRUE);
  direct = glXIsDirect(dpy, glxcontext);
  GLwDrawingAreaMakeCurrent(glxwidget, glxcontext);

  glMatrixMode(GL_PROJECTION);
  glOrtho(0, 4000, 0, 4000, 0, 1);
  glMatrixMode(GL_MODELVIEW);
  glClearColor(0.5, 0.5, 0.5, 0.);

  XtAppMainLoop(appctx);
}

void select_xinput_events(Widget w, int *event_types,
  XtPointer * select_data, int count, XtPointer client_data);
Boolean dispatch_xinput_event(XEvent * event);
void tablet_handler(Widget w, XtPointer client_data,
  XEvent * event, Boolean * continue_to_dispatch);

void
initialize_tablet(void)
{
  Bool exists;
  XExtensionVersion *version;
  XDeviceInfoPtr device_info, device;
  XAnyClassPtr any;
  XEventClass tablet_motion_class, tablet_press_class, tablet_press_grab_class,
    tablet_release_class;
  XButtonInfoPtr b;
  XValuatorInfoPtr v;
  XAxisInfoPtr a;
  int opcode, event_base, error_base;
  int num_dev;
  int i, j, k;

  exists = XQueryExtension(dpy, "XInputExtension", &opcode,
    &event_base, &error_base);
  if (!exists) {
    return;
  }
  version = XGetExtensionVersion(dpy, "XInputExtension");
  if (version == NULL || ((int) version) == NoSuchExtension) {
    return;
  }
  XFree(version);
  device_info = XListInputDevices(dpy, &num_dev);
  if (device_info) {
    for (i = 0; i < num_dev; i++) {
      device = &device_info[i];
      any = (XAnyClassPtr) device->inputclassinfo;

      if (!strcmp(device->name, "tablet")) {
        v = NULL;
        b = NULL;
        for (j = 0; j < device->num_classes; j++) {
          switch (any->class) {
          case ButtonClass:
            b = (XButtonInfoPtr) any;
            /* Sanity check: at least 1 button (normally 4). */
            if (b->num_buttons < 1) {
              goto skip_device;
            }
            break;
          case ValuatorClass:
            v = (XValuatorInfoPtr) any;
            /* Sanity check: exactly 2 valuators? */
            if (v->num_axes != 2) {
              goto skip_device;
            }
            a = (XAxisInfoPtr) ((char *) v + sizeof(XValuatorInfo));
            for (k = 0; k < 2; k++, a++) {
              axis_min[k] = a->min_value;
              axis_range[k] = a->max_value - a->min_value;
            }
            break;
          }
          any = (XAnyClassPtr) ((char *) any + any->length);
        }
        tablet = XOpenDevice(dpy, device->id);
        if (tablet) {

          /* From the tablet device, extract the type and class 
             of the motion notify, button press, and button
             release events. */
          DeviceMotionNotify(tablet,
            tablet_motion_notify, tablet_motion_class);
          DeviceButtonPress(tablet,
            tablet_press_notify, tablet_press_class);
          DeviceButtonPressGrab(tablet,
            not_used_by_macro, tablet_press_grab_class);
          DeviceButtonRelease(tablet,
            tablet_release_notify, tablet_release_class);

          /* Register an event selection routine for the X
             Input extension events so Xt will know how to
             select for these events for a specific widget when 
             XtInsertEventTypeHandler is used to supply an
             event handler for an X Input extension event for a 
             widget. */
          XtRegisterExtensionSelector(dpy, event_base,
            event_base + IEVENTS - 1, select_xinput_events, NULL);

          /* Register a dispatch procedure for the various X
             Input extension events  so Xt will know to
             dispatch these events instead of discarding them. */
          XtSetEventDispatcher(dpy, tablet_motion_notify,
	    dispatch_xinput_event);
          XtSetEventDispatcher(dpy, tablet_press_notify,
	    dispatch_xinput_event);
          XtSetEventDispatcher(dpy, tablet_release_notify,
	    dispatch_xinput_event);

          /* Provide an event handler for the given widget for
             the given X Input extension event. */
          XtInsertEventTypeHandler(glxwidget,
            tablet_motion_notify, (XtPointer) tablet_motion_class,
            tablet_handler, NULL, XtListTail);
          XtInsertEventTypeHandler(glxwidget,
            tablet_press_notify, (XtPointer) tablet_press_class,
            tablet_handler, NULL, XtListTail);
          XtInsertEventTypeHandler(glxwidget,
            tablet_press_notify, (XtPointer) tablet_press_grab_class,
            tablet_handler, NULL, XtListTail);
          XtInsertEventTypeHandler(glxwidget,
            tablet_release_notify, (XtPointer) tablet_release_class,
            tablet_handler, NULL, XtListTail);

          /* Tablet found and setup, look no further. */
          XFreeDeviceList(device_info);
          return;
        }
      }
    skip_device:;
    }
    XFreeDeviceList(device_info);
  }
  /* Did not find tablet device. */
  fprintf(stderr, "wtablet: no tablet device found!\n");
  fprintf(stderr, "         continuing without tablet support.\n");
  return;
}

void
select_xinput_events(Widget w, int *event_types,
  XtPointer * select_data, int count, XtPointer client_data)
{
  XEventClass *xcp = (XEventClass *) select_data;

  XSelectExtensionEvent(XtDisplay(w), XtWindow(w), xcp, count);
}

Boolean
dispatch_xinput_event(XEvent * event)
{
  Widget w;
  XAnyEvent *xany = (XAnyEvent *) event;

  w = XtWindowToWidget(xany->display, xany->window);

  /* Potentially, you could insert "w =
     XtGetKeyboardFocusWidget(w);" if you wished to redirect
     the X Input extension events to the widget with keyboard
     focus.  This is almost certainly, not what you would want
     for a device like a tablet that has a sense of position
     within the window for which the events are generated, but
     for another device like an alternate keyboard,
     XtGetKeyboardFocusWidget might be appropriate. */

  if (!XFilterEvent(event, (w == NULL) ? None : XtWindow(w))) {
    return XtDispatchEventToWidget(w, event);
  } else {
    return True;
  }
}

void tablet_pos_change(int first, int count, int *data);
void post_redisplay(void);

void
tablet_handler(Widget w, XtPointer client_data,
  XEvent * event, Boolean * continue_to_dispatch)
{
  if (event->type == tablet_motion_notify) {
    XDeviceMotionEvent *devmot = (XDeviceMotionEvent *) event;

    tablet_pos_change(devmot->first_axis, devmot->axes_count,
      devmot->axis_data);
  } else if (event->type == tablet_press_notify) {
    XDeviceButtonPressedEvent *devbtn = (XDeviceButtonEvent *) event;

    btn_state[devbtn->button - 1] = GL_FILL;
    post_redisplay();
  } else if (event->type == tablet_release_notify) {
    XDeviceButtonReleasedEvent *devbtn = (XDeviceButtonEvent *) event;

    btn_state[devbtn->button - 1] = GL_LINE;
    post_redisplay();
  }
}

int normalize_tablet_pos(int axis, int rawValue);
void query_tablet_pos(void);

void
tablet_pos_change(int first, int count, int *data)
{
  int i, value, genEvent = 0;

  for (i = first; i < first + count; i++) {
    switch (i) {
    case 0:            /* X axis */
    case 1:            /* Y axis */
      value = normalize_tablet_pos(i, data[i - first]);
      if (value != tablet_pos[i]) {
        tablet_pos[i] = value;
        genEvent = 1;
      }
      break;
    }
  }
  if (tablet_pos[0] == -1 || tablet_pos[1] == -1)
    query_tablet_pos();
  if (genEvent) {
    post_redisplay();
  }
}

int
normalize_tablet_pos(int axis, int rawValue)
{
  assert(rawValue >= axis_min[axis]);
  assert(rawValue <= axis_min[axis] + axis_range[axis]);
  /* Normalize rawValue to between 0 and 4000. */
  return ((rawValue - axis_min[axis]) * 4000) /
    axis_range[axis];
}

void
query_tablet_pos(void)
{
  XDeviceState *state;
  XInputClass *any;
  XValuatorState *v;
  int i;

  state = XQueryDeviceState(dpy, tablet);
  any = state->data;
  for (i = 0; i < state->num_classes; i++) {
    switch (any->class) {
    case ValuatorClass:
      v = (XValuatorState *) any;
      if (v->num_valuators < 2)
        goto end;
      if (tablet_pos[0] == -1)
        tablet_pos[0] = normalize_tablet_pos(0, v->valuators[0]);
      if (tablet_pos[1] == -1)
        tablet_pos[1] = normalize_tablet_pos(1, v->valuators[1]);
    }
    any = (XInputClass *) ((char *) any + any->length);
  }
end:
  XFreeDeviceState(state);
}

void draw_scene(void);

void
do_redisplay(void *client_data, XtIntervalId * id)
{
  draw_scene();
  redisplay_posted = False;
}

void
post_redisplay(void)
{
  if (!redisplay_posted) {
    redisplay_posted = True;
    XtAppAddTimeOut(appctx, 5, do_redisplay, NULL);
  }
}

void
expose(Widget w, XtPointer client_data, XtPointer call)
{
  post_redisplay();
}

void
diamond(GLenum mode)
{                  
  glPushMatrix();
  glPolygonMode(GL_FRONT_AND_BACK, mode);
  glRotatef(45., 0., 0., 1.);
  glRectf(-0.5, -0.5, 1, 1);
  glPopMatrix();
}

void
puck(void)
{                    
  /* Make a puck out of 4 diamonds. */

  glTranslatef(0, 1.2, 0);
  glColor3f(1.0, 1.0, 0.0);
  diamond(btn_state[0]);

  glTranslatef(-1.2, -1.2, 0);
  glColor3f(1.0, 1.0, 1.0);
  diamond(btn_state[1]);

  glTranslatef(1.2, -1.2, 0);
  glColor3f(0.0, 0.0, 1.0);
  diamond(btn_state[2]);

  glTranslatef(1.2, 1.2, 0);
  glColor3f(0.0, 1.0, 0.0);
  diamond(btn_state[3]);
}

void
draw_scene(void)
{
  glClear(GL_COLOR_BUFFER_BIT);
  glPushMatrix();
  glTranslatef(tablet_pos[0], tablet_pos[1], 0);
  glScalef(200, 200, 1);
  puck();
  glPopMatrix();
  GLwDrawingAreaSwapBuffers(glxwidget);
  if (!direct)
    glFinish();         /* To improve net interactivity. */
}

void
resize(Widget w, XtPointer client_data, XtPointer call)
{
  GLwDrawingAreaCallbackStruct *call_data;
  call_data = (GLwDrawingAreaCallbackStruct *) call;

  win_width = call_data->width;
  win_height = call_data->height;
  glViewport(0, 0, win_width, win_height);
}
