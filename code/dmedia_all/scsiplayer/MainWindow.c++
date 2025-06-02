 //////////////////////////////////////////////////////////////
//
// Source file for MainWindow
//
//    This class is a ViewKit VkWindow subclass.  It contains 
//    the main window plus menu items.
//
//////////////////////////////////////////////////////////////

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <X11/Xlib.h>
#include <Vk/VkApp.h>
#include <Vk/VkFileSelectionDialog.h>
#include <Vk/VkErrorDialog.h>
#include <Vk/VkInfoDialog.h>
#include <Vk/VkResource.h>
#include <Vk/VkMenu.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkMenuItem.h>
#include "PlayerPanel.h"
#include "ScsiPlayerApp.h"
#include "PlayerState.h"
#include "MainWindow.h"

const int MainWindow::PLAYER_WIDTH = 468;
const int MainWindow::PLAYER_HEIGHT = 154;
const int MainWindow::TERSE_WIDTH = 220;
const int MainWindow::TERSE_HEIGHT = 130;
const int MainWindow::PROGRAM_WIDTH = PLAYER_WIDTH;
const int MainWindow::PROGRAM_HEIGHT = 800;

extern ScsiPlayerApp* theScsiPlayerApp;

MainWindow* theMainWindow;

//#define DBG(x) {x;}
#define DBG(x) {}  

//---- Constructor:  This doesn't call the VkWindow method because we change the
// hierarchy by putting the menu bar inside a form.

MainWindow::MainWindow( const char *name, ArgList /* args */,
			Cardinal /* argCount */) : VkWindow (name) 
{
  theMainWindow = this;

  // Load any class-default resources for this object

  _originalWidth = 0;

  // The filePane menu pane

  _filePane =  addMenuPane("filePane");

  _saveTrackAsItem =  _filePane->addAction ( "saveTrackAsItem", 
					     &MainWindow::saveTrackAsCallback,
					     (XtPointer) this );

  _saveSelectionAsItem = _filePane->addAction ( "saveSelectionAsItem", 
					  &MainWindow::saveSelectionAsCallback,
					  (XtPointer) this );

  _saveSelectionAsItem->deactivate();

  _ejectItem =  _filePane->addAction ( "ejectItem", 
					 &MainWindow::ejectCallback, 
					 (XtPointer) this );

  _separator1 =  _filePane->addSeparator();

  _exitItem =  _filePane->addAction ( "exitItem", 
                                        &MainWindow::quitCallback, 
                                        (XtPointer) this );

  // The editPane menu pane

  _editPane =  addMenuPane("editPane");

  _copySelectionItem =  _editPane->addAction("copyItem", 
					     &MainWindow::copyCallback, 
					     (XtPointer) this );
  _selectAllItem =  _editPane->addAction("selectAllItem", 
					   &MainWindow::selectAllCallback, 
					   (XtPointer) this );
  _selectNoneItem =  _editPane->addAction("selectNoneItem", 
					   &MainWindow::selectNoneCallback, 
					   (XtPointer) this );
  _extendSelectionRightItem =  _editPane->addAction("extendSelectionRightItem", 
						     &MainWindow::extendSelectionRightCallback, 
						     (XtPointer) this );
  _extendSelectionLeftItem =  _editPane->addAction("extendSelectionLeftItem", 
						   &MainWindow::extendSelectionLeftCallback, 
						   (XtPointer) this );

  // The viewPane menu pane

  _viewPane =  addMenuPane("viewPane");

  _trackListView = _viewPane->addToggle("trackList",
					&MainWindow::trackListViewCallback ,
					(XtPointer) this,
					(theScsiPlayerApp->_displayTrackList));

  // The optionsPane menu pane

  _optionsPane =  addMenuPane("optionsPane");


  if (theScsiPlayerApp->scsiDevice() == CdDev) {

    _headphonesItem = _optionsPane->addAction("headphones",
					      &MainWindow::headphonesCallback, NULL);
  }

  if (theScsiPlayerApp->scsiDevice() == DatDev) {
  _scanDatItem = _optionsPane->addAction("scanDat", &MainWindow::scanDatCallback, NULL);
  _rescanDatItem = _optionsPane->addAction("rescanDat", &MainWindow::rescanDatCallback,
					   (XtPointer) this);
  }


  _repeatModeSubMenu = _optionsPane->addRadioSubmenu("repeatOptionsSubMenu");
  
  _noRepeatItem =
    _repeatModeSubMenu->addToggle("noRepeatToggle", 
				    &MainWindow::changeToNoRepeat, 
				    (XtPointer) this,
				    True );
  _repeatCdItem =
    _repeatModeSubMenu->addToggle("repeatCdToggle", 
				    &MainWindow::changeToRepeatCd, 
				    (XtPointer) this );
  _repeatTrackItem =
    _repeatModeSubMenu->addToggle("repeatTrackToggle", 
				    &MainWindow::changeToRepeatTrack, 
				    (XtPointer) this );   
  
  _optionsPane->addSeparator();

  _loopSelectionToggle = _optionsPane->addToggle("loopSelection",
						 &MainWindow::toggleLoopSelection,
						 NULL, 0);

  _playSelectionToggle = _optionsPane->addToggle("playSelection",
						 &MainWindow::togglePlaySelection,
						 this,
						 theScsiPlayerApp->_playSelection);

  // Create the form which constrains the entire app

  _playerPanel= new PlayerPanel("PlayerPanel",mainWindowWidget());

  XtVaSetValues(mainWindowWidget(), XmNvisualPolicy, XmVARIABLE,
		XmNresizePolicy, XmRESIZE_ANY, 
		//XmNtraversalOn, False,
		NULL);

  // Add the component as the main view

  addView (_playerPanel);

  switch (theScsiPlayerApp->scsiDevice()) {
  case CdDev:
    setTitle("CDplayer");
    setIconName("CDplayer");
    break;
  case DatDev:
    setTitle("DATplayer");
    setIconName("DATplayer");
    break;
  }
} // End Constructor


