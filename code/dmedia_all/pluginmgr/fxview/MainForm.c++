////////////////////////////////////////////////////////////////////////
//
// MainForm.c++
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

#include "MainForm.h"
#include <GL/GLwMDrawA.h> 
#include <Xm/Form.h> 
#include <Xm/Frame.h> 
#include <Xm/Label.h> 
#include <Xm/List.h> 
#include <Xm/PushB.h> 
#include <Xm/RowColumn.h> 
#include <Xm/Scale.h> 
#include <Xm/ScrolledW.h> 
#include <Xm/ToggleB.h> 
#include <Vk/VkResource.h>
#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>

extern void VkUnimplemented(Widget, const char *);

#include "ImageSource.h"
#include "View.h"

#include <bstring.h>		// bzero()
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <il/ilABGRImg.h>
#include <il/ilDisplay.h>
#include <il/ilFileImg.h>

#include <Xm/AtomMgr.h>

#include <Vk/VkFileSelectionDialog.h>

#define ABGR_NAME "_ABGR_IMG"

static void listSelectCB(Widget w, XtPointer clientData, XtPointer callData);
static void dropCB(Widget, Atom, XtPointer, unsigned long,int,int, void *);
static void setupDmParams(DMparams **params, int width, int height);

#define DEBUG_TIMES 1
#include <sys/time.h>
static void snapTime( const char* );

//
// This is the initial image that is displayed.
//

#define STAND_BY_IMAGE "/usr/lib/dmedia/NoVideo.rgb"

//
// This is the size at which images are processed.
//
// It is exactly the same as the output window, so the the effects of
// field processing can be seen accurately.
//

#define IMAGE_WIDTH		240
#define IMAGE_HEIGHT		180

static void reportDMError(
    )
{
    int errorNum;
    char detail[DM_MAX_ERROR_DETAIL];
    const char* message;
    message = dmGetError( &errorNum, detail );
    assert( message != NULL );
    fprintf( stderr, "Error: %s\n", detail );
}

static void fatalDMError()
{
    reportDMError();
    exit(1);
}

////////
//
// Constructor
//
////////

MainForm::MainForm(const char *name, Widget parent) : 
                   MainFormUI(name, parent) 
{ 
    // This constructor calls MainFormUI(parent, name)
    // which calls MainFormUI::create() to create
    // the widgets for this component. Any code added here
    // is called after the component's interface has been built

    int max;
    int min;

    //
    // mySelectedItem needs to track with the selection events on
    // the scrolled area.
    //

    mySelectedItem 	= -1;
    mySelectedEffect 	= NULL;
    myEffect 		= FILTER;
    myFieldMode		= PROCESS_FRAMES;
    myLastItem 		= -1;

    XtVaGetValues(_frameSlider, XmNmaximum, &max, XmNminimum, &min, NULL);
    myPart  = 0;
    myTotal = max - min;

    myPlugIns = new PlugIns(theApplication->baseWidget());

    myWidth		= IMAGE_WIDTH;
    myHeight		= IMAGE_HEIGHT;

    //
    // Create the four buffers to hold the images.
    //

    this->allocateBuffers();

    //
    // create the image viewers
    //

    createImageViewers();
    
    //
    // debugging output, and number of times to call the filter
    //
    
    myCheckState = True;
    myDebugTimes = False;
    myLoopCount  = 1;

    //
    // construct the cut paste class
    //

    myCutPaste = new VkCutPaste(this->baseWidget());

    //
    // initialize the drop sites, drop targets and data types for cut/paste
    //

    Atom interestList[2];
    interestList[0] = XmInternAtom(XtDisplayOfObject(this->baseWidget()),
      ABGR_NAME, False);
    interestList[1] = XmInternAtom(XtDisplayOfObject(this->baseWidget()),
      "_SGI_ICON", False);

    myCutPaste->registerDataType(interestList[0], interestList[0], 8);

    //
    // register the Frame widgets, because if we register the
    // gl render area, for some reason, we aren't getting the "locate"
    // box when we motion over the widget.  Using the frame, and things work
    // fine
    //

    myCutPaste->registerDropSite(_srcAGLFrame, interestList, 2, dropCB, this);
    myCutPaste->registerDropSite(_srcBGLFrame, interestList, 2, dropCB, this);


    //
    // customize the behavior of the scrolled list.
    //

    XtVaSetValues(_scrolledList,
      XmNselectionPolicy,	XmSINGLE_SELECT,
      XmNscrollBarDisplayPolicy, XmSTATIC,
      NULL);

    XtAddCallback(_scrolledList, XmNsingleSelectionCallback, 
      &listSelectCB, this);
    XtAddCallback(_scrolledList, XmNdefaultActionCallback,
      &listSelectCB, this);

    //
    // initialize the filter/transition list displayed in the
    // scrolled list.
    //

    setupFilterList();

    theFileSelectionDialog->setDirectory("/usr/share/data/images");

    char *cp;
    if ((cp = getenv("FXVIEW_IMAGE_DIR")) != NULL)
    {
	theFileSelectionDialog->setDirectory(cp);
    }

    XtSetSensitive( _customizeButton, False );
}

////////
//
// Destructor
//
////////

MainForm::~MainForm()
{
    // The base class destructors are responsible for
    // destroying all widgets and objects used in this component.
    // Only additional items created directly in this class
    // need to be freed here.

    delete myCutPaste;
    delete myPlugIns;
    
    dmFXFreeImageBuffers( FRAME_BUFFER_COUNT, myBuffers );
    dmFXFreeImageBuffers( FIELD_BUFFER_COUNT, myFieldBuffers );
}


