////////////////////////////////////////////////////////////////////////
//
// VkwindowMainWindow.c++
//	
//	The handling for the UI of fxview.
//
// Copyright 1996, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//
////////////////////////////////////////////////////////////////////////

#include "VkwindowMainWindow.h"
#include <Vk/VkApp.h>
#include <Vk/VkFileSelectionDialog.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkRadioSubMenu.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkResource.h>

#include <Xm/ToggleB.h>

#include <assert.h>

// Externally defined classes referenced by this class: 

#include "MainForm.h"

extern void VkUnimplemented ( Widget, const char * );

//---- Start editable code block: headers and declarations


//---- End editable code block: headers and declarations



// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  VkwindowMainWindow::_defaultVkwindowMainWindowResources[] = {
        "*button.accelerator:  Ctrl<Key>R",
        "*button.acceleratorText:  Ctrl+R",
        "*button.labelString:  Reload Effects",
        "*button.mnemonic:  R",
	"*checkState.labelString: Check OpenGL State",
        "*closeButton.accelerator:  Ctrl<Key>W",
        "*closeButton.acceleratorText:  Ctrl+W",
        "*closeButton.labelString:  Close",
        "*closeButton.mnemonic:  C",
        "*copyButton.accelerator:  Ctrl<Key>C",
        "*copyButton.acceleratorText:  Ctrl+C",
        "*copyButton.labelString:  Copy",
        "*copyButton.mnemonic:  C",
        "*cutButton.accelerator:  Ctrl<Key>X",
        "*cutButton.acceleratorText:  Ctrl+X",
        "*cutButton.labelString:  Cut",
        "*cutButton.mnemonic:  t",
        "*editPane.labelString:  Edit",
        "*editPane.mnemonic:  E",
	"*evenFields.labelString: Even field dominance",
        "*exitButton.accelerator:  Ctrl<Key>Q",
        "*exitButton.acceleratorText:  Ctrl+Q",
        "*exitButton.labelString:  Exit",
        "*exitButton.mnemonic:  x",
        "*filePane.labelString:  File",
        "*filePane.mnemonic:  F",
	"*frames.labelString: Frames",
        "*helpPane.labelString:  Help",
        "*helpPane.mnemonic:  H",
        "*help_click_for_help.accelerator:  Shift<Key>F1",
        "*help_click_for_help.acceleratorText:  Shift+F1",
        "*help_click_for_help.labelString:  Click For Help",
        "*help_click_for_help.mnemonic:  C",
        "*help_index.labelString:  Index",
        "*help_index.mnemonic:  I",
        "*help_keys_and_short.labelString:  Keys and Shortcuts",
        "*help_keys_and_short.mnemonic:  K",
        "*help_overview.labelString:  Overview",
        "*help_overview.mnemonic:  O",
        "*help_prod_info.labelString:  Product Information",
        "*help_prod_info.mnemonic:  P",
        "*newButton.accelerator:  Ctrl<Key>N",
        "*newButton.acceleratorText:  Ctrl+N",
        "*newButton.labelString:  New",
        "*newButton.mnemonic:  N",
	"*oddFields.labelString: Odd field dominance",
        "*openButton.accelerator:  Ctrl<Key>O",
        "*openButton.acceleratorText:  Ctrl+O",
        "*openButton.labelString:  Open...",
        "*openButton.mnemonic:  O",
        "*pasteButton.accelerator:  Ctrl<Key>V",
        "*pasteButton.acceleratorText:  Ctrl+V",
        "*pasteButton.labelString:  Paste",
        "*pasteButton.mnemonic:  P",
        "*printButton.accelerator:  Ctrl<Key>P",
        "*printButton.acceleratorText:  Ctrl+P",
        "*printButton.labelString:  Print",
        "*printButton.mnemonic:  P",
	"*printTime.labelString: Print Times",
        "*saveButton.accelerator:  Ctrl<Key>S",
        "*saveButton.acceleratorText:  Ctrl+S",
        "*saveButton.labelString:  Save",
        "*saveButton.mnemonic:  S",
        "*saveasButton.labelString:  Save As...",
        "*saveasButton.mnemonic:  A",
	"*loop.labelString: Number Of Loops",
        "*test1.labelString:  1 Loop",
        "*test10.labelString:  10 Loops",
        "*test100.labelString:  100 Loops",
        "*test1000.labelString:  1000 Loops",
        "*timing.labelString:  Timing",
        "*timing.mnemonic:  T",
        "*title:  Fxview",
        "*undoButton.accelerator:  Ctrl<Key>Z",
        "*undoButton.acceleratorText:  Ctrl+Z",
        "*undoButton.labelString:  Undo",
        "*undoButton.mnemonic:  U",
	"*videoPane.labelString: Video",
	"*videoPane.mnemonic: V",
        (char*)NULL
};


