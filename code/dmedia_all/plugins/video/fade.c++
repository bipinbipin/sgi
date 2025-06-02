////////////////////////////////////////////////////////////////////////
//
// fade.c++
//
// Copyright 1995, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
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

#include <dmedia/PremiereXAPI.h>

#include <assert.h>
#include <bstring.h>
#include <inttypes.h>
#include <stdio.h>		// XXX just for debugging
#include <stdlib.h>

#include <dmedia/fx_dialog.h>
#include <dmedia/fx_buffer.h>
#include <dmedia/fx_utils.h>
#include <dmedia/fx_image_viewer.h>
#include <dmedia/PremiereXAPI.h>
#include <dmedia/fx_dialog.h>

#define SIGNATURE	DM_FX_4_CHARS_TO_INT('f','a','d','e')
#define VERSION		PRX_MAJOR_MINOR(1,0)

#define FALSE 0

#define COLOR_ARRAY 100

#define R_INDEX 0
#define G_INDEX 1
#define B_INDEX 2
#define A_INDEX 3

typedef uint32_t ColorArray[4];


static DMFXDialogScale	*gRedScale   = NULL;
static DMFXDialogScale	*gGreenScale = NULL;
static DMFXDialogScale	*gBlueScale  = NULL;
static DMFXDialogScale	*gAlphaScale = NULL;
static DMFXDialogImage	*gImage	     = NULL;
static DMFXDialog	*gDialog     = NULL;

////////
//
// extractSettings
//
////////

void extractSettings(
    PRX_VideoFilter  theData,
    ColorArray colors)
{
    int numBytes;
    int status;
    int type;
    unsigned int arrayLen;
    void *data;
    
    status = PRX_GetData(theData, COLOR_ARRAY, &type, &arrayLen, &data);
    
    assert(status == pdNoErr);
    assert(arrayLen == 4);

    numBytes = sizeof(colors[0]) * 4;
    
    if (status == pdNoErr && arrayLen == 4)
    {
	memcpy(colors, data, numBytes);
    }
    else
    {
	memset(colors, 0, numBytes);
    }
}


////////
//
// storeSettings
//
////////

static int storeSettings(
    PRX_VideoFilter  theData,
    ColorArray  colors)
{
    unsigned int arrayLen = 4;
    assert(sizeof(ColorArray) / sizeof(colors[0]) == 4);
    
    return PRX_SetData(theData, COLOR_ARRAY, tInt, arrayLen,
      (void *)colors);
}


////////
//
// initializeSettings
//
////////

static void initializeSettings(
    PRX_VideoFilter theData)
{
    if (PRX_GetState(theData) == stNew)
    {
	//
	// Store the signature and version number.
	//
    
	int status;
	status = PRX_InitData(theData, SIGNATURE, VERSION, NULL, 0);
	assert(status == pdNoErr);
	
	//
	// Set the defaults.
	//
    
	ColorArray colors;
    
	colors[R_INDEX] = 0;
	colors[G_INDEX] = 0;
	colors[B_INDEX] = 0;
	colors[A_INDEX] = 0;
    
	storeSettings(theData, colors);
    }
}


////////
//
// getSelectedSettings
//
////////

static void getSelectedSettings(
    ColorArray colors)
{
    if (gRedScale == NULL  || gGreenScale == NULL ||
	gBlueScale == NULL || gAlphaScale == NULL)
    {
	assert(False);

	colors[R_INDEX] = 0;
	colors[G_INDEX] = 0;
	colors[B_INDEX] = 0;
	colors[A_INDEX] = 0;
	return;
    }

    colors[R_INDEX] = gRedScale->getValue();
    colors[G_INDEX] = gGreenScale->getValue();
    colors[B_INDEX] = gBlueScale->getValue();
    colors[A_INDEX] = gAlphaScale->getValue();
}


////////
//
// setSelectedSettings
//
////////