MainWindow::~MainWindow()
{
  if (theScsiPlayerApp->player())
    delete theScsiPlayerApp->player();
  delete _playerPanel;
} 


const char *MainWindow::className()
{
    return ("MainWindow");
} 


Boolean MainWindow::okToQuit()
{

    // This member function is called when the user quits by calling
    // theApplication->terminate() or uses the window manager close protocol
    // This function can abort the operation by returning FALSE, or do some.
    // cleanup before returning TRUE. The actual decision is normally passed on
    // to the view object


    // Query the view object, and give it a chance to cleanup

    return ( _playerPanel->okToQuit() );

}

void MainWindow::stateChange(ScsiPlayer* player)
{
    _playerPanel->stateChange(player);

} 


void MainWindow::clearDisplay()
{
  _playerPanel->clearDisplay();
} 

/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 


void MainWindow::saveSelectionAsCallback(Widget, XtPointer, XtPointer) 
{ 
  if (!theScsiPlayerApp->player()->_selectionEnd){
    theErrorDialog->postAndWait(VkGetResource("ScsiPlayer*noSelectionMsg",
					      XmCString));	    
				//fprintf(stderr,"No Selection.\n");
    return;
  }

  if(theFileSelectionDialog->postAndWait() == VkDialogManager::OK)
    theScsiPlayerApp->player()->saveSelectionAs(theFileSelectionDialog->fileName());
}

void MainWindow::saveTrackAsCallback( Widget /* w */, XtPointer /* clientData */,
				  XtPointer /* callData */ ) 
{ 
  if(theFileSelectionDialog->postAndWait() == VkDialogManager::OK)
    theScsiPlayerApp->player()->saveTrackAs(theFileSelectionDialog->fileName());
}

void MainWindow::quitCallback(Widget  /* w */, XtPointer /* clientData */,
			      XtPointer /* callData */ ) 
{ 
  theApplication->quitYourself();

}


void MainWindow::ejectCallback(Widget /* w */,
			       XtPointer			      /* clientData */,
			       XtPointer							      /* callData */ ) 
{
  theScsiPlayerApp->eject();
// if (theScsiPlayerApp->player()) {
//     theScsiPlayerApp->player()->stop();
//     theScsiPlayerApp->stateChange();
//   }
//   else 
//     ForkEject();

}


