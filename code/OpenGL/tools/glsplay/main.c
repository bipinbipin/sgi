/*------------------------------*\
|* include
\*------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <Xm/Xm.h>
#include <Xm/Protocols.h>
#include <Xm/MwmUtil.h>
#include <Xm/DialogS.h>
#include <Xm/FileSB.h>
#include <Xm/PanedW.h>
#include <Xm/Form.h>
#include <Xm/RowColumn.h>
#include <Xm/MessageB.h>
#include <Xm/Text.h>
#include <Xm/List.h>
#include <Xm/PushB.h>
#include <GL/gl.h>
#include "glsplay.h"

/*------------------------------*\
|* global
\*------------------------------*/
XtAppContext GLSapp;
XtInputId GLSinputID;
Widget GLSwidget;

/*------------------------------*\
|* local
\*------------------------------*/
static String fallbacks[] = {
    "glsplay*sgiMode: true",
    "glsplay*useSchemes: all",
    NULL
};

/*------------------------------*\
|* SetFileName
\*------------------------------*/
static void SetFileName(Widget w, XtPointer client, XtPointer info)
{
    char *name;

    if (XmStringGetLtoR(((XmFileSelectionBoxCallbackStruct *)info)->value,
                        XmSTRING_DEFAULT_CHARSET, &name) == True) {
	ShowWindowInit(name);
	XtFree(name);
	XtUnmanageChild(w);
    }
}

/*------------------------------*\
|* Done
\*------------------------------*/
void Done(Widget w, XtPointer client, XtPointer info)
{

    XtUnmanageChild(w);
    exit(1);
}

/*------------------------------*\
|* File
\*------------------------------*/
void File(void)
{
    Atom atom;
    Widget wDialog;

    wDialog = XmCreateFileSelectionDialog(GLSwidget, "dialog", NULL, 0);
    XtVaSetValues(
	wDialog,
	XmNautoUnmanage, False,
	XmNheight, 500,
	XmNresizePolicy, XmRESIZE_GROW,
	XmNwidth, 600,
	NULL
    );
    XtUnmanageChild(XmFileSelectionBoxGetChild(wDialog, XmDIALOG_HELP_BUTTON));
    XtAddCallback(wDialog, XmNokCallback, (XtCallbackProc)SetFileName, NULL);
    XtAddCallback(wDialog, XmNcancelCallback, (XtCallbackProc)Done, NULL);

    XtManageChild(wDialog);
}

/*------------------------------*\
|* main
\*------------------------------*/
main(int argc, char *argv[])
{

    /* init
    \*------------------------------*/
    XtSetLanguageProc(NULL, NULL, NULL);
    GLSwidget = XtVaAppInitialize(&GLSapp, "glsplay", NULL, 0, &argc, argv,
				  fallbacks, NULL);

    /* get file
    \*------------------------------*/
    if (argc >= 2) {
	ShowWindowInit(argv[1]);
    } else {
        File();
    }

    /* go
    \*------------------------------*/
    XtAppMainLoop(GLSapp);
}

/*------------------------------*\
|* bottom
\*------------------------------*/