static void setSelectedSettings(
    ColorArray colors)
{
    if (gRedScale == NULL  || gGreenScale == NULL ||
	gBlueScale == NULL || gAlphaScale == NULL)
    {
	assert(False);
    }

    gRedScale->setValue(colors[R_INDEX]);
    gGreenScale->setValue(colors[G_INDEX]);
    gBlueScale->setValue(colors[B_INDEX]);
    gAlphaScale->setValue(colors[A_INDEX]);
}


#define FADE(a,b)  ((unsigned char) ( srcPercent * (a) + fadePercent * (b) ))
    
////////
//
// processFrame
//
////////

static int processFrame(
    ColorArray colors,
    unsigned int part,
    unsigned int total,
    PRX_ScanlineBuffer src,
    PRX_ScanlineBuffer dst)
{
    assert (dst->data != NULL);
    
    //
    // Copy the pixels
    //

    int width  = src->bounds.width;
    int height = src->bounds.height;
    
    unsigned char* srcp = (unsigned char*) src->data +
	(src->bounds.x * 4) + (src->bounds.y * src->rowBytes);
    unsigned char* dstp = (unsigned char*) dst->data +
	(dst->bounds.x * 4) + (dst->bounds.y * dst->rowBytes);
    
    size_t srcBump = src->rowBytes - width * 4;
    size_t dstBump = dst->rowBytes - width * 4;
    
    float srcPercent  = (float)(total - part) / (float)total;
    float fadePercent = (float)part / (float)total;

    unsigned char fadeR = colors[R_INDEX];
    unsigned char fadeG = colors[G_INDEX];
    unsigned char fadeB = colors[B_INDEX];
    unsigned char fadeA = colors[A_INDEX];

    for ( int y = 0;  y < height;  y++ )
    {
	for ( int x = 0;  x < width;   x++ )
	{
	    unsigned char a = *srcp++;
	    unsigned char b = *srcp++;
	    unsigned char g = *srcp++;
	    unsigned char r = *srcp++;
	    
	    *dstp++ = FADE( a, fadeA );
	    *dstp++ = FADE( b, fadeB );
	    *dstp++ = FADE( g, fadeG );
	    *dstp++ = FADE( r, fadeR );
	}
	
	srcp += srcBump;
	dstp += dstBump;
    }
    return dcNoErr;
}


////////
//
// updatePreview
//
// Does a local update to the preview window
//
////////

void updatePreview(
    PRX_VideoFilter theData, 
    ColorArray  colors)
{
    PRX_ScanlineBuffer src = theData->src;

    if (src != NULL)
    {
	PRX_ScanlineBufferRec dst;

	memset(&dst, 0, sizeof(PRX_ScanlineBufferRec));

	dst.data = (unsigned char *) 
		malloc(src->rowBytes * (src->bounds.y + src->bounds.height));
	if (dst.data == NULL)
	{
	    assert(False);
	    gImage->myImage->setBuf(NULL, 0, 0, 0);
	    return;		// return error???   XXX
	}

	dst.rowBytes = src->rowBytes;
	dst.bounds   = src->bounds;

	//
	// when this is converted over to be an Image class (with
	// a source and a viewer), we should just be able to do
	// a setColor() (if we want to show the color?).
	//
	
	processFrame(colors, 1, 1, src, &dst);
	
	gImage->myImage->setBuf(
	    dst.data + (dst.bounds.x * 4) + (dst.bounds.y * dst.rowBytes),
	    dst.bounds.width, dst.bounds.height,
	    dst.rowBytes
	    );

	free(dst.data);
    }
}

////////
//
// changedCB:
//
////////

static void changedCB(
    VkCallbackObject *,
    void *clientData,
    void * /*callData*/ )
{
    PRX_VideoFilter theData = (PRX_VideoFilter) clientData;
    ColorArray colors;

    getSelectedSettings(colors);
    updatePreview(theData, colors);
}

////////
//
// setup
//
////////

