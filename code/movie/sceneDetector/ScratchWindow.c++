////////////////////////////////////////////////////////////////////////
//
// ScratchWindow.c++
//	
//	A window to render movies in to do special effects with OpenGL.
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

#include "ScratchWindow.h"

#include "Format.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glu.h>
#include <GL/glx.h>

#include <assert.h>		// assert()
#include <stdio.h>		// fprintf()
#include <string.h>		// ffs()
#include <math.h>		// M_PI_2, tan()

static inline float maxf(
    float a,
    float b
    )
{
    if ( a < b )
    {
	return b;
    }
    else
    {
	return a;
    }
}

////////////////////////////////////////////////////////////////////////
//
// Globals
//
////////////////////////////////////////////////////////////////////////

static Display*		theDisplay 	= NULL;
static int		theScreen  	= 0;
static XVisualInfo*	theVisualInfo  	= NULL;
static Colormap		theColormap	= NULL;

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

////////
//
// degreesToRadians
//
////////

static inline float degreesToRadians(
    float radians
    )
{
    return radians * M_PI / 180.0;
}

////////////////////////////////////////////////////////////////////////
//
// scratchWindow implementation class
//
////////////////////////////////////////////////////////////////////////

class scratchWindow : public ScratchWindow
{
private:
    GLboolean	myValid;
    int		myWidth;
    int		myHeight;
    Window	myWindow;
    GLXContext	myContext;
    
public:
    //
    // Constructors and destructors
    //

    scratchWindow( int width, int height );
    ~scratchWindow();
    
    //
    // ScratchWindow methods.
    //

    void   setContext();
    void   perspective( float heightAngle, float near, float far );
    int    getWidth()		{ return myWidth; }
    int    getHeight()		{ return myHeight; }
    size_t getBufferSize();
    
    void readRgbaImage ( void* buffer, size_t bufferSize );
    void writeRgbaImage( void* buffer, size_t bufferSize );
    
    //
    // Methods used by create()
    //

    GLboolean isValid() { return myValid; }
    
private:
    //
    // Private methods.
    //

    GLboolean findColormap();
    GLboolean findVisual();
    GLboolean openDisplay();
};

////////
//
// constructor
//
////////

scratchWindow::scratchWindow(
    int width,
    int height
    )
{
    assert( 0 < width );
    assert( 0 < height );

    myWidth  = width;
    myHeight = height;

    //
    // Make sure that we have a display open.
    //

    if ( ! this->openDisplay() ) {
	myValid = GL_FALSE;
	return;
    }
    
    //
    // Find a visual of the correct type.
    //

    if ( ! this->findVisual() ) {
	myValid = GL_FALSE;
	return;
    }
    
    //
    // Create the graphics context.
    //

    myContext = glXCreateContext(
	theDisplay,
	theVisualInfo,
	NULL,		// Don't share display lists with other contexts
	GL_TRUE		// Use direct rendering, if possible
	);
    
    //
    // Set up the color map.
    //

    if ( ! this->findColormap() ) {
	myValid = GL_FALSE;
	return;
    }
    
    //
    // Create the window
    //

    {
	XSetWindowAttributes wa;
	wa.background_pixel = 0xFFFFFFFF;
	wa.border_pixel     = 0x00000000;
	wa.colormap         = theColormap;
	wa.event_mask	    = StructureNotifyMask | ExposureMask;
	myWindow = XCreateWindow(
	    theDisplay,
	    RootWindow( theDisplay, theScreen ),
	    0,
	    0,
	    width,
	    height,
	    0,
	    theVisualInfo->depth,
	    InputOutput,
	    theVisualInfo->visual,
	    CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
	    &wa
	    );
    }
    
    //
    // Set the title on the window
    //

    XSetStandardProperties(
	theDisplay, 
	myWindow, 
	"Scratch Window",
	"Scratch Window",
	None,
	0,
	0,
	0
	);

    //
    // Map the window.  (Wait until the window it's done.)
    //

    XMapWindow( theDisplay, myWindow );
    {
	XEvent e;
	XIfEvent( theDisplay, &e, WaitForMapNotify, (XPointer) &myWindow );
    }
    
    //
    // Creating a window makes it be the current graphics context.
    //

    this->setContext();
    
    //
    // Clear the window.
    //

    glLoadIdentity();
    glClearColor( 0.0, 0.0, 0.0, 1.0 );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glFlush();

    //
    // All done.
    //

    myValid = GL_TRUE;
}

////////
//
// destructor
//
////////

scratchWindow::~scratchWindow(
    )
{
    glXDestroyContext( theDisplay, myContext );
    XDestroyWindow( theDisplay, myWindow );
}

////////
//
// findColormap
//
////////

GLboolean scratchWindow::findColormap(
    )
{
    //
    // If we've already created a color map, do nothing.
    //

    if ( theColormap != NULL ) {
	return GL_TRUE;
    }
    
    //
    // Create a color map.
    //

    theColormap = 
	XCreateColormap(
	    theDisplay,
	    RootWindow( theDisplay, theScreen ),
	    theVisualInfo->visual,
	    AllocNone
	    );
    // XXX What does it return when it fails?
    
    //
    // We assume that this is a true-color visual, so it is not
    // neceserry to set up any colors.
    //

    assert( theVisualInfo->c_class == TrueColor );
    
    XSync( theDisplay, 0 );  // XXX Is this needed if no colors were
			     // set up?

    return GL_TRUE;
}