//---- Class declaration

VkwindowMainWindow::VkwindowMainWindow( const char *name,
                                        ArgList args,
                                        Cardinal argCount) : 
                                  VkWindow (name, args, argCount) 
{
    // Load any class-default resources for this object

    setDefaultResources(baseWidget(), _defaultVkwindowMainWindowResources  );


    // Create the view component contained by this window

    _mainForm= new MainForm("mainForm",mainWindowWidget());


    XtVaSetValues ( _mainForm->baseWidget(),
                    XmNwidth, 788, 
                    XmNheight, 498, 
                    (XtPointer) NULL );

    // Add the component as the main view

    addView (_mainForm);
    _mainForm->setParent(this);

    // Create the panes of this window's menubar. The menubar itself
    //  is created automatically by ViewKit


    // The filePane menu pane

    _filePane =  addMenuPane("filePane");

#ifdef UNUSED
    _newButton =  _filePane->addAction ( "newButton", 
                                        &VkwindowMainWindow::newFileCallback, 
                                        (XtPointer) this );

    _openButton =  _filePane->addAction ( "openButton", 
                                        &VkwindowMainWindow::openFileCallback, 
                                        (XtPointer) this );

    _saveButton =  _filePane->addAction ( "saveButton", 
                                        &VkwindowMainWindow::saveCallback, 
                                        (XtPointer) this );

    _saveasButton =  _filePane->addAction ( "saveasButton", 
                                        &VkwindowMainWindow::saveasCallback, 
                                        (XtPointer) this );

    _printButton =  _filePane->addAction ( "printButton", 
                                        &VkwindowMainWindow::printCallback, 
                                        (XtPointer) this );
#endif
    
    _button =  _filePane->addAction ( "button", 
                                        &VkwindowMainWindow::ReloadEffectsCallback, 
                                        (XtPointer) this );

#ifdef UNUSED
    _separator1 =  _filePane->addSeparator();

    _closeButton =  _filePane->addAction ( "closeButton", 
                                        &VkwindowMainWindow::closeCallback, 
                                        (XtPointer) this );
#endif

    _exitButton =  _filePane->addAction ( "exitButton", 
                                        &VkwindowMainWindow::quitCallback, 
                                        (XtPointer) this );

    // The editPane menu pane

#ifdef UNUSED
    _editPane =  addMenuPane("editPane");

    _editPane->add ( theUndoManager );

    _cutButton =  _editPane->addAction ( "cutButton", 
                                        &VkwindowMainWindow::cutCallback, 
                                        (XtPointer) this );

    _copyButton =  _editPane->addAction ( "copyButton", 
                                        &VkwindowMainWindow::copyCallback, 
                                        (XtPointer) this );

    _pasteButton =  _editPane->addAction ( "pasteButton", 
                                        &VkwindowMainWindow::pasteCallback, 
                                        (XtPointer) this );
#endif
    
    // The video menu pane

    _videoPane =  addRadioMenuPane("videoPane");
    
    _videoFramesToggle =
	_videoPane->addToggle(
	    "frames",
	    &VkwindowMainWindow::fieldChangedCallback,
	    (XtPointer) this
	    );
    
    _videoEvenFieldsToggle =
	_videoPane->addToggle(
	    "evenFields",
	    &VkwindowMainWindow::fieldChangedCallback,
	    (XtPointer) this
	    );
    
    _videoOddFieldsToggle =
	_videoPane->addToggle(
	    "oddFields",
	    &VkwindowMainWindow::fieldChangedCallback,
	    (XtPointer) this
	    );
    
    // The timing menu pane

    _timing =  addMenuPane("timing");

    _checkStateToggle =
	_timing->addToggle(
	    "checkState",
	    &VkwindowMainWindow::checkStateChangedCallback,
	    (XtPointer) this
	    );
    
    _printTimeToggle =
	_timing->addToggle(
	    "printTime",
	    &VkwindowMainWindow::printTimeChangedCallback,
	    (XtPointer) this
	    );
    
    _timing->addSeparator();

    _radioLoop = _timing->addRadioSubmenu( "loop" );
    
    _test1 =
	_radioLoop->addToggle(
	    "test1", 
	    &VkwindowMainWindow::loop1ChangedCallback, 
	    (XtPointer) this
	    );

    _test10 = 
	_radioLoop->addToggle(
	    "test10", 
	    &VkwindowMainWindow::loop10ChangedCallback, 
	    (XtPointer) this );

    _test100 = 
	_radioLoop->addToggle(
	    "test100", 
	    &VkwindowMainWindow::loop100ChangedCallback, 
	    (XtPointer) this );

    _test1000 =  
	_radioLoop->addToggle(
	    "test1000", 
	    &VkwindowMainWindow::loopChangedCallback, 
	    (XtPointer) this );

    //
    // Set initial state of toggles
    //

    _videoFramesToggle       ->setVisualState( True );
    _videoOddFieldsToggle    ->setVisualState( False );
    _videoEvenFieldsToggle   ->setVisualState( False );
    _checkStateToggle	     ->setVisualState( True );
    _printTimeToggle 	     ->setVisualState( False );
    _test1		     ->setVisualState( True );
    _test10		     ->setVisualState( False );
    _test100		     ->setVisualState( False );
    _test1000		     ->setVisualState( False );
    

} // End Constructor