static int setup(
    PRX_VideoFilter theData)
{
    int status = dcNoErr;

    //
    // Initialize the defaults if necessary.
    //
    
    initializeSettings(theData);

    //
    // Get the current settings.
    //

    ColorArray colors;
    extractSettings(theData, colors);
    
    //
    // Post the dialog
    //

    String defResources[] =
    {
	"Red*TitleString: Red",
	"Green*TitleString: Green",
	"Blue*TitleString: Blue",
	"Alpha*TitleString: Alpha",
	NULL
    };

    if (gDialog == NULL)
    {
	gDialog = new DMFXDialog("fadeDialog", PRX_GetAppShell(), defResources);

	gDialog->addCallback( DMFXDialog::changedCallback,
		    (VkCallbackFunction)&changedCB, theData );

	gImage = gDialog->addImage("Image", 
	  theData->src->bounds.width, theData->src->bounds.height,
	  False, False, True);
	
	gRedScale = gDialog->addScale("Red", colors[R_INDEX], 0, 255,
	  XmHORIZONTAL, True);
	gGreenScale = gDialog->addScale("Green", colors[G_INDEX], 0, 255,
	  XmHORIZONTAL, True);
	gBlueScale = gDialog->addScale("Blue", colors[B_INDEX], 0, 255,
	  XmHORIZONTAL, True);
	gAlphaScale = gDialog->addScale("Alpha", colors[A_INDEX], 0, 255,
	  XmHORIZONTAL, True);
    }

    if (gDialog->postAndWait() == VkDialogManager::OK)
    {
	getSelectedSettings(colors);
	storeSettings(theData, colors);
	status = dcNoErr;
    }
    else
    {
	extractSettings(theData, colors);
	setSelectedSettings(colors);
	status = dcOtherErr;
    }

    return status;
}


////////
//
// execute
//
////////

static int execute(
    PRX_VideoFilter theData)
{
    ColorArray colors;
    int status = 0;

    //
    // Initialize the defaults if necessary.
    //
    
    initializeSettings(theData);

    extractSettings(theData, colors);
    status = processFrame(colors, theData->part, theData->total,
      theData->src, theData->dst);
    
    return status;
}

////////
//
// cleanup
//
////////

static int cleanup(
    PRX_VideoFilter)
{
    delete gImage;	gImage	     = NULL;
    delete gRedScale;	gRedScale    = NULL;
    delete gGreenScale;	gGreenScale  = NULL;
    delete gBlueScale;	gBlueScale   = NULL;
    delete gAlphaScale;	gAlphaScale  = NULL;
    delete gDialog;	gDialog	     = NULL;

    return dcNoErr;
}

////////
//
// dispatchFunction
//
////////

extern "C" int dispatchFunction(
    int			selector,
    PRX_VideoFilter	theData)
{
    switch (selector) 
    {
	case fsCleanup:
	    return cleanup(theData);

	case fsExecute:
	    return execute(theData);
	    
	case fsSetup:
	    return setup(theData);
	    
	default:
	    assert(FALSE);
	    return dcUnsupported;
    }
}
	

////////
//
// PRX_PluginProperties
//
////////

extern "C" PRX_PropertyList PRX_PluginProperties(
    unsigned /* apiVersion */)
{
    static PRX_KeyValueRec plugInProperties[] =
    {
	{ PK_APIVersion,	(void*) PRX_MAJOR_MINOR(1,0) },
	{ PK_DispatchSym,	(void*) "dispatchFunction" },
	{ PK_Name,		(void*) "Fade" },
	{ PK_UniquePrefix,	(void*) "SGI" },
	{ PK_PluginType,	(void*) PRX_TYPE_VIDEO_FILTER },
	{ PK_PluginVersion,	(void*) PRX_MAJOR_MINOR(1,0) },
	{ PK_HasDialog,		(void*) True },
	{ PK_EndOfList,		(void*) 0 },
    };
    
    return (PRX_PropertyList) &plugInProperties;
}

