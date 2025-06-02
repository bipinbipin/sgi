#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Global variables */
Display *dpy;
XDevice *dialbox_device = NULL;
int motion_event_type = 0;
int button_press_event_type = 0;
int button_release_event_type = 0;
int button_press_grab_event_type = 0;
int state_notify_event_type = 0;
int xinput_event_base = 0;
XEventClass event_classes[5];
Window window;
int dials[8]; /* Store dial angles in degrees */
int buttons[32]; /* Store button states (0 = up, 1 = down) */
int dial_resolutions[8]; /* Store axis resolutions */

/* Error handler */
static int x_error_handler(Display *display, XErrorEvent *error)
{
    char error_msg[256];
    XGetErrorText(display, error->error_code, error_msg, sizeof(error_msg));
    fprintf(stderr, "X Error: %s\n  Major opcode: %d\n  Serial: %ld\n",
            error_msg, error->request_code, error->serial);
    return 0;
}

/* Normalize raw dial value to degrees */
static int normalize_dial_angle(int axis, int raw_value)
{
    double scaled;
    if (dial_resolutions[axis] == 0) {
        return (raw_value % 360 + 360) % 360; /* Fallback */
    }
    scaled = (double)raw_value / dial_resolutions[axis] * 360.0;
    return (int)(scaled + 360.0) % 360;
}

/* Find and configure Dialbox device */
XDevice *find_dialbox(Display *display, Window win)
{
    int ndevices;
    int i;
    int j;
    int k;
    XDeviceInfo *devices;
    XDevice *dialbox;
    XAnyClassPtr class;
    Window root, parent, *children;
    unsigned int nchildren;

    devices = NULL;
    dialbox = NULL;
    class = NULL;
    root = RootWindow(display, DefaultScreen(display));
    parent = 0;
    children = NULL;
    nchildren = 0;

    /* Try enabling device */
    system("sgidials start >/dev/null 2>&1");

    devices = XListInputDevices(display, &ndevices);
    printf("Listing input devices:\n");
    for (i = 0; i < ndevices; i++) {
        printf("  Device %ld: %s (type: %ld)\n", devices[i].id, devices[i].name, devices[i].type);
        if (strstr(devices[i].name, "dial") || strstr(devices[i].name, "Dialbox")) {
            dialbox = XOpenDevice(display, devices[i].id);
            if (dialbox) {
                printf("Found and opened Dialbox: %s (ID: %ld)\n", devices[i].name, devices[i].id);
                printf("Number of classes: %ld\n", devices[i].num_classes);
                class = devices[i].inputclassinfo;
                for (j = 0; j < devices[i].num_classes; j++) {
                    if (class->class == ValuatorClass) {
                        XValuatorInfo *val = (XValuatorInfo *)class;
                        XAxisInfoPtr axis;
                        printf("Valuator: axes=%d, mode=%d\n", val->num_axes, val->mode);
                        axis = (XAxisInfoPtr)((char *)val + sizeof(XValuatorInfo));
                        for (k = 0; k < val->num_axes && k < 8; k++, axis++) {
                            dial_resolutions[k] = axis->resolution;
                            printf("Axis %d: resolution=%d\n", k + 1, axis->resolution);
                        }
                    } else if (class->class == ButtonClass) {
                        XButtonInfo *btn = (XButtonInfo *)class;
                        printf("Button: num_buttons=%d\n", btn->num_buttons);
                    } else if (class->class == KeyClass) {
                        XKeyInfo *key = (XKeyInfo *)class;
                        printf("Key: num_keys=%d\n", key->num_keys);
                    }
                    class = (XAnyClassPtr)((char *)class + class->length);
                }
                
                DeviceMotionNotify(dialbox, motion_event_type, event_classes[0]);
                DeviceButtonPress(dialbox, button_press_event_type, event_classes[1]);
                DeviceButtonRelease(dialbox, button_release_event_type, event_classes[2]);
                DeviceButtonPressGrab(dialbox, button_press_grab_event_type, event_classes[3]);
                DeviceStateNotify(dialbox, state_notify_event_type, event_classes[4]);
                printf("Motion event type: %d, Button press: %d, Button release: %d, Button press grab: %d, State notify: %d\n",
                       motion_event_type, button_press_event_type, button_release_event_type,
                       button_press_grab_event_type, state_notify_event_type);
                
                if (motion_event_type == 0 && button_press_event_type == 0 &&
                    button_release_event_type == 0 && button_press_grab_event_type == 0 &&
                    state_notify_event_type == 0) {
                    fprintf(stderr, "Failed to register any events\n");
                    XCloseDevice(display, dialbox);
                    dialbox = NULL;
                } else {
                    XSelectExtensionEvent(display, win, event_classes, 5);
                    XSelectExtensionEvent(display, root, event_classes, 5);
                }
            } else {
                fprintf(stderr, "Failed to open device: %s\n", devices[i].name);
            }
        }
    }

    if (dialbox && win != None) {
        XQueryTree(display, win, &root, &parent, &children, &nchildren);
        printf("Window hierarchy: win=%ld, parent=%ld, root=%ld\n", win, parent, root);
        if (children) XFree(children);
        if (parent != 0 && parent != root) {
            XSelectExtensionEvent(display, parent, event_classes, 5);
        }
    }

    XFreeDeviceList(devices);
    return dialbox;
}

