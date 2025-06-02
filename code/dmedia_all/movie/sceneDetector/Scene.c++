////////////////////////////////////////////////////////////////////////
//
// Scene.c++
//	
//	A program to find scene transitions
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

#include "Format.h"
#include "Movie.h"
#include "ScratchWindow.h"
#include "VideoIn.h"

#include <alloca.h>
#include <bstring.h>		// bcopy()
#include <dmedia/dm_image.h>	// dmSetImageDefaults()
#include <signal.h>		// signal(), SIGINT
#include <stdio.h>		// strcmp()
#include <stdlib.h>		// exit()
#include <string.h>


static const int THRESHOLD	  = 2000000;	// 2.0 Million

////////////////////////////////////////////////////////////////////////
//
// Frame class
//
// Holds one image plus a frame number.  The frame number is an offset
// into the current scene.
//
////////////////////////////////////////////////////////////////////////

struct Frame
{
public:
    Frame();
    ~Frame();
    
    ABGRPixel*	buffer;
    int		index;

};

////////
//
// Constructor
//
////////

Frame::Frame(
    )
{
    buffer = new ABGRPixel[PIXELS_PER_FRAME];
    index  = 0;
}

////////
//
// Destructor
//
////////

Frame::~Frame(
    )
{
    delete [] buffer;
}

////////////////////////////////////////////////////////////////////////
//
// Main part of the program
//
////////////////////////////////////////////////////////////////////////

////////
//
// done
//
// This global flag is set to TRUE when it is time to stop capturing
// scenes. 
//
////////

static Boolean done = FALSE;

////////
//
// interrupt
//
////////

void interrupt( int ) 
{
    printf( "Interrupt.  Writing move file...\n" );

    //
    // Set the flag to stop the main loop.
    //

    done = TRUE;

    //
    // Ignore subsequent interrupts.
    //

    signal( SIGINT, SIG_IGN );
}

////////
//
// inlineAbs
//
////////

static inline int inlineAbs(
    int n
    )
{
    if ( n < 0 ) {
	return -n;
    }
    else {
	return n;
    }
}

////////
//
// deltaAndCopy
//
////////

int deltaAndCopy(
    ABGRPixel*	src,
    ABGRPixel*	dst,
    ABGRPixel*      ref
    )
{
    int delta = 0;
    
    Component* srcp = (Component*) src;
    Component* dstp = (Component*) dst;
    Component* refp = (Component*) ref;

    Component s;
    Component r;

    for ( int i = 0;  i < PIXELS_PER_FRAME;  i++ ) {
	// alpha
	s = *srcp;
	*dstp = s;
	dstp++;
	srcp++;
	refp++;
	
	// blue
	s = *srcp;
	r = *refp;
	*dstp = s;
	dstp++;
	srcp++;
	refp++;
	delta += inlineAbs( s - r );

	// green
	s = *srcp;
	r = *refp;
	*dstp = s;
	dstp++;
	srcp++;
	refp++;
	delta += inlineAbs( s - r );

	// read
	s = *srcp;
	r = *refp;
	*dstp = s;
	dstp++;
	srcp++;
	refp++;
	delta += inlineAbs( s - r );
    }
    
    return delta;
}

////////
//
// copyFrame
//
////////

void copyFrame(
    ABGRPixel* src,
    ABGRPixel* dst
    )
{
    bcopy( src, dst, PIXELS_PER_FRAME * sizeof(ABGRPixel) );
}

////////
//
// invertFrame
//
////////

void invertFrame(
    ABGRPixel* frame
    )
{
    for ( int y1 = 0;  y1 < HEIGHT / 2;  y1++ ) {
	int y2 = HEIGHT - y1 - 1;
	ABGRPixel* p1 = frame + y1 * WIDTH;
	ABGRPixel* p2 = frame + y2 * WIDTH;
	for ( int x = 0;  x < WIDTH;  x++ ) {
	    ABGRPixel tmp;
	    tmp = *p1;
	    *p1 = *p2;
	    *p2 = tmp;
	    p1++;
	    p2++;
	}
    }
}

////////
//
// abgrToRgba
//
////////

