////////////////////////////////////////////////////////////////////////
//
// MainForm.h
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

#ifndef MAINFORM_H
#define MAINFORM_H

#include "MainFormUI.h"
#include "PlugIns.h"

#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkCutPaste.h>

#include <dmedia/fx_buffer.h>
#include <dmedia/fx_plugin_mgr.h>


#define TRANSITION 1
#define FILTER     2

enum FieldMode
{
    PROCESS_FRAMES,
    PROCESS_EVEN_FIELDS,
    PROCESS_ODD_FIELDS
};

class MainForm : public MainFormUI
{
public:
    MainForm(const char *, Widget);
    ~MainForm();
    const char *  className();
    virtual void setParent(class VkWindow *);
    virtual void ReloadEffects(Widget, XtPointer);
    virtual void checkStateChanged(Widget, XtPointer);
    virtual void copy();
    virtual void cut();
    virtual void fieldChanged( FieldMode );
    virtual void loop1000Changed(Widget, XtPointer);
    virtual void loop100Changed(Widget, XtPointer);
    virtual void loop10Changed(Widget, XtPointer);
    virtual void loop1Changed(Widget, XtPointer);
    virtual void loopChanged(Widget, XtPointer);
    virtual void newFile();
    virtual void openFile(const char *);
    virtual void paste();
    virtual void print(const char *);
    virtual void printTimeChanged(Widget, XtPointer);
    virtual void save();
    virtual void saveas(const char *);

    static VkComponent *CreateMainForm( const char *name, Widget parent ); 

    void clearDestination();

    DMplugmgr * getEffectsManager();

    void listSelect(Widget w, XmListCallbackStruct *);

    void drop(Widget, Atom, XtPointer, unsigned long, int, int);

    DMstatus getFrame(	int frame,
			DMplugintrack track,
			int width, 
			int height, 
			int rowBytes, 
			unsigned char *data);

protected:

    // These functions will be called as a result of callbacks
    // registered in MainFormUI

    virtual void doFilters ( Widget, XtPointer );
    virtual void doFrameSlider ( Widget, XtPointer );
    virtual void doSetup ( Widget, XtPointer );
    virtual void doTransitions ( Widget, XtPointer );
    virtual void glInput ( Widget, XtPointer );
    virtual void loadSourceA ( Widget, XtPointer );
    virtual void loadSourceB ( Widget, XtPointer );

    VkWindow * _parent;

private:

    VkCutPaste*		myCutPaste;	// Cut & Paste manager
    
    int	    		myWidth;	// Size of processed images.
    int	    		myHeight;	// Size of processed images.

    enum 
    {
	SRC_A_BUF 		= 0,
	SRC_B_BUF 		= 1,
	DST_BUF   		= 2,
	ALPHA_BUF 		= 3,
	FRAME_BUFFER_COUNT	= 4
    }; 
    
    DMfxbuffer*		myBuffers[4];	// The buffers that hold the
					// four images.

    enum
    {
	SRC_A_EVEN_FIELD	= 0,
	SRC_A_ODD_FIELD		= 1,
	SRC_B_EVEN_FIELD	= 2,
	SRC_B_ODD_FIELD		= 3,
	DST_EVEN_FIELD		= 4,
	DST_ODD_FIELD		= 5,
	FIELD_BUFFER_COUNT	= 6
    };
    
    DMfxbuffer*		myFieldBuffers[6];

    class View*		mySourceA;	// GLX Window Handler
    class View*		mySourceB;
    class View*		myResult;
    class View*		myAlpha;
    
    PlugIns* 		myPlugIns;	// Plugin Manager Interface

    // tracks the selected item in the scrolled filter/transition list
    int 		mySelectedItem;
    int 		myLastItem;
    DMeffect* 		mySelectedEffect;

    int 		myEffect;
    FieldMode		myFieldMode;

    unsigned int 	myPart;
    unsigned int 	myTotal;

    Boolean		myCheckState;
    int 		myLoopCount;
    Boolean 		myDebugTimes;

    void allocateBuffers();
    void applyCurrentEffect();
    void cleanupInputBuffer( DMfxbuffer*, DMfxbuffer*, DMfxbuffer* );
    void createImageViewers();
    void executeFilter( int dstUsage, DMparams* format );
    void executeFilterOnce( int outputUsage, DMparams* format, DMboolean,
			    DMfxbuffer*, DMfxbuffer*, int, int );
    void executeTransition( int dstUsage, DMparams* format );
    void executeTransitionOnce( int outputUsage, DMparams* format, DMboolean,
				DMfxbuffer*, DMfxbuffer*, DMfxbuffer*, int, int );
    void extractAlpha();
    void loopChanged(int cnt);
    void prepareOutputBuffer( int usage, DMfxbuffer*, DMfxbuffer*, DMfxbuffer* );
    void reloadImages();
    void resetCurrentEffect(Boolean);
    void setupCurrentEffect();
    void setupFilterList();
    void setupInputBuffer( int usage, DMfxbuffer*, DMfxbuffer*, DMfxbuffer* );
};

#endif