////////
//
// applyCurrentEffect
//
////////

void MainForm::applyCurrentEffect()
{
    DMstatus s;
    
    if (mySelectedEffect == NULL)
    {
	clearDestination();
	return;
    }

    DMplugmgr *mgr = getEffectsManager();
    if (mgr == NULL)
    {
	assert(False);
	return;
    }

    DMplugin* plugin = dmPMGetPluginFromEffect( mySelectedEffect );
    const char *name = dmPMGetName( plugin );
    
    //
    // Set up the two (or three) buffers
    //

    {
	int sourceAUsage = dmPMGetSourceAUsage( plugin );
	this->setupInputBuffer(
	    sourceAUsage,
	    myBuffers[SRC_A_BUF],
	    myFieldBuffers[SRC_A_EVEN_FIELD],
	    myFieldBuffers[SRC_A_ODD_FIELD]
	    );
    }
    
    if ( myEffect == TRANSITION )
    {
	int sourceBUsage = dmPMGetSourceBUsage( plugin );
	this->setupInputBuffer(
	    sourceBUsage,
	    myBuffers[SRC_B_BUF],
	    myFieldBuffers[SRC_B_EVEN_FIELD],
	    myFieldBuffers[SRC_B_ODD_FIELD]
	    );
    }

    //
    // Set up the params that describe the image format.
    //

    DMparams* format = NULL;
    switch ( myFieldMode )
    {
	case PROCESS_FRAMES:
	    setupDmParams( &format, myWidth, myHeight );
	    break;
	    
	case PROCESS_EVEN_FIELDS:
	case PROCESS_ODD_FIELDS:
	    setupDmParams( &format, myWidth, myHeight/2 );
	    break;
	    
	default:
	    assert( DM_FALSE );
    }
    
    
    //
    // Execute the effect.
    //

    if (myDebugTimes)
    {
	char msg[80];
	sprintf(msg, "Begin: %s count: %d  ", name, myLoopCount);
	snapTime(msg);
    }

    for ( int i = 0;  i < myLoopCount;  i++ )
    {
	switch ( myEffect )
	{
	    case FILTER:
		this->executeFilter(
		    dmPMGetDestUsage( plugin ),
		    format
		    );
		break;
	    
	    case TRANSITION:
		this->executeTransition(
		    dmPMGetDestUsage( plugin ),
		    format
		    );
		break;

	    default:
		assert( FALSE );
	}
    }
    
    if (myDebugTimes)
    {
	char msg[80];
	sprintf(msg, "  End: %s count: %d  ", name, myLoopCount);
	snapTime(msg);
    }

    //
    // Clean up the buffers.
    //

    this->cleanupInputBuffer(
	myBuffers[SRC_A_BUF],
	myFieldBuffers[SRC_A_EVEN_FIELD],
	myFieldBuffers[SRC_A_ODD_FIELD]
	);
    
    if ( myEffect == TRANSITION )
    {
	this->cleanupInputBuffer(
	    myBuffers[SRC_B_BUF],
	    myFieldBuffers[SRC_B_EVEN_FIELD],
	    myFieldBuffers[SRC_B_ODD_FIELD]
	    );
    }
    
    switch ( myFieldMode )
    {
	case PROCESS_FRAMES:
	    break;
	    
	case PROCESS_EVEN_FIELDS:
	case PROCESS_ODD_FIELDS:
	    s = dmFXJoinFields(
		bufInputAll, 
		myFieldBuffers[DST_EVEN_FIELD],
		myFieldBuffers[DST_ODD_FIELD],
		myBuffers[DST_BUF]
		);
	    if ( s != DM_SUCCESS )
	    {
		fatalDMError();
	    }
	    break;
	    
	default:
	    assert( DM_FALSE );
    }
    
    
    dmParamsDestroy( format );

    //
    // Extract the alpha from the result image.
    //

    this->extractAlpha();
    
    //
    // Display the results
    //

    myResult->redraw();
    myAlpha ->redraw();
}



////////
//
// className
//
////////

const char * MainForm::className()
{
    return "MainForm";
}

////////
//
// allocateBuffers
//
////////

void MainForm::allocateBuffers(
    )
{
    DMparams* format;
    DMstatus status;
    
    //
    // Set up the image format.
    //

    dmParamsCreate( &format );
    dmSetImageDefaults( format, myWidth, myHeight, DM_IMAGE_PACKING_ABGR );
    dmParamsSetEnum( format, DM_IMAGE_ORIENTATION, DM_IMAGE_TOP_TO_BOTTOM );
    
    //
    // Make no assumptions about how the buffers will be used: set all
    // of the bits.
    //

    int inputUsage = bufInputAll;
    
    int outputUsage = bufOutputDirect | bufOutputOpenGL;
    
    //
    // Allocate the buffers that store the full frames.
    //

    status = dmFXAllocateImageBuffers(
	XtDisplayOfObject(baseWidget()),
	format,
	inputUsage,
	outputUsage,
	FRAME_BUFFER_COUNT,
	myBuffers
	);
    if ( status != DM_SUCCESS )
    {
	fatalDMError();
    }
    
    //
    // Allocate the buffers that store the fields.
    //

    status = dmParamsSetInt( format, DM_IMAGE_HEIGHT, myHeight / 2 );
    if ( status != DM_SUCCESS )
    {
	fatalDMError();
    }
    
    status = dmFXAllocateImageBuffers(
	XtDisplayOfObject(baseWidget()),
	format,
	inputUsage,
	outputUsage,
	FIELD_BUFFER_COUNT,
	myFieldBuffers
	);
    if ( status != DM_SUCCESS )
    {
	fatalDMError();
    }
    
    //
    // Clean up.
    //

    dmParamsDestroy( format );
}