void abgrToRgba(
    ABGRPixel* src,
    ABGRPixel* dst
    )
{
    Component* srcp = (Component*) src;
    Component* dstp = (Component*) dst;
    
    for ( int i = 0;  i < PIXELS_PER_FRAME;  i++ ) {
	Component alpha  = *(srcp++);
	Component blue   = *(srcp++);
	Component green  = *(srcp++);
	Component red    = *(srcp++);
	*(dstp++) = red;
	*(dstp++) = green;
	*(dstp++) = blue;
	*(dstp++) = alpha;
    }
}


////////
//
// showFrame
//
////////

void showFrame(
    ABGRPixel*      frame,
    ScratchWindow* win
    )
{
    //
    // Convert ABGR to RGBA.  (That's what GL likes).
    //

    size_t rgbaSize = sizeof(ABGRPixel) * PIXELS_PER_FRAME;
    ABGRPixel* rgbaFrame = (ABGRPixel*) alloca( rgbaSize );
    abgrToRgba( frame, rgbaFrame );
    
    //
    // Put the image in the window.
    //

    win->writeRgbaImage( rgbaFrame, rgbaSize );
}

////////
//
// grabScene
//
////////

void grabScene(
    VideoIn*	vin,
    ABGRPixel*	returnMiddleFrame,
    int*	returnStartFrameNumber,
    int*	returnEndFrameNumber
    )
{
    //
    // Allocate the buffers to hold the three frames that we hang on
    // to.
    //

    static Frame* m1 = NULL;
    static Frame* m2 = NULL;
    static Frame* c  = NULL;
    if ( m1 == NULL ) {
	m1 = new Frame();
	m2 = new Frame();
	c  = new Frame();
    }
    
    //
    // Start the scene with three "similar" frames
    //

    int delta;
    int startFrameNumber;
    
  startOver:
    if ( done )
    {
	*returnStartFrameNumber = -1;
	*returnEndFrameNumber   = -1;
	return;
    }
    
    vin->advance();
    startFrameNumber = vin->getMSC();
    delta = deltaAndCopy( vin->getFrame(), m1->buffer, m1->buffer );

    vin->advance();
    delta = deltaAndCopy( vin->getFrame(), m2->buffer, m1->buffer );
    if ( THRESHOLD < delta ) {
	goto startOver;
    }

    vin->advance();
    delta = deltaAndCopy( vin->getFrame(), c->buffer, m2->buffer );
    if ( THRESHOLD < delta ) {
	goto startOver;
    }

    m1->index = 1;
    m2->index = 2;
    c ->index = 3;
    
    //
    // Go until a different frame is found.
    //
    
    do {
	if ( c->index == m2->index * 2 ) {
	    Frame* tmp = m1;
	    m1 = m2;
	    m2 = tmp;
	    
	    copyFrame( c->buffer, m2->buffer );
	    m2->index = c->index;
	}
	
	vin->advance();
	delta = deltaAndCopy( vin->getFrame(), c->buffer, c->buffer );
	c->index += 1;
    } while ( ( delta <= THRESHOLD ) && ( c->index < 300 ) && ! done );
    
    //
    // Return the frame at the middle of the scene.
    //

    if ( c->index < 8 ) {
	goto startOver;
    }
    
    int middleIndex = c->index / 2;
    if ( abs( middleIndex - m1->index ) < abs( middleIndex - m2->index ) ) {
	copyFrame( m1->buffer, returnMiddleFrame );
    }
    else {
	copyFrame( m2->buffer, returnMiddleFrame );
    }
    invertFrame( returnMiddleFrame );
    *returnStartFrameNumber = startFrameNumber;
    *returnEndFrameNumber   = startFrameNumber + ( c->index - 1 );
}

////////
//
// speedTest
//
////////

void speedTest(
    )
{
    Frame* f1 = new Frame();
    Frame* f2 = new Frame();
    Frame* f3 = new Frame();
    
    for ( int i = 0;  i < 300;  i++ ) {
	deltaAndCopy( f1->buffer, f2->buffer, f3->buffer );
    }
}

////////
//
// videoTest
//
////////

void videoTest(
    )
{
    ScratchWindow* win = ScratchWindow::create( WIDTH, HEIGHT );
    if ( win == NULL ) {
	printf( "Could not create window\n" );
	exit( 1 );
    }
    win->perspective( 30.0, 5.0, -5.0 );

    VideoIn* vin = VideoIn::create( WIDTH, HEIGHT );
    if ( vin == NULL ) {
	printf( "Could not create video input.\n" );
	exit( 1 );
    }

    Frame* f = new Frame();

    while ( TRUE ) {
	vin->advance();
	abgrToRgba( vin->getFrame(), f->buffer );
	win->writeRgbaImage( f->buffer, PIXELS_PER_FRAME * sizeof(ABGRPixel) );
    }
}

