////////////////////////////////////////////////////////////////////////
//
// NoopConvolveFilter.c++
//
//	A video filter that does nothing by convolving.
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

#include <assert.h>
#include <stdio.h>		// XXX just for debugging

#include <GL/glx.h>

#include <dmedia/PremiereXAPI.h>

#include <dmedia/fx_buffer.h>
#include <dmedia/fx_utils.h>	// DM_FX_4_CHARS_TO_INT

#define VERSION     PRX_MAJOR_MINOR(1,0)
#define SIGNATURE   DM_FX_4_CHARS_TO_INT( 'n', 'o', 'p', 'c' )
#define PLUGIN_NAME	"No-op (convolve)"

#define	kernelWidth	3
#define	kernelHeight	3

#define	convBias	0
#define	convScale	1

static float kernelCoeff[] =
{
	(0.0/1),	(0.0/1),	(0.0/1),
	(0.0/1),	(1.0/1),	(0.0/1),
	(0.0/1),	(0.0/1),	(0.0/1)
};

////////
//
// initialize
//
////////

static void initialize(
    PRX_VideoFilter theData)
{
    //
    // If this is the first time, set up the default values.
    //
    
    if (PRX_GetState(theData) == stNew)
    {
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
    PRX_VideoFilter theData)
{
    initialize(theData);
    
    //
    // This plug-in does not have a dialog box, so there is nothing to do.
    //
}

////////
//
// copyFrame
//
////////

int copyFrame(
    PRX_VideoFilter	theData)
{
    initialize(theData);

    //
    // Define image processing pipeline that will operate on the src image,
    // and render that image.
    //

    if (dmFXConvolve(theData->dst, theData->src, kernelWidth, kernelHeight,
			kernelCoeff, convBias, convScale) != 0)
	return dcOtherErr;

    return dcNoErr;
}

////////
//
// dispatchFunction
//
////////

extern "C" int dispatchFunction(
    int			selector,
    PRX_VideoFilter	theData
    )
{
    switch (selector) 
    {
	case fsCleanup:
	    return dcNoErr;

	case fsExecute:
	    return copyFrame(theData);
	    
	case fsSetup:
	    setup(theData);
	    return dcNoErr;
	    
	default:
	    assert(False);
	    return dcUnsupported;
    }
}
	
extern "C" PRX_PropertyList PRX_PluginProperties(
    unsigned /* apiVersion */)
{
    static PRX_KeyValueRec plugInProperties[] =
    {
	{ PK_APIVersion,	(void*) PRX_MAJOR_MINOR(1,0) },
	{ PK_DispatchSym,	(void*) "dispatchFunction" },
	{ PK_Name,		(void*) PLUGIN_NAME },
	{ PK_UniquePrefix,	(void*) "SGI" },
	{ PK_PluginType,	(void*) PRX_TYPE_VIDEO_FILTER },
	{ PK_PluginVersion,	(void*) PRX_MAJOR_MINOR(1,0) },
	{ PK_SourceAUsage,	(void*) bufInputDrawPixels },
	{ PK_DestUsage,		(void*) bufOutputOpenGL },

	{ PK_EndOfList,		(void*) 0 },
    };
    
    return (PRX_PropertyList) &plugInProperties;
}