////////
//
// checkStateChanged
//
////////

void MainForm::checkStateChanged( Widget w, XtPointer )
{
    myCheckState = XmToggleButtonGetState(w);
}


////////
//
// cleanupInputBuffer
//
////////

void MainForm::cleanupInputBuffer(
    DMfxbuffer* frame,
    DMfxbuffer* evenField,
    DMfxbuffer* oddField
    )
{
    DMstatus s;
    
    switch ( myFieldMode )
    {
	case PROCESS_FRAMES:
	    s = dmFXCleanupInputImageBuffer( frame );
	    if ( s != DM_SUCCESS )
	    {
		fatalDMError();
	    }
	    break;
	    
	case PROCESS_EVEN_FIELDS:
	case PROCESS_ODD_FIELDS:
	    s = dmFXCleanupInputImageBuffer( evenField );
	    if ( s != DM_SUCCESS )
	    {
		fatalDMError();
	    }
	    s = dmFXCleanupInputImageBuffer( oddField );
	    if ( s != DM_SUCCESS )
	    {
		fatalDMError();
	    }
	    break;

	default:
	    assert( DM_FALSE );
    }
}

////////
//
// clearDestination
//
////////

void MainForm::clearDestination()
{
    myResult->getSource()->clear();
    myResult->redraw();

    myAlpha->getSource()->clear();
    myAlpha->redraw();

    XmListDeselectAllItems(_scrolledList);
}



////////
//
// copy
//
////////

void MainForm::copy(   )
{
   ::VkUnimplemented ( NULL, "MainForm::copy" );
}


////////
//
// cut
//
////////

void MainForm::cut(   )
{
    ::VkUnimplemented ( NULL, "MainForm::cut" );
}


////////
//
// doFilters
//
////////

void MainForm::doFilters ( Widget, XtPointer )
{
    myEffect = FILTER;
    setupFilterList();
    resetCurrentEffect(True);
    clearDestination();
}

////////
//
// doFrameSlider
//
////////

void MainForm::doFrameSlider ( Widget w, XtPointer callData )
{
    XmScaleCallbackStruct *cbs = (XmScaleCallbackStruct*) callData;

    int min;
    XtVaGetValues(w, XmNminimum, &min, NULL);
    myPart = cbs->value - min;

    this->reloadImages();
}


////////
//
// doSetup
//
////////

void MainForm::doSetup ( Widget, XtPointer )
{
    resetCurrentEffect(False);
    setupCurrentEffect();
    applyCurrentEffect();
}

////////
//
// doTransitions
//
////////

void MainForm::doTransitions ( Widget, XtPointer )
{
    myEffect = TRANSITION;
    setupFilterList();
    resetCurrentEffect(True);
    clearDestination();
}

////////
//
// executeFilter
//
// This assumes that the input and output buffers have already been
// set up (see applyCurrentEffect()).
//
////////

void MainForm::executeFilter(
    int dstUsage,
    DMparams* format
    )
{
    //
    // Decide which field goes first in field processing.
    //

    int oddOffsetPart  = 0;
    int evenOffsetPart = 0;
    switch ( myFieldMode )
    {
	case PROCESS_FRAMES:
	    break;
	    
	case PROCESS_EVEN_FIELDS:
	    oddOffsetPart = 1;
	    break;
	    
	case PROCESS_ODD_FIELDS:
	    evenOffsetPart = 1;
	    break;
	    
	default:
	    assert( DM_FALSE );
    }
    
    //
    // Execute the filter.
    //

    switch ( myFieldMode )
    {
	case PROCESS_FRAMES:
	    this->executeFilterOnce(
		dstUsage,
		format,
		DM_FALSE,
		myBuffers[SRC_A_BUF],
		myBuffers[DST_BUF],
		myPart, myTotal
		);
	    break;
			
	case PROCESS_EVEN_FIELDS:
	case PROCESS_ODD_FIELDS:
	    this->executeFilterOnce(
		dstUsage,
		format,
		DM_TRUE, // it's an even field
		myFieldBuffers[SRC_A_EVEN_FIELD],
		myFieldBuffers[DST_EVEN_FIELD],
		myPart * 2 + evenOffsetPart, myTotal * 2 + 1
		);
			
	    this->executeFilterOnce(
		dstUsage,
		format,
		DM_FALSE, // it's not an even field
		myFieldBuffers[SRC_A_ODD_FIELD],
		myFieldBuffers[DST_ODD_FIELD],
		myPart * 2 + oddOffsetPart, myTotal * 2 + 1
		);
	    break;
			
	default:
	    assert( DM_FALSE );
    }
}

////////
//
// executeFilterOnce
//
////////