VkwindowMainWindow::~VkwindowMainWindow()
{
    delete _mainForm;
    //---- Start editable code block: VkwindowMainWindow destructor


    //---- End editable code block: VkwindowMainWindow destructor
} // End destructor


const char *VkwindowMainWindow::className()
{
    return ("VkwindowMainWindow");
} // End className()


Boolean VkwindowMainWindow::okToQuit()
{
    //---- Start editable code block: VkwindowMainWindow okToQuit



    // This member function is called when the user quits by calling
    // theApplication->terminate() or uses the window manager close protocol
    // This function can abort the operation by returning FALSE, or do some.
    // cleanup before returning TRUE. The actual decision is normally passed on
    // to the view object


    // Query the view object, and give it a chance to cleanup

    return ( _mainForm->okToQuit() );

    //---- End editable code block: VkwindowMainWindow okToQuit
} // End okToQuit()



/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void VkwindowMainWindow::ReloadEffectsCallback ( Widget    w,
                                                 XtPointer clientData,
                                                 XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->ReloadEffects ( w, callData );
}

void VkwindowMainWindow::checkStateChangedCallback ( Widget    w,
                                                 XtPointer clientData,
                                                 XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->checkStateChanged ( w, callData );
}

void VkwindowMainWindow::closeCallback ( Widget    w,
                                         XtPointer clientData,
                                         XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->close ( w, callData );
}

void VkwindowMainWindow::copyCallback ( Widget    w,
                                        XtPointer clientData,
                                        XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->copy ( w, callData );
}

void VkwindowMainWindow::cutCallback ( Widget    w,
                                       XtPointer clientData,
                                       XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->cut ( w, callData );
}

void VkwindowMainWindow::fieldChangedCallback ( Widget    w,
						XtPointer clientData,
						XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->fieldChanged ( w, callData );
}

void VkwindowMainWindow::loop100ChangedCallback ( Widget    w,
                                                 XtPointer clientData,
                                                 XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->loop100Changed ( w, callData );
}

void VkwindowMainWindow::loop10ChangedCallback ( Widget    w,
                                               XtPointer clientData,
                                               XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->loop10Changed ( w, callData );
}

