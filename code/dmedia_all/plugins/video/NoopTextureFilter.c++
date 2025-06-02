////////////////////////////////////////////////////////////////////////
//
// noop.c++
//
//	A video filter that just copies its input to its output. 
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

#include <dmedia/fx_buffer.h>
#include <dmedia/fx_utils.h>

#include <assert.h>
#include <bstring.h>		// bcopy()
#include <stdio.h>		// XXX just for debugging

#define FALSE 0

#define SIGNATURE   DM_FX_4_CHARS_TO_INT( 'n', 'o', 'p', '4' )
#define VERSION     PRX_MAJOR_MINOR(1,0)


////////
//
// initialize
//
////////

static void initialize(
    PRX_VideoFilter theData
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
    PRX_VideoFilter theData
    )
{
    initialize(theData);
    
    //
    // This plug-in does not have a dialog box, so there is nothing to do.
    //
}


////////
//
// filter
//
////////

int filter(
    PRX_VideoFilter	theData
    )
{
    initialize(theData);

    //
    // Load the source image as a texture.
    //

    glMatrixMode( GL_TEXTURE );
    glPushMatrix();
    dmFXTexImage2D( theData->dst, theData->src, DM_FALSE );
    
    //
    // Mipmapping is on by default in GL, so turn it off.
    //

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    
    //
    // Draw a rectangle.  The color must be white (including alpha),
    // and the mode MODULATE (the default), so that the alpha gets
    // copied to the destination.
    //

    glEnable( GL_TEXTURE_2D );
    glColor4f( 1.0, 1.0, 1.0, 1.0 );
    
    glBegin(GL_QUADS); {
        float w = float(theData->src->bounds.width);
        float h = float(theData->src->bounds.height);
        glTexCoord2f( 0.0, 0.0 ); glVertex3f( 0.0, 0.0, 0.0 );
        glTexCoord2f( 0.0, 1.0 ); glVertex3f( 0.0, h,   0.0 );
        glTexCoord2f( 1.0, 1.0 ); glVertex3f( w,   h,   0.0 );
        glTexCoord2f( 1.0, 0.0 ); glVertex3f( w,   0.0, 0.0 );
    } glEnd();
    
    //
    // Restore GL state.
    //

    glMatrixMode( GL_TEXTURE );
    glPopMatrix();
    glMatrixMode( GL_MODELVIEW );
    glDisable( GL_TEXTURE_2D );
    
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    
    //
    // All done.
    //

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
    switch ( selector ) 
	{
	case fsCleanup:
	    return dcNoErr;

	case fsExecute:
	    return filter( theData );
	    
	case fsSetup:
	    setup( theData );
	    return dcNoErr;
	    
	default:
	    assert( FALSE );
	    return dcUnsupported;
	}
}
	
////////
//
// PRX_PluginProperties
//
////////

extern "C" PRX_PropertyList PRX_PluginProperties(
    unsigned /* apiVersion */
    )
{
    static PRX_KeyValueRec plugInProperties[] = {
	{ PK_APIVersion,	(void*) PRX_MAJOR_MINOR(1,0) },
	{ PK_DispatchSym,	(void*) "dispatchFunction" },
	{ PK_Name,		(void*) "No-op (Texture)" },
	{ PK_UniquePrefix,	(void*) "SGI" },
	{ PK_PluginType,	(void*) PRX_TYPE_VIDEO_FILTER },
	{ PK_PluginVersion,	(void*) PRX_MAJOR_MINOR(1,0) },
	{ PK_SourceAUsage,	(void*) bufInputTexture },
	{ PK_DestUsage,		(void*) bufOutputOpenGL },
	{ PK_EndOfList,		(void*) 0 },
    };
    
    return (PRX_PropertyList) &plugInProperties;
}

