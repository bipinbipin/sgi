////////////////////////////////////////////////////////////////////////
//
// View.h
//	
//	A class to manage an image source and display it in one of the
//	view windows.
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

#ifndef _View_H_
#define _View_H_

#include "VfxTime.h"
#include <X11/Intrinsic.h>		// Widget
#include <sys/types.h>			// size_t

class ImageSource;

class View
{
public:
    static View* create( Widget glWidget );
    virtual ~View();
    
    //
    // setSource() -- get images from a new source.  The ownership of
    // the ImageSource object is passed in.  It will be deleted the
    // next time the source in changed.
    //

    virtual void setSource( ImageSource* ) = 0;

    //
    // getSource() -- return a pointer to the current source.  This
    // object retains ownership of it.
    //
    
    virtual ImageSource* getSource() = 0;

    //
    // setTime() -- set the time used to select a frame from a movie.
    //

    virtual void setTime( VfxTime ) = 0;

    //
    // redraw() -- repaints the screen.  It is used when the image
    // source's contents change.
    //
    
    virtual void redraw() = 0;
};

#endif // _View_H_

