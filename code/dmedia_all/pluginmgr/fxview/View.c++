////////////////////////////////////////////////////////////////////////
//
// View.c++
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

#include "View.h"

#include "ImageSource.h"
#include <dmedia/fx_buffer.h>
#include <dmedia/fx_image_viewer.h>

#include <assert.h>
#include <bstring.h>	// bzero()
#include <stdio.h>
#include <stdlib.h>	// exit(), malloc(), free()

static void reportDMError(
    )
{
    int errorNum;
    char detail[DM_MAX_ERROR_DETAIL];
    const char* message;
    message = dmGetError( &errorNum, detail );
    assert( message != NULL );
    fprintf( stderr, "Error: %s\n", detail );
    exit( 1 );
}

////////////////////////////////////////////////////////////////////////
//
// view implementation class
//
////////////////////////////////////////////////////////////////////////

class view : public View
{
public:
    //
    // Constructors and destructors
    //
    
    view( Widget glWidget );
    ~view();
    
    //
    // View methods
    //

    void   setSource( ImageSource* );
    void   setTime( VfxTime );
    void   redraw();
    
    ImageSource* getSource()
    {
	return mySource;
    }
    
private:
    ImageSource*	mySource;
    DMFXImageViewer*	myViewer;
    VfxTime		myTime;
    
    void storeImage();
};

////////
//
// Constructor
//
////////

view::view(
    Widget	glWidget
    )
{
    //
    // There is no default source
    //

    mySource = NULL;

    //
    // Create the viewer object
    //

    myViewer = new DMFXImageViewer( glWidget );
    if ( myViewer == NULL ) {
	fprintf( stderr, "Memory allocation failed.\n" );
	exit( 1 );
    }

    //
    // Set the default time.
    //

    myTime = VfxTime::fromNanoseconds( 0 );
    
    //
    // Show the default image.
    //

    this->redraw();
}

////////
//
// destructor
//
////////

view::~view(
    )
{
    if ( mySource != NULL ) {
	delete mySource;
	mySource = NULL;
    }
    if ( myViewer != NULL ) {
	delete myViewer;
	myViewer = NULL;
    }
}

////////
//
// redraw
//
////////

void view::redraw(
    )
{
    // dmFXGetDataPtr is not guaranteed to return the same pointer
    // each time, so be sure to re-load it.
    this->storeImage();
}


////////
//
// storeImage
//
////////

void view::storeImage(
    )
{
    if ( mySource != NULL )
    {
	DMfxbuffer* buffer = mySource->getBuffer();
	DMstatus status;
	status = dmFXSetupInputImageBufferWithUsage(
	    bufInputDirect, buffer );
	if ( status != DM_SUCCESS )
	{
	    reportDMError();
	}

	myViewer->setBuf( dmFXGetDataPtr( buffer ), 
			  mySource->getWidth(),
			  mySource->getHeight(),
			  dmFXGetRowLength( buffer ) * 4 );
	status = dmFXCleanupInputImageBuffer( buffer );
	if ( status != DM_SUCCESS )
	{
	    reportDMError();
	}
    }
    else
    {
	myViewer->setBuf( NULL, 0, 0, 0 );
    }
}

////////
//
// setSource
//
////////

void view::setSource(
    ImageSource* newSource
    )
{
    if (mySource != NULL)
    {
	delete mySource;
    }

    mySource = newSource;

    this->storeImage();
}

////////
//
// setTime
//
////////

void view::setTime(
    VfxTime	newTime
    )
{
    myTime = newTime;
    if ( mySource != NULL )
    {
	mySource->setTime( myTime );
    }
    this->storeImage();
}


////////////////////////////////////////////////////////////////////////
//
// View class
//
////////////////////////////////////////////////////////////////////////

////////
//
// create
//
////////

View* View::create(
    Widget	glWidget
    )
{
    return new view( glWidget );
}

////////
//
// destructor
//
////////

View::~View(
    )
{
}
