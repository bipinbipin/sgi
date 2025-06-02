////////////////////////////////////////////////////////////////////////
//
// Context.c++
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

#include <GL/glu.h>

////////////////////////////////////////////////////////////////////////
//
// Implementation class
//
////////////////////////////////////////////////////////////////////////

class ContextImpl : public Context
{
public:
    //
    // Constructor
    //

    ContextImpl(
	GLXFBConfigSGIX fbconfig,
	Params*		format
	)
    {
	Global::installXErrorHandler();
	myContext = glXCreateContextWithConfigSGIX(
	    Global::xDisplay,
	    fbconfig,
	    GLX_RGBA_TYPE_SGIX,
	    NULL,
	    True
	    );
	Global::removeXErrorHandler( "glXCreateContextWithConfigSGIX failed" );
	if ( myContext == NULL )
	{
	    ErrorHandler::error( "glXCreateContextWithConfigSGIX failed" ); 
	}
	myTransformsDone = DM_FALSE;
	myFormat = format;
    }
    
    //
    // Destructor
    //

    ~ContextImpl()
    {
	glXDestroyContext( Global::xDisplay, myContext );
    }
    
    //
    // makeCurrent
    //

    void makeCurrent(
	GLXDrawable draw
	)
    {
	if ( ! myTransformsDone )
	{
	    Global::installXErrorHandler();
	}
	Bool ok = glXMakeCurrent( Global::xDisplay, draw, myContext ); 
	if ( ! myTransformsDone )
	{
	    Global::removeXErrorHandler( "glXMakeCurrent failed" );
	}
	if ( ! ok )
	{
	    ErrorHandler::error( "glXMakeCurrent failed" ); 
	}
	this->setupTransforms();
    }
    
    //
    // makeCurrentRead
    //

    void makeCurrentRead(
	GLXDrawable draw,
	GLXDrawable read
	)
    {
	if ( ! myTransformsDone )
	{
	    Global::installXErrorHandler();
	}
	Bool ok = glXMakeCurrentReadSGI( Global::xDisplay, draw, 
					 read, myContext ); 
	if ( ! myTransformsDone )
	{
	    Global::removeXErrorHandler( "glXMakeCurrentReadSGI failed" );
	}
	if ( ! ok )
	{
	    ErrorHandler::error( "glXMakeCurrentRead failed" ); 
	}
	this->setupTransforms();
    }
    
    
private:
    GLXContext	myContext;
    DMboolean	myTransformsDone;
    Ref<Params> myFormat;
    
    //
    // setupTransforms
    //

    void setupTransforms()
    {
	if ( myTransformsDone )
	{
	    return;
	}
	
	myTransformsDone = DM_TRUE;
	
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	gluOrtho2D( 0, myFormat->width(), 0, myFormat->height() );
    }
};

////////////////////////////////////////////////////////////////////////
//
// Static members
//
////////////////////////////////////////////////////////////////////////

Ref<Context> Context::create(
    GLXFBConfigSGIX	fbconfig,
    Params*		imageFormat
    )
{
    return new ContextImpl( fbconfig, imageFormat );
}
