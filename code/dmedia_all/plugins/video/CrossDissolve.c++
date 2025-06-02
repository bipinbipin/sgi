////////////////////////////////////////////////////////////////////////
//
// CrossDissolve.c++
//
//	A video transition that blends two images
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
#include <bstring.h>		// bcopy()
#include <stdio.h>		// XXX just for debugging
#include <inttypes.h>

#include <GL/glx.h>
#include <GL/gl.h>

#include <dmedia/fx_buffer.h>
#include <dmedia/fx_utils.h>
#include <dmedia/fx_buffer.h>
#include <dmedia/fx_utils.h>

#define FALSE 0

#define SIGNATURE   DM_FX_4_CHARS_TO_INT('x', 'd', 'l', 'v')
#define VERSION     PRX_MAJOR_MINOR(1,0)


////////
//
// initialize
//
////////

static void initialize(
    PRX_Transition theData
    )
{
    //
    // If this is the first time, set up the default values.
    //
    
    if (PRX_GetState(theData) == stNew) {
	int status;
	status = PRX_InitData(theData, SIGNATURE, VERSION, NULL, 0);
	assert(status == pdNoErr);
    }
}


////////
//
// setup
//
////////

static void setup(
    PRX_Transition theData
    )
{
    initialize(theData);
    
    //
    // This plug-in does not have a dialog box, so there is nothing to do.
    //
}

////////
//
// dissolveTransition
//
////////

int dissolveTransition(
    PRX_Transition	theData
    )
{
    initialize(theData);

    PRX_ScanlineBuffer srcA = theData->srcA;
    PRX_ScanlineBuffer srcB = theData->srcB;
    
    int width  = srcA->bounds.width;
    int height = srcA->bounds.height;

    // Draw the srcA image with blending disabled.

    glDisable(GL_BLEND);
    dmFXDrawPixels(theData->dst, 0, 0, srcA, 0, 0, width, height);
    
    // Now blend in sourceB, with a blending factor that increases
    // linearly from 0% to 100% over 'total' frames.

    glEnable(GL_BLEND);
    float bPercent = (float)(theData->part) / (float)theData->total;
    glBlendColorEXT(bPercent, bPercent, bPercent, bPercent);
    glBlendFunc(GL_CONSTANT_ALPHA_EXT, GL_ONE_MINUS_CONSTANT_ALPHA_EXT);

    dmFXDrawPixels(theData->dst, 0, 0, srcB, 0, 0, width, height);

    glDisable(GL_BLEND);

    return dcNoErr;
}

////////
//
// transitionDispatch
//	returns 0 (dcNoErr) if completed without error
//	returns	"dc" error code if there was an error
//
////////

extern "C" int transitionDispatch(
    int		    selector,	/* esCleanup = -1, esExecute = 0, esSetup = 1 */
    PRX_Transition  theData
    )
{
    switch (selector) 
	{
	case esCleanup:
	    return dcNoErr;

	case esExecute:
	    return dissolveTransition(theData);
	    
	case esSetup:
	    setup(theData);
	    return dcNoErr;
	    
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

static PRX_SPFXOptionsRec options =
{
    0,			/* enable: which corners are enabled */
    0,			/* state: which corners are initially set */
    0,			/* flags */
    0,			/* exclusive: do corners act like radio buttons */
    0,			/* hasReverse */
    0,			/* hasEdges */
    0,			/* hasStart */
    0			/* hasEnd */
};

extern "C" PRX_PropertyList PRX_PluginProperties(
    unsigned /* apiVersion */
    )
{
    static PRX_KeyValueRec plugInProperties[] = {
	// Required properties for all plug-in types
	{ PK_APIVersion,	(void*) PRX_MAJOR_MINOR(1,0) },
	{ PK_DispatchSym,	(void*) "transitionDispatch" },
	{ PK_Name,		(void*) "SGI Cross Dissolve" },
	{ PK_UniquePrefix,	(void*) "SGI" },
	{ PK_PluginType,	(void*) PRX_TYPE_TRANSITION },
	{ PK_PluginVersion,	(void*) PRX_MAJOR_MINOR(1,0) },
	
	// Required properties for transitions
	{ PK_Description,	(void*) "Image A fades into image B." },
	{ PK_SPFXOptions,	(void*) &options },
	{ PK_SourceAUsage,	(void*) bufInputDrawPixels },
	{ PK_SourceBUsage,	(void*) bufInputDrawPixels },
	{ PK_DestUsage,		(void*) bufOutputOpenGL },

	// SMPTE Wipe code
	{ PK_WipeCodes_Len,	(void*) 0 },
	{ PK_WipeCodes,		(void*) fDISS },
	{ PK_WipeCodes_Dir,	(void*) NULL },

	{ PK_EndOfList,		(void*) 0 },
	
    };
    
    return (PRX_PropertyList) &plugInProperties;
}
