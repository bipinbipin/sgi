/*------------------------------*\
|* include
\*------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/Protocols.h>
#include <Xm/MwmUtil.h>
#include <Xm/DialogS.h>
#include <Xm/PanedW.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/PushB.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/gls.h>
#include <GL/GLwMDrawA.h>
#include "glsplay.h"

/*------------------------------*\
|* local
\*------------------------------*/
static Widget wParent, wWindow;
static Widget wFormControl, wFormAction;
static Widget wFrame, wPicture;
static Widget wDone;
static char fileName[256];
static int attribs[] = {GLX_RGBA, 0};
static Display *display;
static int screen;
static XVisualInfo *vi;
static GLXContext glx;

/*------------------------------*\
|* Viewport2
\*------------------------------*/
static void Viewport2(GLint x, GLint y, GLsizei width, GLsizei height)
{
    Dimension w, h;

    XtVaGetValues(wFormControl, XmNwidth, &w, XmNheight, &h, NULL);
    glViewport(x, y, (GLint)w, (GLint)h);
}

/*------------------------------*\
|* Draw2
\*------------------------------*/
static void Draw2(Widget w, XtPointer client, XtPointer info)
{
    GLuint gls;

    glXMakeCurrent(display, XtWindow(wPicture), glx);
    gls = glsGenContext();
    glsContext(gls);
    glsCommandFunc(GLS_CALL_FUNC, GLS_OP_glViewport, (GLSfunc)Viewport2);
    glsCallStream(fileName);
    glFlush();
    glsDeleteContext(gls);
}

/*------------------------------*\
|* Viewport1
\*------------------------------*/
static void Viewport1(GLint x, GLint y, GLsizei width, GLsizei height)
{

    glViewport(x, y, 300, 300);
}

/*------------------------------*\
|* Draw1
\*------------------------------*/
static void Draw1(Widget w, XtPointer client, XtPointer info)
{
    GLuint gls;

    glXMakeCurrent(display, XtWindow(wPicture), glx);
    gls = glsGenContext();
    glsContext(gls);
    glsCommandFunc(GLS_CALL_FUNC, GLS_OP_glViewport, (GLSfunc)Viewport1);
    glsCallStream(fileName);
    glFlush();
    glsDeleteContext(gls);
    XtRemoveAllCallbacks(wPicture, XmNexposeCallback);
    XtAddCallback(wPicture, XmNexposeCallback, (XtCallbackProc)Draw2, NULL);
}

/*------------------------------*\
|* Done
\*------------------------------*/
static void Done(Widget w, XtPointer client, XtPointer info)
{

    glXDestroyContext(display, glx);
    XtDestroyWidget((Widget)client);
    exit(1);
}

/*------------------------------*\
|* MinSize
\*------------------------------*/
static void MinSize(Widget w, XtPointer client, XtPointer info)
{
    XConfigureEvent *ptr = (XConfigureEvent *)info;
    Dimension x;

    if (ptr->type == ConfigureNotify) {
	XtVaGetValues((Widget)client, XmNheight, &x, NULL);
	XtVaSetValues((Widget)client, XmNpaneMaximum, x, XmNpaneMinimum, x,
		      NULL);
	XtRemoveEventHandler(w, StructureNotifyMask, False,
			     (XtEventHandler)MinSize, (caddr_t)client);
    }
}

/*------------------------------*\
|* Die
\*------------------------------*/
static void Die(Widget w, XtPointer client, XtPointer info)
{

    exit(1);
}

/*------------------------------*\
|* ShowWindowInit
\*------------------------------*/
void ShowWindowInit(char *name)
{
    Atom atom;

    strcpy(fileName, name);

    display = XtDisplay(GLSwidget);
    screen = DefaultScreen(display);
    if ((vi = glXChooseVisual(display, screen, attribs)) == NULL)  {
	return;
    }
    if ((glx = glXCreateContext(display, vi, 0, GL_TRUE)) == NULL) {
	return;
    }

    wParent = XtVaCreatePopupShell(
	"shell", xmDialogShellWidgetClass, GLSwidget,
	XmNtitle, "Show Panel",
	NULL
    );
    wWindow = XtVaCreateWidget(
	"window", xmPanedWindowWidgetClass, wParent,
	XmNsashWidth, 1,
	XmNsashHeight, 1,
	XmNseparatorOn, False,
	NULL
    );

    wFormControl = XtVaCreateManagedWidget(
	"form", xmFormWidgetClass, wWindow,
	XmNwidth, 300,
	XmNheight, 300,
	NULL
    );
    wFrame = XtVaCreateManagedWidget(
        "frame", xmFrameWidgetClass, wFormControl,
	XmNtopAttachment, XmATTACH_FORM,
	XmNbottomAttachment, XmATTACH_FORM,
	XmNleftAttachment, XmATTACH_FORM,
	XmNrightAttachment, XmATTACH_FORM,
	NULL
    );
    wPicture = XtVaCreateManagedWidget(
	"picture", glwMDrawingAreaWidgetClass, wFrame,
	GLwNvisualInfo, vi,
	NULL
    );

    wFormAction = XtVaCreateManagedWidget(
	"form", xmFormWidgetClass, wWindow,
        XmNfractionBase, 3,
	NULL
    );
    wDone = XtVaCreateManagedWidget(
        " Done ", xmPushButtonWidgetClass, wFormAction,
        XmNtopAttachment, XmATTACH_FORM,
        XmNtopOffset, 3,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNbottomOffset, 3,
        XmNleftAttachment, XmATTACH_POSITION,
        XmNleftPosition, 1,
        XmNrightAttachment, XmATTACH_POSITION,
        XmNrightPosition, 2,
        NULL
    );

    XtAddEventHandler(wParent, StructureNotifyMask, False,
                      (XtEventHandler)MinSize, (caddr_t)wFormAction);

    atom = XmInternAtom(XtDisplay(wParent), "WM_DELETE_WINDOW", False);
    XmAddWMProtocolCallback(wParent, atom, Die, NULL);
    atom = XmInternAtom(XtDisplay(wParent), "_WM_QUIT_APP", False);
    XmAddWMProtocolCallback(wParent, atom, Die, NULL);

    XtAddCallback(wPicture, XmNexposeCallback, (XtCallbackProc)Draw1, NULL);
    XtAddCallback(wDone, XmNactivateCallback, (XtCallbackProc)Done, wParent);

    XtManageChild(wWindow);
}

/*------------------------------*\
|* bottom
\*------------------------------*/
