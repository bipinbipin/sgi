////////////////////////////////////////////////////////////////////////
//
// Movie.c++
//	
//	A simple movie object: opens a movie file and reads images.
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

#include "Movie.h"

#include "Format.h"

#include <assert.h>
#include <dmedia/moviefile.h>	// mvOpen(), mvReadFrames(), etc.
#include <stdio.h>		// XXX

////////////////////////////////////////////////////////////////////////
//
// movie implementation class
//
////////////////////////////////////////////////////////////////////////

class movie : public Movie
{
private:
    Boolean	myValid;		// The rest of the data
					// members are only valid when
					// this is TRUE;
    
    MVid	myMovie;
    MVid	myImageTrack;
    MVtimescale	myTimescale;
    MVtime	myFrameDuration;
    
public:
    //
    // Constructors and destructors
    //

    movie();
    ~movie();
    
    //
    // Movie methods
    //

    void writeImage(
	void* buffer, 
	size_t bufferSize,
	DMparams* bufferParams,
	int startFrameNumber,
	int endFrameNumber
	);
    
    //
    // Methods just used in this file.
    //

    void initCreate( 
	const char* 	fileName,
	DMparams* 	imageParams,
	MVtimescale 	timescale,
	MVtime		frameDuration
	);

    Boolean getValid() { return myValid; }
    
private:
    //
    // Private methods
    //

    /****
    size_t getPixelSize();
    void   xbgrToRgb( void* );
    void   rgbToXbgr( void* );
    ***/
};

////////
//
// Constructor
//
////////

movie::movie(
    )
{
    myValid = FALSE;
}

////////
//
// Destructor
//
////////

movie::~movie(
    )
{
    if ( myValid ) {
	mvClose( myMovie );
    }
}

////////
//
// initCreate
//
////////

void movie::initCreate(
    const char* fileName,
    DMparams*   imageParams,
    MVtimescale	timescale,
    MVtime	frameDuration
    )
{
    DMstatus status;
    
    //
    // Stash settings
    //

    myTimescale		= timescale;
    myFrameDuration	= frameDuration;
    
    //
    // Create the movie file.
    //

    {
	DMparams* params;
	dmParamsCreate( &params );
	mvSetMovieDefaults( params, MV_FORMAT_QT );
	status = mvCreateFile( fileName, params, NULL, &myMovie );
	dmParamsDestroy( params );
	if ( status != DM_SUCCESS ) {
	    return;
	}
    }
    
    //
    // Create the image track.
    //

    status = mvAddTrack( myMovie, DM_IMAGE, imageParams, NULL, &myImageTrack );
    if ( status != DM_SUCCESS ) {
	printf( "Movie error: %s\n", mvGetErrorStr( mvGetErrno() ) );
	mvClose( myMovie );
	return;
    }
    status = mvSetTrackTimeScale( myImageTrack, myTimescale );
    if ( status != DM_SUCCESS ) {
	printf( "Movie error: %s\n", mvGetErrorStr( mvGetErrno() ) );
	mvClose( myMovie );
	return;
    }
    
    //
    // Get ready for storing the timecodes on the frames.
    //

    mvAddUserParam( "TIMECODE_RANGE" );
    
    //
    // All done.
    //

    myValid = GL_TRUE;
}

////////
//
// writeImage
//
////////

void movie::writeImage(
    void*	buffer,
    size_t	bufferSize,
    DMparams*	bufferParams,
    int		startFrameNumber,
    int		endFrameNumber
    )
{
    //
    // Figure out where the end of the track is.
    //

    MVtime whereToInsert = startFrameNumber * myFrameDuration;

    //
    // How long will this image be displayed?
    //

    MVtime duration = 
	( endFrameNumber - startFrameNumber + 1 ) * myFrameDuration;
    
    //
    // Write the image to the movie file, setting the duration so that
    // it will last as long as the scene did.
    //

    DMstatus status = mvInsertFramesAtTime(
	myImageTrack,
	whereToInsert,
	duration,	// how long the image is shown when the movie
			// is played
	myTimescale,	// Units for endOfTrack, duration
	buffer,
	bufferSize,
	bufferParams,
	0		// use the default track params
	);
    if ( status != DM_SUCCESS ) {
	printf( "Movie error: %s\n", mvGetErrorStr( mvGetErrno() ) );
	mvClose( myMovie );
	return;
    }
}

////////////////////////////////////////////////////////////////////////
//
// Movie interface class methods.
//
////////////////////////////////////////////////////////////////////////

////////
//
// create
//
////////

Movie* Movie::create(
    const char* fileName,
    DMparams*   imageParams,
    MVtimescale	timescale,
    MVtime	frameDuration
    )
{
    movie* newMovie = new movie();
    newMovie->initCreate( fileName, imageParams, timescale, frameDuration );
    
    if ( newMovie->getValid() ) {
	return newMovie;
    }
    else {
	delete newMovie;
	return NULL;
    }
}

////////
//
// Destructor
//
////////

Movie::~Movie(
    )
{
}