void MainForm::executeFilterOnce(
    int dstUsage,
    DMparams* format,
    DMboolean isEvenField,
    DMfxbuffer* srcA,
    DMfxbuffer* dst,
    int part,
    int total
    )
{
    DMstatus s;
    
    s = dmFXSetupOutputImageBuffer(
	dstUsage,
	bufInputAll,
	dst
	);
    if ( s != DM_SUCCESS )
    {
	reportDMError();
    }
    
    switch( myFieldMode )
    {
	case PROCESS_FRAMES:
	    s = dmPMBufferExecuteVideoFilter(
		mySelectedEffect, 
		srcA, format,
		dst,  format,
		part, total
		);
	    break;
	    
	case PROCESS_EVEN_FIELDS:
	case PROCESS_ODD_FIELDS:
	    s = dmPMBufferExecuteVideoFilterOnField(
		mySelectedEffect,
		isEvenField,
		srcA, format,
		dst,  format,
		part, total
		);
	    break;
	    
	default:
	    assert( DM_FALSE );
    }
    if ( s != DM_SUCCESS )
    {
	reportDMError();
    }
    
    if ( myCheckState )
    {
	dmFXCheckOutputImageBufferState( dst );
    }
    s = dmFXCleanupOutputImageBuffer( dst );
    if ( s != DM_SUCCESS )
    {
	reportDMError();
    }
}

////////
//
// executeTransition
//
// This assumes that the input and output buffers have already been
// set up (see applyCurrentEffect()).
//
////////

void MainForm::executeTransition(
    int dstUsage,
    DMparams* format
    )
{
    //
    // Decide which field goes first in field processing.
    //

    int oddOffsetPart  = 0;
    int evenOffsetPart = 0;
    switch ( myFieldMode )
    {
	case PROCESS_FRAMES:
	    break;
	    
	case PROCESS_EVEN_FIELDS:
	    oddOffsetPart = 1;
	    break;
	    
	case PROCESS_ODD_FIELDS:
	    evenOffsetPart = 1;
	    break;
	    
	default:
	    assert( DM_FALSE );
    }
    
    //
    // Execute the transition.
    //

    switch ( myFieldMode )
    {
	case PROCESS_FRAMES:
	    this->executeTransitionOnce(
		dstUsage,
		format,
		DM_FALSE,
		myBuffers[SRC_A_BUF],
		myBuffers[SRC_B_BUF],
		myBuffers[DST_BUF],
		myPart, myTotal
		);
	    break;
			
	case PROCESS_EVEN_FIELDS:
	case PROCESS_ODD_FIELDS:
	    this->executeTransitionOnce(
		dstUsage,
		format,
		DM_TRUE, // it's an even field
		myFieldBuffers[SRC_A_EVEN_FIELD],
		myFieldBuffers[SRC_B_EVEN_FIELD],
		myFieldBuffers[DST_EVEN_FIELD],
		myPart * 2 + evenOffsetPart, myTotal * 2 + 1
		);
			
	    this->executeTransitionOnce(
		dstUsage,
		format,
		DM_FALSE, // it's not an even field
		myFieldBuffers[SRC_A_ODD_FIELD],
		myFieldBuffers[SRC_B_ODD_FIELD],
		myFieldBuffers[DST_ODD_FIELD],
		myPart * 2 + oddOffsetPart, myTotal * 2 + 1
		);
			
	    break;
			
	default:
	    assert( DM_FALSE );
    }
}

////////
//
// executeTransitionOnce
//
////////

void MainForm::executeTransitionOnce(
    int dstUsage,
    DMparams* format,
    DMboolean isEvenField,
    DMfxbuffer* srcA,
    DMfxbuffer* srcB,
    DMfxbuffer* dst,
    int part,
    int total
    )
{
    DMstatus s;
    
    s = dmFXSetupOutputImageBuffer(
	dstUsage,
	bufInputAll,
	dst
	);
    if ( s != DM_SUCCESS )
    {
	reportDMError();
    }
    
    switch( myFieldMode )
    {
	case PROCESS_FRAMES:
	    s = dmPMBufferExecuteVideoTransition(
		mySelectedEffect, 
		srcA, format,
		srcB, format,
		dst,  format,
		part, total
		);
	    break;
	    
	case PROCESS_EVEN_FIELDS:
	case PROCESS_ODD_FIELDS:
	    s = dmPMBufferExecuteVideoTransitionOnField(
		mySelectedEffect,
		isEvenField,
		srcA, format,
		srcB, format,
		dst,  format,
		part, total
		);
	    break;
	    
	default:
	    assert( DM_FALSE );
    }
    if ( s != DM_SUCCESS )
    {
	reportDMError();
    }
    
    if ( myCheckState )
    {
	dmFXCheckOutputImageBufferState( dst );
    }
    s = dmFXCleanupOutputImageBuffer( dst );
    if ( s != DM_SUCCESS )
    {
	reportDMError();
    }
}

////////
//
// extractAlpha
//
////////

