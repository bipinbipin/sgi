////////////////////////////////////////////////////////////////////////
//
// VkwindowMainWindow.h
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

#ifndef VKWINDOWMAINWINDOW_H
#define VKWINDOWMAINWINDOW_H
#include <Vk/VkWindow.h>

class VkMenuItem;
class VkMenuToggle;
class VkMenuConfirmFirstAction;
class VkSubMenu;
class VkRadioSubMenu;

//---- Start editable code block: headers and declarations


//---- End editable code block: headers and declarations


//---- VkwindowMainWindow class declaration

class VkwindowMainWindow: public VkWindow { 

  public:

    VkwindowMainWindow( const char * name, 
                        ArgList args = NULL,
                        Cardinal argCount = 0 );
    ~VkwindowMainWindow();
    const char *className();
    virtual Boolean okToQuit();

    //---- Start editable code block: VkwindowMainWindow public


    //---- End editable code block: VkwindowMainWindow public


  protected:



    // Classes created by this class

    class MainForm *_mainForm;


    // Widgets created by this class


    // Menu items created by this class
    VkSubMenu  *_filePane;
    VkMenuItem *_newButton;
    VkMenuItem *_openButton;
    VkMenuItem *_saveButton;
    VkMenuItem *_saveasButton;
    VkMenuItem *_printButton;
    VkMenuItem *_button;
    VkMenuItem *_separator1;
    VkMenuItem *_closeButton;
    VkMenuItem *_exitButton;
    
#ifdef UNUSED
    VkSubMenu  *_editPane;
    VkMenuItem *_undoButton;
    VkMenuItem *_cutButton;
    VkMenuItem *_copyButton;
    VkMenuItem *_pasteButton;
#endif
    
    VkRadioSubMenu*	_videoPane;
    VkMenuToggle*	_videoFramesToggle;
    VkMenuToggle*	_videoEvenFieldsToggle;
    VkMenuToggle*	_videoOddFieldsToggle;
    
    VkSubMenu  *_timing;
    VkMenuToggle *_checkStateToggle;
    VkMenuToggle *_printTimeToggle;
    VkRadioSubMenu *_radioLoop;
    VkMenuToggle *_test1;
    VkMenuToggle *_test10;
    VkMenuToggle *_test100;
    VkMenuToggle *_test1000;

    // Member functions called from callbacks

    virtual void ReloadEffects ( Widget, XtPointer );
    virtual void checkStateChanged ( Widget, XtPointer );
    virtual void close ( Widget, XtPointer );
    virtual void copy ( Widget, XtPointer );
    virtual void cut ( Widget, XtPointer );
    virtual void fieldChanged( Widget, XtPointer );
    virtual void loop100Changed ( Widget, XtPointer );
    virtual void loop10Changed ( Widget, XtPointer );
    virtual void loop1Changed ( Widget, XtPointer );
    virtual void loopChanged ( Widget, XtPointer );
    virtual void newFile ( Widget, XtPointer );
    virtual void openFile ( Widget, XtPointer );
    virtual void paste ( Widget, XtPointer );
    virtual void print ( Widget, XtPointer );
    virtual void printTimeChanged ( Widget, XtPointer );
    virtual void quit ( Widget, XtPointer );
    virtual void save ( Widget, XtPointer );
    virtual void saveas ( Widget, XtPointer );


    //---- Start editable code block: VkwindowMainWindow protected


    //---- End editable code block: VkwindowMainWindow protected


  private:


    // Callbacks to interface with Motif

    static void ReloadEffectsCallback ( Widget, XtPointer, XtPointer );
    static void checkStateChangedCallback ( Widget, XtPointer, XtPointer );
    static void closeCallback ( Widget, XtPointer, XtPointer );
    static void copyCallback ( Widget, XtPointer, XtPointer );
    static void cutCallback ( Widget, XtPointer, XtPointer );
    static void fieldChangedCallback ( Widget, XtPointer, XtPointer );
    static void loop100ChangedCallback ( Widget, XtPointer, XtPointer );
    static void loop10ChangedCallback ( Widget, XtPointer, XtPointer );
    static void loop1ChangedCallback ( Widget, XtPointer, XtPointer );
    static void loopChangedCallback ( Widget, XtPointer, XtPointer );
    static void newFileCallback ( Widget, XtPointer, XtPointer );
    static void openFileCallback ( Widget, XtPointer, XtPointer );
    static void pasteCallback ( Widget, XtPointer, XtPointer );
    static void printCallback ( Widget, XtPointer, XtPointer );
    static void printTimeChangedCallback ( Widget, XtPointer, XtPointer );
    static void quitCallback ( Widget, XtPointer, XtPointer );
    static void saveCallback ( Widget, XtPointer, XtPointer );
    static void saveasCallback ( Widget, XtPointer, XtPointer );

    static String      _defaultVkwindowMainWindowResources[];

    //---- Start editable code block: VkwindowMainWindow private


    //---- End editable code block: VkwindowMainWindow private


};
//---- Start editable code block: End of generated code


//---- End editable code block: End of generated code

#endif