////////
//
// generateTimecode
//
////////

void generateTimecode(
    int frame1,
    int frame2,
    char* buffer
    )
{
    int f1 = frame1 % 30;   frame1 = frame1 / 30;
    int s1 = frame1 % 60;   frame1 = frame1 / 60;
    int m1 = frame1 % 60;   frame1 = frame1 / 60;
    int h1 = frame1;
    
    int f2 = frame2 % 30;   frame2 = frame2 / 30;
    int s2 = frame2 % 60;   frame2 = frame2 / 60;
    int m2 = frame2 % 60;   frame2 = frame2 / 60;
    int h2 = frame2;
    
    sprintf( buffer,
	   "%02d:%02d:%02d:%02d / %02d:%02d:%02d:%02d",
	   h1, m1, s1, f1,
	   h2, m2, s2, f2
	   );
}

////////
//
// main
//
////////

int main(
    int argc,
    char** argv
    )
{
    //
    // Debugging cases.
    //

    if ( 1 < argc ) {
	if ( strcmp( argv[1], "-speed" ) == 0 ) {
	    speedTest();
	    exit( 0 );
	}
	if ( strcmp( argv[1], "-video" ) == 0 ) {
	    videoTest();
	    exit( 0 );
	}
    }
    
    //
    // Print useful information.
    //
    
    printf( "This program reads video from the default input on the\n" );
    printf( "default video device and tries to find the transitions\n" );
    printf( "between scenes.\n" );
    printf( "\n" );
    printf( "One representative frame from each scene is put into\n" );
    printf( "the movie file 'scenes.mv'.  The window on the screen\n" );
    printf( "shows the frames that are captured after they have been\n" );
    printf( "written to the movie\n" );
    printf( "\n" );
    printf( "Type ^C when you are done to finish writing the movie\n" );
    printf( "and exit the program.\n" );
    printf( "\n" );

    //
    // Trap ^C so that we can close the movie file cleanly.
    //

    signal( SIGINT, &interrupt );

    //
    // Make a buffer to keep the poster frame for each scene.
    //

    Frame* mid = new Frame();

    //
    // Make a window to display the last frame picked.
    //

    ScratchWindow* win = ScratchWindow::create( WIDTH, HEIGHT );
    if ( win == NULL ) {
	printf( "Could not create window\n" );
	exit( 1 );
    }
    win->perspective( 30.0, 5.0, -5.0 );

    //
    // Create a movie to store the frames in.
    //

    Movie* movie;
    DMparams* imageParams;
    dmParamsCreate( &imageParams );
    dmSetImageDefaults( imageParams, WIDTH, HEIGHT, DM_IMAGE_PACKING_XBGR );
    dmParamsSetFloat( imageParams, DM_IMAGE_RATE, 29.97 );
    movie = Movie::create( "scenes.mv", imageParams, 2997, 100 );
    if ( movie == NULL ) {
	printf( "Could not create movie file\n" );
	exit( 1 );
    }
	
    //
    // Open the video input (this will start the transfer).
    //

    VideoIn* vin = VideoIn::create( WIDTH, HEIGHT );
    if ( vin == NULL ) {
	printf( "Could not create video input.\n" );
	exit( 1 );
    }

    //
    // Capture scenes until interrupted
    //

    printf( "Capturing...\n" );

    while ( ! done ) {
	int startFrameNumber;
	int endFrameNumber;
	grabScene( vin, mid->buffer, &startFrameNumber, &endFrameNumber );
	
	if ( startFrameNumber != -1 )
	{
	    showFrame( mid->buffer, win );
	    movie->writeImage(
		mid->buffer,
		PIXELS_PER_FRAME * sizeof(ABGRPixel),
		imageParams,
		startFrameNumber,
		endFrameNumber
		);
	}
    }
    
    //
    // Clean up.
    //

    delete vin;
    delete movie;
    dmParamsDestroy( imageParams );
    delete win;

    //
    // All done.
    //

    printf( "Done.\n\n" );

    return 0;
}
    