void VkwindowMainWindow::loop1ChangedCallback ( Widget    w,
                                               XtPointer clientData,
                                               XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->loop1Changed ( w, callData );
}

void VkwindowMainWindow::loopChangedCallback ( Widget    w,
                                               XtPointer clientData,
                                               XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->loopChanged ( w, callData );
}

void VkwindowMainWindow::newFileCallback ( Widget    w,
                                           XtPointer clientData,
                                           XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->newFile ( w, callData );
}

void VkwindowMainWindow::openFileCallback ( Widget    w,
                                            XtPointer clientData,
                                            XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->openFile ( w, callData );
}

void VkwindowMainWindow::pasteCallback ( Widget    w,
                                         XtPointer clientData,
                                         XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->paste ( w, callData );
}

void VkwindowMainWindow::printCallback ( Widget    w,
                                         XtPointer clientData,
                                         XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->print ( w, callData );
}

void VkwindowMainWindow::printTimeChangedCallback ( Widget    w,
                                                 XtPointer clientData,
                                                 XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->printTimeChanged ( w, callData );
}

void VkwindowMainWindow::quitCallback ( Widget    w,
                                        XtPointer clientData,
                                        XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->quit ( w, callData );
}

void VkwindowMainWindow::saveCallback ( Widget    w,
                                        XtPointer clientData,
                                        XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->save ( w, callData );
}

void VkwindowMainWindow::saveasCallback ( Widget    w,
                                          XtPointer clientData,
                                          XtPointer callData ) 
{ 
    VkwindowMainWindow* obj = ( VkwindowMainWindow * ) clientData;
    obj->saveas ( w, callData );
}





/////////////////////////////////////////////////////////////// 
// The following functions are called from callbacks 
/////////////////////////////////// 

void VkwindowMainWindow::ReloadEffects ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow ReloadEffects


    // This virtual function is called from ReloadEffectsCallback.
    // By default this member function passes control
    // to the component contained by this window

    _mainForm->ReloadEffects( w, callData);


    //---- End editable code block: VkwindowMainWindow ReloadEffects

} // End VkwindowMainWindow::ReloadEffects()


void VkwindowMainWindow::checkStateChanged ( Widget w, XtPointer callData ) 
{
    _mainForm->checkStateChanged( w, callData);
}


void VkwindowMainWindow::close ( Widget, XtPointer ) 
{
    //---- Start editable code block:  close


    // To close a window, just delete the object
    // checking first with the view object.
    // If this is the last window, ViewKit will exit

    if(okToQuit())
        delete this;

    //---- End editable code block:  close
} // End VkwindowMainWindow::close()

void VkwindowMainWindow::copy ( Widget, XtPointer ) 
{
    //---- Start editable code block: VkwindowMainWindow copy


    // This virtual function is called from copyCallback.
    // By default this member function passes control
    // to the component contained by this window

    _mainForm->copy();


    //---- End editable code block: VkwindowMainWindow copy

} // End VkwindowMainWindow::copy()


void VkwindowMainWindow::cut ( Widget, XtPointer ) 
{
    //---- Start editable code block: VkwindowMainWindow cut


    // This virtual function is called from cutCallback.
    // By default this member function passes control
    // to the component contained by this window

    _mainForm->cut();


    //---- End editable code block: VkwindowMainWindow cut

} // End VkwindowMainWindow::cut()


void VkwindowMainWindow::loop100Changed ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow loop100Changed

    // XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _mainForm->loop100Changed( w, callData);


    //---- End editable code block: VkwindowMainWindow loop100Changed

} // End VkwindowMainWindow::loop100Changed()


void VkwindowMainWindow::fieldChanged ( Widget w, XtPointer ) 
{
    if ( XmToggleButtonGetState(w) )
    {
	if ( w == _videoFramesToggle->baseWidget() )
	{
	    _mainForm->fieldChanged( PROCESS_FRAMES );
	}
	else if ( w == _videoEvenFieldsToggle->baseWidget() )
	{
	    _mainForm->fieldChanged( PROCESS_EVEN_FIELDS );
	}
	else if ( w == _videoOddFieldsToggle->baseWidget() )
	{
	    _mainForm->fieldChanged( PROCESS_ODD_FIELDS );
	}
	else
	{
	    assert( False );
	}
    }
} // End VkwindowMainWindow::loop100Changed()


