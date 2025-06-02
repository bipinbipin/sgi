////////////////////////////////////////////////////////////////////////
//
// Global.c++
//	
// Copyright 1996, Silicon Graphics, Inc.
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

#include "VideoKit.h"

VLServer Global::videoServer = NULL;
Display* Global::xDisplay    = NULL;

////////
//
// init
//
////////

void Global::init(
    )
{
    xDisplay = XOpenDisplay( "" );
    if ( xDisplay == NULL )
    {
	ErrorHandler::error( "Could not open X display" );
    }
    
    videoServer = vlOpenVideo( NULL );
    if ( videoServer == NULL )
    {
	ErrorHandler::vlError();
    }
    
}

////////
//
// cleanup
//
////////

void Global::cleanup(
    )
{
    XCloseDisplay( xDisplay );
    vlCloseVideo( videoServer );
}

////////
//
// chooseFBConfig
//
////////

GLXFBConfigSGIX Global::chooseFBConfig(
    Params* imageFormat,
    int depthSize,
    int stencilSize,
    DMboolean doubleBufferFlag
    )
{
    //
    // Figure out what size the color buffer needs to be based on the
    // pixel packing.
    //

    int colorSize;
    int alphaSize;
    switch ( dmParamsGetEnum( imageFormat->dmparams(), DM_IMAGE_PACKING ) )
    {
	case DM_IMAGE_PACKING_RGBA:
	case DM_IMAGE_PACKING_ABGR:
	    colorSize = 8;
	    alphaSize = 8;
	    break;
	    
	case DM_IMAGE_PACKING_XRGB1555:
	    colorSize = 5;
	    alphaSize = 0;
	    break;
	    
	default:
	    ErrorHandler::error( "Unknown pixel format in Global::chooseFBConfig" );
    }

    //
    // Get a list of possible configs.
    //

    int params[] = { 
	GLX_RENDER_TYPE_SGIX, 	GLX_RGBA_BIT_SGIX,
	GLX_DRAWABLE_TYPE_SGIX,	/*GLX_PBUFFER_BIT_SGIX |*/ GLX_WINDOW_BIT_SGIX,
	GLX_DEPTH_SIZE,		depthSize,
	GLX_STENCIL_SIZE,	stencilSize,
	GLX_X_RENDERABLE_SGIX,	False,
	GLX_DOUBLEBUFFER,	doubleBufferFlag,
	(int)None,
    };
    int configCount;
    GLXFBConfigSGIX* configs = 
	glXChooseFBConfigSGIX(
	    Global::xDisplay,
	    DefaultScreen(Global::xDisplay),
	    params,
	    &configCount
	    );

    if ( configs == NULL || configCount == 0 )
    {
	ErrorHandler::error( "glXChooseFBConfigSGIX failed" );
    }
    
    //
    // Find one that exactly matches the color size.  (This is
    // required for DM pbuffers.)
    //

    for ( int i = 0;  i < configCount;  i++ )
    {
	GLXFBConfigSGIX config = configs[i];

	int red;
	int green;
	int blue;
	int alpha;

	glXGetFBConfigAttribSGIX( Global::xDisplay,
				  config,
				  GLX_RED_SIZE,
				  &red );
	glXGetFBConfigAttribSGIX( Global::xDisplay,
				  config,
				  GLX_GREEN_SIZE,
				  &green );
	glXGetFBConfigAttribSGIX( Global::xDisplay,
				  config,
				  GLX_BLUE_SIZE,
				  &blue );
	glXGetFBConfigAttribSGIX( Global::xDisplay,
				  config,
				  GLX_ALPHA_SIZE,
				  &alpha );
	
	if ( red  == colorSize && green == colorSize && 
	     blue == colorSize && alpha == alphaSize )
	{
	    XFree( configs );
	    return config;
	}
    }
    
    fprintf( stderr, "colorSize    = %d\n", colorSize );
    fprintf( stderr, "alphaSize    = %d\n", alphaSize );
    fprintf( stderr, "doubleBuffer = %d\n", doubleBufferFlag );
    ErrorHandler::error( "No matching fb config was found." );
    
    // make the compiler happy:
    return configs[0];
}

