////////////////////////////////////////////////////////////////////////
//
// ScratchWindow.h
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

#ifndef _ScratchWindow_H
#define _ScratchWindow_H

#include <sys/types.h>		// size_t

class ScratchWindow
{
public:
    // The create function returns NULL if the window could not be
    // created.  The window is guaranteed to have 8 bits each of red,
    // greed, blue, and alpha.
    static ScratchWindow* create( int width, int height );
    ~ScratchWindow();

    // setContext() makes this window the current GL context.  The
    // coordinate system is set up so that the rectangle from 
    // (0,0,0) to (width-1,height-1,0) will fill the window.
    virtual void   setContext()		= 0;

    // perspective() sets up both the modelview and perspective
    // transformations so that the rectangle from (0,0,0) to
    // (width,height,0) will fill the window.  The height angle given
    // will determine how far the camera is from the scene.
    // NOTE: the near and far are Z coordinates relative to the
    // rectangle above.  They are *not* distances from the camera.  If
    // your effect rotates an image about the Y axis, it will come
    // forward and go back half the width of the image, and you could
    // call perspective like this:
    //      perspective( 30.0, width/2, -width/2 );
    virtual void   perspective(
	float	heightAngleInDegrees,
	float	near,
	float	far
	) = 0;

    virtual int    getWidth() 		= 0;
    virtual int    getHeight() 		= 0;
    virtual size_t getBufferSize()	= 0;
    
    virtual void readRgbaImage ( void* buffer, size_t bufferSize ) = 0;
    virtual void writeRgbaImage( void* buffer, size_t bufferSize ) = 0;
};

#endif // _ScratchWindow_H