void MainForm::extractAlpha(
    )
{
    DMstatus status;
    
    //
    // Get the buffers ready.
    //
    
    status = dmFXSetupInputImageBufferWithUsage( bufInputDirect, myBuffers[DST_BUF]);
    if ( status != DM_SUCCESS )
    {
	fatalDMError();
    }
    status = dmFXSetupOutputImageBuffer(
	bufOutputDirect, bufInputAll, myBuffers[ALPHA_BUF] );
    if ( status != DM_SUCCESS )
    {
	fatalDMError();
    }
    
    //
    // Get pointers to the result and alpha buffers
    //

    unsigned char* rp = (unsigned char*) dmFXGetDataPtr( myBuffers[DST_BUF] );
    unsigned char* ap = (unsigned char*) dmFXGetDataPtr( myBuffers[ALPHA_BUF] );

    //
    // How many extra bytes at the end of each row?
    //

    size_t rBump = ( dmFXGetRowLength( myBuffers[DST_BUF] ) - myWidth ) * 4;
    size_t aBump = ( dmFXGetRowLength( myBuffers[ALPHA_BUF] ) - myWidth ) * 4;

    //
    // Process the pixels.
    //

    for ( int y = 0;  y < myHeight;  y++ )
    {
	for ( int x = 0;  x < myWidth;  x++ )
	{
	    unsigned char a = *rp++;
	    rp += 3;		// skip b, g, r

	    *ap++ = 255;	// a
	    *ap++ = a;		// b
	    *ap++ = a;		// g
	    *ap++ = a;		// r
	}
	rp += rBump;
	ap += aBump;
    }

    //
    // Tell the buffers that we're done.
    //

    status = dmFXCleanupInputImageBuffer( myBuffers[DST_BUF] );
    if ( status != DM_SUCCESS )
    {
	fatalDMError();
    }
    status = dmFXCleanupOutputImageBuffer( myBuffers[ALPHA_BUF] );
    if ( status != DM_SUCCESS )
    {
	fatalDMError();
    }
}


////////
//
// createImageViewers
//
////////

void MainForm::createImageViewers()
{
    mySourceA = View::create(_sourceA_GLWidget);
    mySourceB = View::create(_sourceB_GLWidget);
    myResult  = View::create(_result_GLWidget);
    myAlpha   = View::create(_alpha_GLWidget);

    mySourceA->setSource(
	ImageSource::openFile(
	    myWidth, myHeight, myBuffers[SRC_A_BUF],
	    TRUE, STAND_BY_IMAGE));

    mySourceB->setSource(
	ImageSource::solidColor( 
	    myWidth, myHeight, myBuffers[SRC_B_BUF],
	    80, 80, 80, 255));

    myResult->setSource(
	ImageSource::createBuffer(
	    myWidth, myHeight, myBuffers[DST_BUF]));

    myAlpha->setSource(
	ImageSource::createBuffer(
	    myWidth, myHeight, myBuffers[ALPHA_BUF]));
}

////////
//
// fieldChanged
//
////////

void MainForm::fieldChanged(
    FieldMode mode
    )
{
    myFieldMode = mode;
    this->applyCurrentEffect();
}

////////
//
// glInput
//
////////

void MainForm::glInput ( Widget w, XtPointer callData )
{
    GLwDrawingAreaCallbackStruct *cbs = (GLwDrawingAreaCallbackStruct*) callData;

    assert(cbs->reason == GLwCR_INPUT);
    if (cbs->reason != GLwCR_INPUT || cbs->event->type != ButtonPress)
    {
	return;
    }

    //
    // find the correct widget to use.
    // Motif appears to have a bug when dragging from a window in
    // a non-default visual (or simply if the visual is TrueColor?).
    // We work around this by using the frame widget that wraps
    // the gl drawing area.  This is sleezy, as it relies on a particular
    // layout of the widget tree, but it appears to be the only workaround
    // available.
    //
    // XXX
    //


    Widget dragW;
    View *view = NULL;

    if (w == _sourceA_GLWidget)
    {
	dragW = _srcAGLFrame;
	view  = mySourceA;
    }
    else if (w == _sourceB_GLWidget)
    {
	dragW = _srcBGLFrame;
	view  = mySourceB;
    } 
    else if (w == _result_GLWidget)
    {
	dragW = _resultGLFrame1;
	view  = myResult;
    } 
    else if (w == _alpha_GLWidget)
    {
	dragW = _alphaGLFrame;
	view  = myAlpha;
    } 
    else
    {
	assert(False);
	return;
    }

    DMfxbuffer* buffer = view->getSource()->getBuffer();
    
    dmFXSetupInputImageBufferWithUsage( bufInputDirect, buffer );
    
    //
    // we simply construct a byte stream.  The stream looks like:
    //
    // bytes			variable	type
    // 0-3				width		uint32_t
    // 4-7				height		uint32_t
    // 8-11				rowbytes	uint32_t
    // 11-(height * rowbytes)	data		ABGR
    //

    int rowBytes = dmFXGetRowLength( buffer ) * 4;
    uint32_t bufferSize = myHeight * rowBytes;
    uint32_t numBytes = bufferSize + (3 * sizeof(uint32_t));
    XtPointer stream = (XtPointer) malloc(numBytes);
    if (stream == NULL)
    {
	fprintf( stderr, "Could not allocate memory for drag-away" );
	return;
    }

    uint32_t* uip;
    uip = (uint32_t*) stream;
    *(uip + 0) = (uint32_t) myWidth;
    *(uip + 1) = (uint32_t) myHeight;
    *(uip + 2) = (uint32_t) rowBytes;
    
    memcpy( uip + 3, dmFXGetDataPtr( buffer ), bufferSize );

    Atom abgr;
    abgr = XmInternAtom(XtDisplayOfObject(dragW), ABGR_NAME, False);

    myCutPaste->dragAwayCopy(dragW, cbs->event, abgr, stream, numBytes,
			     NULL, NULL);

    free(stream);
    dmFXCleanupInputImageBuffer( buffer );
}

////////
//
// loadSourceA
//
////////