////////
//
// theErrorFlag
//
////////

static DMboolean theErrorFlag = DM_FALSE;

////////
//
// theOldHandler
//
////////

static int (*theOldHandler) ( Display*, XErrorEvent* ) = NULL;

////////
//
// errorHandler
//
////////

static int errorHandler(
    Display*,
    XErrorEvent*
    )
{
    theErrorFlag = DM_TRUE;
    return 0; // return value is ignored
}
    
////////
//
// installErrorHandler
//
////////

void Global::installXErrorHandler(
    )
{
    //
    // This is probably overly paranoid.  It will cause any errors
    // that are in the pipe already to use the old handler.
    //

    XSync( Global::xDisplay, DM_FALSE );
    
    //
    // Clear the flag.  It will get set if an error occurs.
    //
    
    theErrorFlag = DM_FALSE;

    //
    // Install our error handler and 
    //

    theOldHandler = XSetErrorHandler( errorHandler );
}

////////
//
// removeErrorHandler
//
////////

void Global::removeXErrorHandler(
    const char* message
    )
{
    //
    // Force any errors out of the pipe.
    //

    XSync( Global::xDisplay, DM_FALSE );
    
    //
    // Replace the old error handler.
    //

    XSetErrorHandler( theOldHandler );
    
    //
    // Did an error happen?
    //

    if ( theErrorFlag )
    {
	// our caller ought to set the error code
	ErrorHandler::error( message );
    }
}

////////
//
// printFBConfig
//
////////

#define ARRAY_SIZE(a)  ( sizeof(a) / sizeof(a[0]) )

void Global::printFBConfig(
    GLXFBConfigSGIX config
    )
{
    static struct { int attrib; const char* name; } attribs [] = {
	{ GLX_BUFFER_SIZE, "GLX_BUFFER_SIZE" },
	{ GLX_LEVEL, "GLX_LEVEL" },
	{ GLX_DOUBLEBUFFER, "GLX_DOUBLEBUFFER" },
	{ GLX_AUX_BUFFERS, "GLX_AUX_BUFFERS" },
	{ GLX_RED_SIZE, "GLX_RED_SIZE" },
	{ GLX_GREEN_SIZE, "GLX_GREEN_SIZE" },
	{ GLX_BLUE_SIZE, "GLX_BLUE_SIZE" },
	{ GLX_ALPHA_SIZE, "GLX_ALPHA_SIZE" },
	{ GLX_DEPTH_SIZE, "GLX_DEPTH_SIZE" },
	{ GLX_STENCIL_SIZE, "GLX_STENCIL_SIZE" },
	{ GLX_DRAWABLE_TYPE_SGIX, "GLX_DRAWABLE_TYPE_SGIX" },
	{ GLX_RENDER_TYPE_SGIX, "GLX_RENDER_TYPE_SGIX" },
	{ GLX_X_RENDERABLE_SGIX, "GLX_X_RENDERABLE_SGIX" },
	{ GLX_MAX_PBUFFER_WIDTH_SGIX, "GLX_MAX_PBUFFER_WIDTH_SGIX" },
	{ GLX_MAX_PBUFFER_HEIGHT_SGIX, "GLX_MAX_PBUFFER_HEIGHT_SGIX"},
    };
    
    for ( int i = 0;  i < ARRAY_SIZE( attribs );  i++ )
    {
	int value;
	int errorCode = glXGetFBConfigAttribSGIX(
	    Global::xDisplay, config, attribs[i].attrib, &value );
	assert( errorCode == 0 );
	printf( "    %24s = %d\n", attribs[i].name, value );
    }
}

