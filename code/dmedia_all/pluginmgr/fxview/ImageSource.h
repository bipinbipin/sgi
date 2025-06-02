////////////////////////////////////////////////////////////////////////
//
// ImageSource.h
//	
//	Abstract base class for reading images.  The source can be
//	an image file, a movie file, ...
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

#ifndef _ImageSource_H_
#define _ImageSource_H_

#include "VfxTime.h"

#include <sys/types.h>		// size_t
#include <X11/Intrinsic.h>	// Boolean
#include <dmedia/fx_buffer.h>	// DMfxbuffer

////////////////////////////////////////////////////////////////////////
//
// ImageSource class
//
////////////////////////////////////////////////////////////////////////

class ImageSource
{
public:
    //
    // openFile() -- figures out what kind of file the given file is,
    // and opens it if possible.  Returns NULL if the file could not
    // be opened.
    //

    static ImageSource* openFile(
	int 		width,
	int 		height,
	DMfxbuffer*	buffer,
	Boolean		keepAspect,
	const char*	fileName
	);

    //
    // solidColor() -- creates an image source that always returns an
    // image that is a solid color.
    //

    static ImageSource* solidColor(
	int		width,
        int		height,
	DMfxbuffer*	buffer,
        unsigned char	red,
        unsigned char	green,
        unsigned char	blue,
        unsigned char	alpha
        );

    static ImageSource* createBuffer(
	int		width,
	int		height,
	DMfxbuffer*	buffer
	);
    
    //
    // every base class needs a virtual destructor.
    //

    virtual ~ImageSource();

    //
    // attributes of the image(s).  Movies have a duration, but images
    // return a duration of 0.
    //
    
    virtual VfxTime 	getDuration() 	= 0;
    virtual int		getWidth()	= 0;
    virtual int		getHeight()	= 0;

    //
    // getBuffer() -- returns the underlying DMfxbuffer that holds the
    // image. 
    //
    
    virtual DMfxbuffer*	getBuffer()	= 0;

    //
    // setTime() -- for movies, selects which frame to use.  Returns
    // TRUE if the image changed from the last time setTime was
    // called.  The default time before setTime() is called is 0.
    //

    virtual Boolean setTime( VfxTime ) = 0;
    
    //
    // clear() -- stored gray in the buffer.
    //

    virtual void clear( int r, int g, int b, int a ) = 0;
    void clear() { clear( 80, 80, 80, 255 ); }
    
    //
    // loadImage() -- copies (and resizes) an image into the buffer.
    //

    virtual void loadImage( int width, int height, int rowBytes, void* buffer ) = 0;

    //
    // getImageAtTime() -- used in the callback from a plug-in, when
    // it's requesting a specific frame.
    //

    virtual void getImageAtTime(
	VfxTime time,
	int width,
	int height,
	int rowBytes,
	void* buffer
	) = 0;
};

#endif // _ImageSource_H_