////////
//
// findVisual
//
////////

GLboolean scratchWindow::findVisual(
    )
{
    if ( theVisualInfo != NULL ) {
	return GL_TRUE;
    }
    
    int params[] = { 
	GLX_RGBA, 
	GLX_RED_SIZE, 8,
	GLX_GREEN_SIZE, 8,
	GLX_BLUE_SIZE, 8,
	// GLX_ALPHA_SIZE, 8,
	GLX_DEPTH_SIZE, 1,    // XXX ?
	(int)None,
    };
    
    theVisualInfo = glXChooseVisual( theDisplay, theScreen, (int*) params );
    
    return ( theVisualInfo != NULL );
}

////////
//
// getBufferSize
//
////////

size_t scratchWindow::getBufferSize(
    )
{
    // XXX This is hard-coded for RGB
    return myWidth * myHeight * 4 * sizeof( GLubyte );
}


////////
//
// openDisplay
//
////////

GLboolean scratchWindow::openDisplay(
    )
{
    //
    // If it's already open, don't do anything.
    //

    if ( theDisplay != NULL ) {
	return GL_TRUE;
    }
    
    //
    // Open the display.
    //

    theDisplay = XOpenDisplay(0);
    if ( theDisplay == NULL ) {
	fprintf(stderr, "Can't connect to display!\n");
	return GL_FALSE;
    }
    
    //
    // Make sure that it supports OpenGL.
    //

    {
	int erb, evb;
	if ( ! glXQueryExtension( theDisplay, &erb, &evb ) ) {
	    fprintf(stderr, "No glx extension!\n");
	    XCloseDisplay( theDisplay );
	    theDisplay = NULL;
	    return GL_FALSE;
	}
    }
    
    //
    // Get the screen number.
    //

    
    theScreen = DefaultScreen(theDisplay);

    //
    // All done.
    //

    return GL_TRUE;
}

////////
//
// perspective
//
////////

void scratchWindow::perspective(
    float heightAngle,
    float near,
    float far
    )
{
    //
    // Check arguments
    //
    
    assert( 0.0 < heightAngle && heightAngle < 180.0 );
    assert( far <= 0.0 && 0.0 <= near );

    //
    // The distance from the scene to the camera is just enough so
    // that the viewing rectangle occupies the entire height.
    //

    float heightRadians = degreesToRadians( heightAngle );
    float cameraDistance =  
	float(myHeight) / ( 2.0 * tan(heightRadians / 2.0) );
    
    //
    // The scene must be moved by the camera distance, because the
    // camera is at the origin.  It must also be translated so that
    // the z axis goes through the center of the scene.
    //

    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    glTranslatef(
	-float(myWidth) / 2.0,
	-float(myHeight) / 2.0,
	-cameraDistance
	);
    
    //
    // The projection transformation is set up based on the height
    // angle. 
    //

    {
	float aspectRatio  = float(myWidth) / float(myHeight);
	float cameraToNear = cameraDistance - near;
	float cameraToFar  = cameraDistance - far;
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	gluPerspective(
	    heightAngle,
	    aspectRatio,
	    maxf( cameraToNear, 0.0 ),
	    cameraToFar
	    );
    }
    
}

////////
//
// readRgbaImage
//
////////

void scratchWindow::readRgbaImage(
    void*	buffer,
    size_t	bufferSize
    )
{
    assert( this->getBufferSize() <= bufferSize );
    
    glReadPixels(
	0, 0,			// origin of rectangle,
	myWidth, myHeight,	// size of rectangle
	PIXEL_FORMAT,		// pixel format
	PIXEL_TYPE,		// type of data
	buffer
	);
}

////////
//
// setContext
//
////////

void scratchWindow::setContext(
    )
{
    GLboolean status = glXMakeCurrent( theDisplay, myWindow, myContext );
    assert( status );
}

////////
//
// writeRgbaImage
//
////////

void scratchWindow::writeRgbaImage(
    void*	buffer,
    size_t	bufferSize
    )
{
    assert( this->getBufferSize() <= bufferSize );
    
    //
    // Set the position for the lower-left corner of the screen
    // rectangle where the image will go.  To avoid roundoff-error
    // problems, the location specified is in the *middle* of the
    // pixel at the lower-left corner.
    //

    glRasterPos3f( 0.5, 0.5, 0.0 );
    
    //
    // Copy the pixels to the frame buffer.
    //

    glDrawPixels(
	myWidth,
	myHeight,
	PIXEL_FORMAT,
	PIXEL_TYPE,
	buffer
	);
}

////////////////////////////////////////////////////////////////////////
//
// ScratchWindow methods
//
////////////////////////////////////////////////////////////////////////

////////
//
// create
//
////////

ScratchWindow* ScratchWindow::create(
    int width,
    int height
    )
{
    scratchWindow* window = new scratchWindow( width, height );
    
    if ( window->isValid() ) {
	return window;
    }
    else {
	delete window;
	return NULL;
    }
}

////////
//
// Destructor
//
////////

ScratchWindow::~ScratchWindow(
    )
{
}