void MainForm::loadSourceA ( Widget, XtPointer )
{
    if( theFileSelectionDialog->postAndWait() == VkDialogManager::OK )
    {
	const char* fileName = theFileSelectionDialog->fileName();
	mySourceA->setSource( 
	    ImageSource::openFile(
		myWidth, myHeight, myBuffers[SRC_A_BUF],
		TRUE, fileName ) );
    }

    this->reloadImages();
}

////////
//
// loadSourceB
//
////////

void MainForm::loadSourceB ( Widget, XtPointer )
{
    if( theFileSelectionDialog->postAndWait() == VkDialogManager::OK )
    {
	const char* fileName = theFileSelectionDialog->fileName();
	mySourceB->setSource( 
	    ImageSource::openFile(
		myWidth, myHeight, myBuffers[SRC_B_BUF],
		TRUE, fileName ) );
    }

    this->reloadImages();
}


////////
//
// loop1000Changed
//
////////

void MainForm::loop1000Changed( Widget w, XtPointer )
{
    if (XmToggleButtonGetState(w))
    {
	loopChanged(1000);
    }
}


////////
//
// loop100Changed
//
////////

void MainForm::loop100Changed( Widget w, XtPointer )
{
    if (XmToggleButtonGetState(w))
    {
	loopChanged(100);
    }
}


////////
//
// loop10Changed
//
////////

void MainForm::loop10Changed( Widget w, XtPointer )
{
    if (XmToggleButtonGetState(w))
    {
	loopChanged(10);
    }
}


////////
//
// loop1Changed
//
////////

void MainForm::loop1Changed( Widget w, XtPointer )
{
    if (XmToggleButtonGetState(w))
    {
	loopChanged(1);
    }
}


////////
//
// loopChanged
//
////////

void MainForm::loopChanged( Widget, XtPointer )
{

    ::VkUnimplemented ( NULL, "MainForm::loopChanged" );
}


////////
//
// newFile
//
////////

void MainForm::newFile(   )
{
    ::VkUnimplemented ( NULL, "MainForm::newFile" );
}


////////
//
// openFile
//
////////

void MainForm::openFile( const char* )
{
    this->loadSourceA( NULL, NULL );
}


////////
//
// paste
//
////////

void MainForm::paste(   )
{
    ::VkUnimplemented ( NULL, "MainForm::paste" );
}


////////
//
// print
//
////////

void MainForm::print( const char* )
{
    ::VkUnimplemented ( NULL, "MainForm::print" );
}


////////
//
// printTimeChanged
//
////////

void MainForm::printTimeChanged( Widget w, XtPointer )
{
    myDebugTimes = XmToggleButtonGetState(w);
}


////////
//
// ReloadEffects
//
////////

void MainForm::ReloadEffects( Widget, XtPointer )
{
    clearDestination();

    if (myPlugIns != NULL)
    {
	delete myPlugIns;
    }
    myPlugIns = new PlugIns(theApplication->baseWidget());

    setupFilterList();
}


////////
//
// reloadImages
//
////////

void MainForm::reloadImages(
    )
{
    //
    // Map the slider setting to a time.
    // XXX The 2-second total is hard-coded for now.
    //

    VfxTime time = VfxTime::fromNanoseconds( 2 * myPart * BILLION / myTotal );

    //
    // Update the source images
    //

    mySourceA->setTime( time );
    mySourceB->setTime( time );
    
    //
    // Re-do the effect
    //

    this->applyCurrentEffect();
}

////////
//
// setParent
//
////////

void MainForm::setParent( class VkWindow * parent )
{
    // Store a pointer to the parent VkWindow. This can
    // be useful for accessing the menubar from this class.


    _parent = parent;

}

////////
//
// setupInputBuffer
//
////////

void MainForm::setupInputBuffer(
    int inputUsage,
    DMfxbuffer* frame,
    DMfxbuffer* evenField,
    DMfxbuffer* oddField
    )
{
    DMstatus s;
    
    switch ( myFieldMode )
    {
	case PROCESS_FRAMES:
	    s = dmFXSetupInputImageBufferWithUsage( inputUsage, frame );
	    if ( s != DM_SUCCESS )
	    {
		fatalDMError();
	    }
	    break;
	    
	case PROCESS_EVEN_FIELDS:
	case PROCESS_ODD_FIELDS:
	    s = dmFXSplitFields( inputUsage, frame, evenField, oddField );
	    if ( s != DM_SUCCESS )
	    {
		fatalDMError();
	    }
	    s = dmFXSetupInputImageBufferWithUsage( inputUsage, evenField );
	    if ( s != DM_SUCCESS )
	    {
		fatalDMError();
	    }
	    s = dmFXSetupInputImageBufferWithUsage( inputUsage, oddField );
	    if ( s != DM_SUCCESS )
	    {
		fatalDMError();
	    }
	    break;

	default:
	    assert( DM_FALSE );
    }
}

////////
//
// save
//
////////

void MainForm::save(   )
{
    ::VkUnimplemented ( NULL, "MainForm::save" );
}


////////
//
// saveas
//
////////

void MainForm::saveas( const char* )
{
    ::VkUnimplemented ( NULL, "MainForm::saveas" );
}

////////
//
// setupCurrentEffect:
//
////////