void MainWindow::changeToNoRepeat(Widget /* w */, XtPointer /* clientData */,
				  XtPointer				 /* callData */ )
{
  theScsiPlayerApp->_repeatMode = NoRepeat;
}


void MainWindow::changeToRepeatCd(Widget /* w */, XtPointer /* clientData */, 
				  XtPointer				 /* callData */ )
{
  theScsiPlayerApp->_repeatMode = RepeatAlways;
} 


void MainWindow::changeToRepeatTrack(Widget /* w */, XtPointer /* clientData */,
				     XtPointer				    /* callData */ )
{
  theScsiPlayerApp->_repeatMode =  RepeatTrack;
} 

void MainWindow::playerOnlyView()
{
  theScsiPlayerApp->_displayTrackList = False;
  Dimension width;
  XtVaGetValues(baseWidget(), XmNwidth, &_originalWidth, NULL);
  XtVaGetValues(_playerPanel->_controlPanel, XmNwidth, &width, NULL);
  XtVaSetValues(baseWidget(), XmNwidth, width+10, NULL);
}

void MainWindow::trackListViewCallback(Widget w, XtPointer  clientData,
				       XtPointer			/* callData */ )
{
  Boolean areWeOn;
  MainWindow* obj = (MainWindow*)clientData;
  
  XtVaGetValues(w, XmNset, &areWeOn, NULL);
  if (areWeOn && (obj->_originalWidth != 0)) {
    theScsiPlayerApp->_displayTrackList = True;
    XtVaSetValues(obj->baseWidget(), XmNwidth, obj->_originalWidth, NULL);
  }
  else
    obj->playerOnlyView();
  theScsiPlayerApp->updateViewSetting();
} 


void MainWindow::toggleLoopSelection(Widget w, XtPointer, XtPointer)
{
  Boolean areWeOn;
  XtVaGetValues(w, XmNset, &areWeOn, NULL);
  theScsiPlayerApp->_loopSelection =  areWeOn;
} 

void MainWindow::togglePlaySelection(Widget w, XtPointer, XtPointer)
{
  Boolean areWeOn;
  XtVaGetValues(w, XmNset, &areWeOn, NULL);
  theScsiPlayerApp->_playSelection =  areWeOn;
  theScsiPlayerApp->updatePlaySelectionSetting();
} 

void MainWindow::scanDatCallback(Widget , XtPointer, XtPointer)
{
  theScsiPlayerApp->player()->scanDat(theScsiPlayerApp->transport()->lastTrack+1);
} 

void MainWindow::rescanDatCallback(Widget , XtPointer,
				   XtPointer			/* clientData */)
{
  theScsiPlayerApp->player()->scanDat(1);
} 

void MainWindow::copyCallback(Widget , XtPointer, XtPointer)
{
  theScsiPlayerApp->initCutPaste();
  theScsiPlayerApp->xcopy();
} 

void MainWindow::selectAllCallback(Widget , XtPointer, XtPointer)
{
  theMainWindow->_playerPanel->selectAll();
} 

void MainWindow::selectNoneCallback(Widget , XtPointer, XtPointer)
{
  theMainWindow->_playerPanel->selectNone();
} 

void MainWindow::extendSelectionRightCallback(Widget , XtPointer, XtPointer)
{
  theMainWindow->_playerPanel->extendSelection(True);
} 

void MainWindow::extendSelectionLeftCallback(Widget , XtPointer, XtPointer)
{
  theMainWindow->_playerPanel->extendSelection(False);
} 


void MainWindow::headphonesCallback(Widget , XtPointer, XtPointer)
{
  if (theInfoDialog->postAndWait(VkGetResource("ScsiPlayer*headphonesMsg",
					       XmCString),
				 True, True)
      == VkDialogManager::OK) {
    theScsiPlayerApp->player()->stop();   
    theScsiPlayerApp->player()->closeDevice();

    if (execl("/usr/bin/X11/cdheadphone", "cdheadphone", NULL))
      perror("Attempt to launch /usr/bin/X11/cdheadphone failed.");
  }
} 