/* Main function */
int main(int argc, char *argv[])
{
    int event_base;
    int error_base;
    int major_opcode;
    int i;
    XEvent event;
    XDeviceMotionEvent *motion;
    XDeviceButtonEvent *button;
    XDeviceStateNotifyEvent *state;
    Window root;
    XSetWindowAttributes attrs;
    XWMHints wmhints;
    XSizeHints size_hints;
    XTextProperty window_name;

    /* Initialize arrays */
    for (i = 0; i < 8; i++) {
        dials[i] = 0;
        dial_resolutions[i] = 0;
    }
    for (i = 0; i < 32; i++) {
        buttons[i] = 0;
    }

    /* Set error handler */
    XSetErrorHandler(x_error_handler);

    /* Open display */
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return 1;
    }

    /* Create window */
    root = RootWindow(dpy, DefaultScreen(dpy));
    attrs.event_mask = FocusChangeMask | StructureNotifyMask | ExposureMask | ButtonPressMask;
    attrs.background_pixel = BlackPixel(dpy, DefaultScreen(dpy));
    window = XCreateWindow(dpy, root, 0, 0, 200, 200, 0,
                           CopyFromParent, InputOutput, CopyFromParent,
                           CWEventMask | CWBackPixel, &attrs);
    printf("Window ID: %ld\n", window);

    /* Set WM properties */
    wmhints.flags = InputHint | StateHint;
    wmhints.input = True;
    wmhints.initial_state = NormalState;
    size_hints.flags = PSize | PMinSize;
    size_hints.width = 200;
    size_hints.height = 200;
    size_hints.min_width = 200;
    size_hints.min_height = 200;
    XStringListToTextProperty(&argv[0], 1, &window_name);
    XSetWMProperties(dpy, window, &window_name, NULL, argv, argc, &size_hints, &wmhints, NULL);
    XFree(window_name.value);

    /* Map window */
    XMapWindow(dpy, window);
    XFlush(dpy);

    /* Check for XInput extension */
    if (!XQueryExtension(dpy, "XInputExtension", &major_opcode, &event_base, &error_base)) {
        fprintf(stderr, "XInput extension not available\n");
        XCloseDisplay(dpy);
        return 1;
    }
    printf("XInput extension found, event base: %d\n", event_base);
    xinput_event_base = event_base;

    /* Find and configure Dialbox */
    dialbox_device = find_dialbox(dpy, window);
    if (!dialbox_device) {
        fprintf(stderr, "No Dialbox device found or failed to configure\n");
        XCloseDisplay(dpy);
        return 1;
    }

    printf("Dialbox configured. Rotate dials or press buttons to test (click window to focus). Press Ctrl+C to exit.\n");

    /* Event loop */
    while (1) {
        XNextEvent(dpy, &event);
        printf("Event received: type=%d\n", event.type);
        if (event.type == MapNotify && event.xmap.window == window) {
            printf("Window mapped, setting focus\n");
            XSetInputFocus(dpy, window, RevertToParent, CurrentTime);
        } else if (event.type == motion_event_type) {
            motion = (XDeviceMotionEvent *)&event;
            printf("Motion event: device_id=%ld, first_axis=%d, axes_count=%d\n",
                   motion->deviceid, motion->first_axis, motion->axes_count);
            for (i = 0; i < motion->axes_count && i < 8; i++) {
                int dial = motion->first_axis + i;
                int raw_value = motion->axis_data[i - motion->first_axis];
                int degrees = normalize_dial_angle(dial, raw_value);
                dials[dial] = degrees;
                printf("Dial %d: %d (raw value), %d degrees\n", dial + 1, raw_value, degrees);
            }
        } else if (event.type == button_press_event_type || event.type == button_press_grab_event_type) {
            button = (XDeviceButtonEvent *)&event;
            if (button->button <= 32) {
                buttons[button->button - 1] = 1; /* GLUT_DOWN */
                printf("Button %d pressed\n", button->button);
            }
        } else if (event.type == button_release_event_type) {
            button = (XDeviceButtonEvent *)&event;
            if (button->button <= 32) {
                buttons[button->button - 1] = 0; /* GLUT_UP */
                printf("Button %d released\n", button->button);
            }
        } else if (event.type == state_notify_event_type) {
            state = (XDeviceStateNotifyEvent *)&event;
            printf("State notify: device_id=%ld, time=%ld\n", state->deviceid, state->time);
        } else if (event.type >= xinput_event_base) {
            printf("Unknown XInput event: type=%d\n", event.type);
            for (i = 0; i < sizeof(XEvent); i++) {
                printf("Byte %d: %02x\n", i, ((unsigned char *)&event)[i]);
            }
        } else if (event.type == FocusIn) {
            printf("Window gained focus\n");
        } else if (event.type == FocusOut) {
            printf("Window lost focus\n");
        }
    }

    /* Cleanup */
    if (dialbox_device) {
        XCloseDevice(dpy, dialbox_device);
    }
    XCloseDisplay(dpy);
    return 0;
}