void MainForm::setupCurrentEffect()
{
    if (mySelectedEffect == NULL)
    {
	return;
    }

    DMplugin* plugin = dmPMGetPluginFromEffect( mySelectedEffect );
    
    //
    // Set up the params that describe the image format.
    //

    DMparams* format;
    setupDmParams( &format, myWidth, myHeight );
    
    //
    // Set up the two (or three) buffers
    //

    {
	int sourceAUsage = dmPMGetSourceAUsage( plugin );
	if ( dmFXSetupInputImageBufferWithUsage( sourceAUsage,  myBuffers[SRC_A_BUF] ) 
	     != DM_SUCCESS )
	{
	    fatalDMError();
	}
    }
    
    if ( myEffect == TRANSITION )
    {
	int sourceBUsage = dmPMGetSourceBUsage( plugin );
	if ( dmFXSetupInputImageBufferWithUsage( sourceBUsage,  myBuffers[SRC_B_BUF] ) 
	     != DM_SUCCESS )
	{
	    fatalDMError();
	}
    }
    
    //
    // Setup the effect.
    //

    for ( int i = 0;  i < myLoopCount;  i++ )
    {
	switch ( myEffect )
	{
	    case FILTER:
		if ( dmPMBufferSetupVideoFilter(
		    mySelectedEffect, 
		    myBuffers[SRC_A_BUF], format,
		    myPart, myTotal
		    ) != DM_SUCCESS )
		{
		    reportDMError();
		}
		break;
	    
	    case TRANSITION:
		if ( dmPMBufferSetupVideoTransition(
		    mySelectedEffect, 
		    myBuffers[SRC_A_BUF], format,
		    myBuffers[SRC_B_BUF], format,
		    myPart, myTotal
		    ) != DM_SUCCESS )
		{
		    reportDMError();
		}
		break;

	    default:
		assert( FALSE );
	}
    }
    
    //
    // Clean up the buffers.
    //

    if ( dmFXCleanupInputImageBuffer( myBuffers[SRC_A_BUF] ) != DM_SUCCESS )
    {
	fatalDMError();
    }
    
    if ( myEffect == TRANSITION )
    {
	if ( dmFXCleanupInputImageBuffer( myBuffers[SRC_B_BUF] ) != DM_SUCCESS )
	{
	    fatalDMError();
	}
    }
}


////////
//
// setupFilterList
//
// Sets the scroll list of filters.
//
// The pluginEffect list (internal list which holds the pluginEffects) order MUST
// match the order in which the items are placed the XmList.
//
////////

void MainForm::setupFilterList()
{
    mySelectedItem = -1;

    DMplugmgr *mgr = getEffectsManager();
    if (mgr == NULL)
    {
	return;
    }

    int numEffects  = dmPMGetPluginCount(mgr);
    XmString *names = (XmString *)malloc(numEffects * sizeof(XmString));

    if (names == NULL)
    {
	assert(False);
	return;
    }

    for (int i = 0; i < numEffects; i++)
    {
	const char *name = dmPMGetName(dmPMGetPlugin(mgr, i));
	if (name != NULL)
	{
	    names[i] = XmStringCreate((char *)name, XmFONTLIST_DEFAULT_TAG);
	} else
	{
	    names[i] = XmStringCreate("???", XmFONTLIST_DEFAULT_TAG);
	}
    }

    XmListDeleteAllItems(_scrolledList);
    XmListAddItems(_scrolledList, names, numEffects, 0);

    for (i = 0; i < numEffects; i++)
    {
	XmStringFree(names[i]);
    }
    free(names);
}


////////
//
// getEffectsManager()
//
// returns the current effect manager, either the filter, or the
// transition manager.
//
////////

DMplugmgr * MainForm::getEffectsManager()
{
    if (myEffect == FILTER)
    {
	return myPlugIns->getFilterManager();
    }
    else if (myEffect == TRANSITION)
    {
	return myPlugIns->getTransitionManager();
    }

    assert(False);
    return NULL;
}
    

////////
//
// loopChanged()
//
////////

void MainForm::loopChanged(
    int cnt)
{
    myLoopCount = cnt;
    applyCurrentEffect();
}


////////
//
// getFrameCallback
//
////////

DMstatus getFrameCallback(
    void *clientData, 
    int frame,
    DMplugintrack track,
    int width, 
    int height, 
    int rowBytes, 
    unsigned char *data)
{
    MainForm *form = (MainForm *) clientData;
    return form->getFrame(frame, track, width, height, rowBytes, data);
}
 
DMstatus MainForm::getFrame(
    int frame,
    DMplugintrack track,
    int width, 
    int height, 
    int rowBytes, 
    unsigned char *data)
{   
    ImageSource* src;

    if (track == DM_TRACK_A)    
	src = mySourceA->getSource();
    else
	src = mySourceB->getSource();
   
    if ((src->getHeight() != height)  || (src->getWidth() != width))
    {
	fprintf(stderr, "fxview: wrong size passed into getFrame callback\n");
	return DM_FAILURE;
    }
    
    // XXX This math may not be very accurate for long movies, but it
    // should avoid integer overflow.

    VfxTime time = 
	VfxTime::fromNanoseconds( 
	    ( ( src->getDuration().toNanoseconds() - 1 ) / myTotal ) * frame
	    );

    src->getImageAtTime( time, width, height, rowBytes, data );
   
    return DM_SUCCESS;
}


////////
//
// resetCurrentEffect
//
////////