void VkwindowMainWindow::loop10Changed ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow loop10Changed

    // XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _mainForm->loop10Changed( w, callData);


    //---- End editable code block: VkwindowMainWindow loop10Changed

} // End VkwindowMainWindow::loop10Changed()


void VkwindowMainWindow::loop1Changed ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow loop1Changed

    // XmAnyCallbackStruct *cbs = (XmAnyCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _mainForm->loop1Changed( w, callData);


    //---- End editable code block: VkwindowMainWindow loop1Changed

} // End VkwindowMainWindow::loop1Changed()


void VkwindowMainWindow::loopChanged ( Widget w, XtPointer callData ) 
{
    //---- Start editable code block: VkwindowMainWindow loopChanged

    // XmToggleButtonCallbackStruct *cbs = (XmToggleButtonCallbackStruct*) callData;

    // By default this member function passes control
    // to the component contained by this window

    _mainForm->loopChanged( w, callData);


    //---- End editable code block: VkwindowMainWindow loopChanged

}    // End VkwindowMainWindow::loopChanged()


void VkwindowMainWindow::newFile ( Widget, XtPointer ) 
{
    //---- Start editable code block: VkwindowMainWindow newFile


    // This virtual function is called from newFileCallback.
    // By default this member function passes control
    // to the component contained by this window

    _mainForm->newFile();


    //---- End editable code block: VkwindowMainWindow newFile

} // End VkwindowMainWindow::newFile()


void VkwindowMainWindow::openFile ( Widget, XtPointer ) 
{
    //---- Start editable code block: VkwindowMainWindow openFile

    // This virtual function is called from openFileCallback
    // Use the blocking mode of the file selection
    // dialog to get a file before calling view object's openFile()

    if(theFileSelectionDialog->postAndWait() == VkDialogManager::OK)
    {
        _mainForm->openFile(theFileSelectionDialog->fileName());
    }

    //---- End editable code block: VkwindowMainWindow openFile

}  // End VkwindowMainWindow::openFile()

void VkwindowMainWindow::paste ( Widget, XtPointer ) 
{
    //---- Start editable code block: VkwindowMainWindow paste


    // This virtual function is called from pasteCallback.
    // By default this member function passes control
    // to the component contained by this window

    _mainForm->paste();


    //---- End editable code block: VkwindowMainWindow paste

} // End VkwindowMainWindow::paste()


void VkwindowMainWindow::print ( Widget, XtPointer ) 
{
    //---- Start editable code block:  print


    // This virtual function is called from printCallback
_mainForm->print(NULL);

    //---- End editable code block:  print

}  // End VkwindowMainWindow::print()

void VkwindowMainWindow::printTimeChanged ( Widget w, XtPointer callData ) 
{
    _mainForm->printTimeChanged( w, callData);
}

void VkwindowMainWindow::quit ( Widget, XtPointer ) 
{
    // Exit via quitYourself() to allow the application
    // to go through its normal shutdown routines, checking with
    // all windows, views, and so on.

    theApplication->quitYourself();

}  // End VkwindowMainWindow::quit()

void VkwindowMainWindow::save ( Widget, XtPointer ) 
{
    //---- Start editable code block: VkwindowMainWindow save


    // This virtual function is called from saveCallback.
    // By default this member function passes control
    // to the component contained by this window

    _mainForm->save();


    //---- End editable code block: VkwindowMainWindow save

} // End VkwindowMainWindow::save()


void VkwindowMainWindow::saveas ( Widget, XtPointer ) 
{
    //---- Start editable code block: VkwindowMainWindow saveas


    // This virtual function is called from saveasCallback
    // Use the blocking mode of the file selection
    // dialog to get a file before calling view object's save()

    if(theFileSelectionDialog->postAndWait() == VkDialogManager::OK)
    {
        _mainForm->saveas(theFileSelectionDialog->fileName());

    }

    //---- End editable code block: VkwindowMainWindow saveas

} // End VkwindowMainWindow::saveAs()




//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code


