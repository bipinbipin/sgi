/*_____________________________________________________________________________
 *
 * File:    example1.c++
 * Date:    3/1/93
 * Author:  Ashmeet Sidana
 * Contact: as@sgi.com
 *
 * $Id: example.c++,v 1.2 1995/08/30 07:58:05 sporter Exp $
 *_____________________________________________________________________________
 */

#include <stdlib.h>
#include <stdio.h>

#include <Xm/Xm.h>
#include <Xm/RowColumn.h>
#include <Xm/MainW.h>
#include <Xm/PushBG.h>
#include <Xm/CascadeB.h>
#include <Xm/SeparatoG.h>
#include <Xm/Form.h>

#include "TransportButton.h"
#include "play.xpm"

TransportButton* transportButton;
    

void TestCallback(Widget w, XtPointer clientData, XtPointer callData) 
{
    TransportButton* obj = (TransportButton*)clientData;
    if (obj->selected())
	obj->deselect();
    else
	obj->select();
}


/*_____________________________________________________________________________
 *
 * main()
 *_____________________________________________________________________________
 */
void main(int argc, char **argv)
{
    Widget          toplvl,mainWindow;
    Widget          menubar,pull1,cascade1,pbut1[3],workarea;
    Arg             args[5];
    XtAppContext    app_con;
    void            FileExit();
    void            DummyCB();
    int i;
    

/*
 * initialize Xt intrinics.
 */

    toplvl=XtAppInitialize(&app_con,"Example",NULL,0,&argc,argv,NULL,NULL,0);


/*
 * set up a simple motif interface with a small menu.
 */
    mainWindow = XmCreateMainWindow(toplvl, "main", NULL, 0);
    XtManageChild (mainWindow);
    
    menubar = XmCreateMenuBar (mainWindow, "menubar", NULL, 0);
    XtManageChild (menubar);

    pull1 = XmCreatePulldownMenu(menubar, "pull1", NULL, 0);
    XtSetArg(args[0], XmNsubMenuId, pull1);
    cascade1 = XmCreateCascadeButton(menubar, "File", args, 1);
    XtManageChild (cascade1);
    pbut1[0] = XmCreatePushButtonGadget (pull1, "Exit", NULL, 0);
    XtAddCallback (pbut1[0],XmNactivateCallback,(XtCallbackProc)FileExit,NULL);
    XtManageChildren (pbut1, 1);

    i=0;
    //XtSetArg (args[i],XmNwidth,500);i++;    
    //XtSetArg (args[i],XmNheight,500);i++;
    XtManageChild(workarea=XmCreateBulletinBoard(mainWindow,"workarea",
						 args,i));
    XmMainWindowSetAreas(mainWindow,menubar,NULL,NULL, NULL,workarea);

    transportButton = new TransportButton("testButton", workarea, playPixmap, TRUE);
    XtVaSetValues(transportButton->baseWidget(), XmNlabelString, XmStringCreateSimple("Test"), NULL);
   
    XtAddCallback(transportButton->baseWidget(),
		  XmNarmCallback,
		  &TestCallback,
		  (XtPointer)transportButton ); 

    transportButton->show();


/*
 * realize all widgets and start event loop
 */
    XtRealizeWidget(toplvl);
    XtAppMainLoop(app_con);

    exit(0);
}

void FileExit()
{
    exit(0);
}