void MainForm::resetCurrentEffect(Boolean forceReset)
{
    if (mySelectedItem == myLastItem && !forceReset)
	return;
	
    if (mySelectedEffect != NULL)
    {
	dmPMDestroyEffect(mySelectedEffect);
	mySelectedEffect = NULL;
    }

    if (mySelectedItem >= 0)
    {
	DMplugmgr *mgr = getEffectsManager();
	if (mgr == NULL)
	{
	    assert(False);
	    return;
	}

	myLastItem = mySelectedItem;
	mySelectedEffect = dmPMCreateEffect(mgr, dmPMGetPlugin(mgr, mySelectedItem));
	if (mySelectedEffect == NULL)
	{
	    assert(False);
	    return;
	}
	dmPMSetVideoCallback(mySelectedEffect, getFrameCallback, this);
	XtSetSensitive( _customizeButton, dmPMHasDialog(  dmPMGetPlugin(mgr, mySelectedItem) ) );
    }
}

////////
//
// listSelect
//
////////

void MainForm::listSelect(
    Widget w,
    XmListCallbackStruct *cbs)
{
    int numSelectedItems;

    XtVaGetValues(w, XmNselectedItemCount, &numSelectedItems, NULL);

    if (numSelectedItems == 0)
    {
	mySelectedItem = -1;
	clearDestination();
	
    } else {
	mySelectedItem = cbs->item_position - 1;
	resetCurrentEffect(False);
	applyCurrentEffect();
    }
}


////////
//
//  drop
//
////////

void MainForm::drop(
    Widget w,
    Atom   target,
    XtPointer data,
    unsigned long numBytes,
    int /*x*/,
    int /*y*/)
{
    int numFnames = 0;
    char **fnames = NULL;
    View *view = NULL;

    if (w == _srcAGLFrame)
    {
	view = mySourceA;
    }
    else if (w == _srcBGLFrame)
    {
	view = mySourceB;
    }
    else
    {
	assert(False);
	return;
    }

    if (data == NULL || numBytes == 0)
    {
	assert(False);
	return;
    }

    if (target == XmInternAtom(XtDisplayOfObject(w), "_SGI_ICON", False))
    {
	if (myCutPaste->getFilenamesFromSGI_ICON((char *)data, numBytes,
	    &fnames, &numFnames) == True)
	{
	    view->setSource(
		ImageSource::openFile( 
		    myWidth, myHeight, 
		    view->getSource()->getBuffer(),
		    TRUE, fnames[0]
		    ) );
	    myCutPaste->freeFilenamesFromSGI_ICON(fnames, numFnames);
	}
    }
    else if (target == XmInternAtom(XtDisplayOfObject(w), ABGR_NAME, False))
    {
	// printf("have an abgr img\n");

	uint32_t width, height, rb;
	uint32_t *uip = (uint32_t *)data;

	width  = *(uip + 0);
	height = *(uip + 1);
	rb     = *(uip + 2);

	ImageSource* buffer =
	    ImageSource::createBuffer(
		myWidth, myHeight, 
		view->getSource()->getBuffer()
		);

	buffer->loadImage( width, height, rb, (uip + 3) );
	view->setSource( buffer );
    }
    
    this->reloadImages();
}


////////
//
// listSelectCB:
//
// callback which fires when there are changes to the selected
// item in the filter scrolled list.
//
////////

static void listSelectCB(Widget w, XtPointer clientData, XtPointer callData)
{
    MainForm *mainForm = (MainForm *)clientData;
    XmListCallbackStruct *cbs = (XmListCallbackStruct *)callData;

    assert(cbs->reason == XmCR_SINGLE_SELECT ||
	   cbs->reason == XmCR_DEFAULT_ACTION);
    if (cbs->reason != XmCR_SINGLE_SELECT && cbs->reason != XmCR_DEFAULT_ACTION)
    {
	return;
    }

    mainForm->listSelect(w, cbs);
}

static void dropCB(
    Widget w,
    Atom   target,
    XtPointer data,
    unsigned long numBytes,
    int x,
    int y,
    void *clientData)
{
    MainForm *mainForm = (MainForm *)clientData;
    assert(mainForm != NULL);
    if (mainForm == NULL)
    {
	return;
    }

    mainForm->drop(w, target, data, numBytes, x, y);
}

#ifndef DEBUG_TIMES
static void snapTime(const char* label) {}
#else
static void snapTime(
    const char* label
    )
{
    static int first = 1;
    static float start;
    static float then;
    static struct timeval st;
    struct timeval t;

    if ( first ) {
        gettimeofday( &st );
    }

    gettimeofday( &t );

    float now = ( t.tv_sec - st.tv_sec ) + t.tv_usec / 1000000.0;

    if ( ! first ) {
        printf( "%-25s: %7.3f   (%7.3f)\n",
               label, now - start, now - then );
    }
    else {
        start = now;
    }

    first = 0;
    then = now;
}
#endif // DEBUG_TIMES

static void setupDmParams(
    DMparams **params,
    int width,
    int height
    )
{
    if (dmParamsCreate(params) != DM_SUCCESS)
    {
	fatalDMError();
    }

    if (dmSetImageDefaults(*params, width, height,
			   DM_IMAGE_PACKING_ABGR) != DM_SUCCESS)
    {
	fatalDMError();
    }

    if (dmParamsSetEnum( *params, DM_IMAGE_ORIENTATION,
				  DM_IMAGE_TOP_TO_BOTTOM ) != DM_SUCCESS ) 
    {
	fatalDMError();
    }
}


