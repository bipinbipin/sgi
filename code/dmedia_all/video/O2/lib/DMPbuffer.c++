////////////////////////////////////////////////////////////////////////
//
// DMPbuffer.c++
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

////////////////////////////////////////////////////////////////////////
//
// Implementation class
//
////////////////////////////////////////////////////////////////////////

class DMPbufferImpl : public DMPbuffer
{
public:
    //
    // Constructor
    //

    DMPbufferImpl(
	GLXFBConfigSGIX	fbconfig,
	Params*		imageFormat
	);
    
    //
    // Destructor
    //

    ~DMPbufferImpl()
    {
	glXDestroyGLXPbufferSGIX( Global::xDisplay, myPbuffer );
    }
    
    //
    // getPoolRequirements
    //

    void getPoolRequirements( Params* poolSpec )
    {
	dmBufferGetGLPoolParams( myFormat->dmparams(), poolSpec->dmparams() );
	// dmBufferGetGLPoolParams doesn't set buffer count.
	dmParamsSetInt( poolSpec->dmparams(), DM_BUFFER_COUNT, 1 );
    }
    
    //
    // setPool
    //

    virtual void setPool( Pool* )
    {
    }
    
    //
    // drawable
    //

    GLXDrawable drawable()
    {
	return myPbuffer;
    }
    
    //
    // associate
    //

    void associate( Buffer* buffer )
    {
	Bool ok = glXAssociateDMPbufferSGIX(
	    Global::xDisplay,
	    myPbuffer,
	    myFormat->dmparams(),
	    buffer->dmbuffer()
	    );
	if ( ! ok ) 
	{
	    ErrorHandler::error( "Could not associate dm pbuffer" );
	}
    }
    

private:
    GLXPbufferSGIX	myPbuffer;
    Ref<Params>		myFormat;
};

////////
//
// Constructor
//
////////

DMPbufferImpl::DMPbufferImpl(
    GLXFBConfigSGIX	fbconfig,
    Params*		imageFormat
    )
{
    myFormat = imageFormat;

    int width  = myFormat->width();
    int height = myFormat->height();
    
    int attribs [] = {
	GLX_DIGITAL_MEDIA_PBUFFER_SGIX, True,
	GLX_PRESERVED_CONTENTS_SGIX, True,
	(int) None
    }; 

    Global::installXErrorHandler();
    myPbuffer = glXCreateGLXPbufferSGIX(
	Global::xDisplay,
	fbconfig,
	width,
	height,
	attribs
	);
    Global::removeXErrorHandler( "Could not create dm pbuffer" );
    if ( myPbuffer == None )
    {
	ErrorHandler::error( "Could not create dm pbuffer" );
    }
}

////////////////////////////////////////////////////////////////////////
//
// Static methods
//
////////////////////////////////////////////////////////////////////////

////////
//
// create
//
////////

Ref<DMPbuffer> DMPbuffer::create(
    GLXFBConfigSGIX	fbconfig,
    Params*		imageFormat
    )
{
    return new DMPbufferImpl( fbconfig, imageFormat );
}

