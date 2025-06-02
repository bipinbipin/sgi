/* utility routines for GLX.
 * Since these routines are used both in the Motif and X versions of
 * the widget, they cannot peek directly into the structure
 */
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <Xm/PrimitiveP.h>
#include <X11/GLw/GLwDrawAP.h>

#if XmVERSION > 1
#define GLwDrawingAreaMakeCurrent   GLwDrawingArea21MakeCurrent
#define GLwDrawingAreaSwapBuffers   GLwDrawingArea21SwapBuffers
#endif

void GLwDrawingAreaMakeCurrent (Widget w, GLXContext ctx)
{
    glXMakeCurrent (XtDisplay(w), XtWindow(w), ctx);
}

void GLwDrawingAreaSwapBuffers (Widget w)
{
    glXSwapBuffers (XtDisplay(w), XtWindow(w));
}
