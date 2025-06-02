

/* $Revision: 1.1 $ */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xmd.h>

extern "C" {

/* Transparent type values */
/*      None                  0 */
#define TransparentPixel      1
#define TransparentMask       2

/* layered visual info template flags */
#define VisualLayerMask		0x200
#define VisualTransparentType	0x400
#define VisualTransparentValue	0x800
#define VisualAllLayerMask	0xFFF

/* layered visual info structure */
typedef struct _XLayerVisualInfo {
   XVisualInfo vinfo;
   int layer;
   int type;
   unsigned long value;
} XLayerVisualInfo;

/* SERVER_OVERLAY_VISUALS property element */
typedef struct _OverlayInfo {
   CARD32  overlay_visual;
   CARD32  transparent_type;
   CARD32  value;
   CARD32  layer;
} OverlayInfo;

XLayerVisualInfo *
XGetLayerVisualInfo(Display *display, long lvinfo_mask, 
		    XLayerVisualInfo *lvinfo_template, 
		    int *nitems_return);

Status
XMatchLayerVisualInfo(Display *display, int screen, int depth, 
		      int nClass, int layer, XLayerVisualInfo *lvinfo_return);
}
