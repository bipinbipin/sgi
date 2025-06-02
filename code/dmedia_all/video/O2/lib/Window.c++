////////////////////////////////////////////////////////////////////////
//
// GLWindow.c++
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

////////
//
// WaitForMapNotify
//
////////

static int WaitForMapNotify(
    Display*,
    XEvent* e,
    XPointer clientData
    )
{
    Window* pw = (Window*) clientData;
    
    if ( e->type == MapNotify && e->xmap.window == *pw ) {
	return GL_TRUE;
    }
    return GL_FALSE;
}

////////////////////////////////////////////////////////////////////////
//
// Implementation class
//
////////////////////////////////////////////////////////////////////////

class GLWindowImpl : public GLWindow
{
public:
    //
    // Constructor
    //

    GLWindowImpl(
	const char* name,
	GLXFBConfigSGIX	fbconfig,
	Params*		imageFormat
	);
    
    //
    // Destructor
    //

    ~GLWindowImpl();
    
    //
    // drawable
    //

    GLXDrawable drawable()
    {
	return myWindow;
    }
    
    void swapBuffers()
    {
	glXSwapBuffers( Global::xDisplay, myWindow );
    }
    
    //
    // xwindow
    //

    Window xwindow()
    {
	return myWindow;
    }
    
private:
    Window	myWindow;
};

////////
//
// Constructor
//
////////

GLWindowImpl::GLWindowImpl(
    const char* 	name,
    GLXFBConfigSGIX	fbconfig,
    Params*		imageFormat
    )
{
    int width  = imageFormat->width();
    int height = imageFormat->height();
    
    XVisualInfo* visualInfo = glXGetVisualFromFBConfigSGIX(
	Global::xDisplay, fbconfig );
    
    //
    // Create a colormap
    //
    
    Colormap colormap = 
	XCreateColormap(
	    Global::xDisplay,
	    RootWindow( Global::xDisplay, DefaultScreen(Global::xDisplay) ),
	    visualInfo->visual,
	    AllocNone
	    );

    //
    // Create the window
    //

    {
	XSetWindowAttributes wa;
	wa.background_pixel = 0xFFFFFFFF;
	wa.border_pixel     = 0x00000000;
	wa.colormap         = colormap;
	wa.event_mask	    = StructureNotifyMask | ExposureMask |
	PointerMotionMask | ButtonPressMask | ButtonReleaseMask ;
	
	myWindow = XCreateWindow(
	    Global::xDisplay,
	    RootWindow( Global::xDisplay, DefaultScreen(Global::xDisplay) ),
	    0,
	    0,
	    width,
	    height,
	    0,
	    visualInfo->depth,
	    InputOutput,
	    visualInfo->visual,
	    CWBackPixel | CWBorderPixel | CWColormap  | CWEventMask,
	    &wa
	    );
    }
    
    //
    // Set the title on the window
    //

    XSetStandardProperties(
	Global::xDisplay, 
	myWindow, 
	name,
	name,
	None,
	0,
	0,
	0
	);

    //
    // Map the window.  (Wait until the window it's done.)
    //

    XMapWindow( Global::xDisplay, myWindow );
    {
	XEvent e;
	XIfEvent( Global::xDisplay, &e, WaitForMapNotify,
		  (XPointer) &myWindow );
    }

    //
    // Clean up
    //

    XFree( visualInfo );
}

////////
//
// Destructor
//
////////

GLWindowImpl::~GLWindowImpl(
    )
{
    XUnmapWindow( Global::xDisplay, myWindow );
    XDestroyWindow( Global::xDisplay, myWindow );
    XFlush( Global::xDisplay );
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

Ref<GLWindow> GLWindow::create(
    const char* 	name,
    GLXFBConfigSGIX	fbconfig,
    Params*		imageFormat
    )
{
    return new GLWindowImpl( name, fbconfig, imageFormat );
}

