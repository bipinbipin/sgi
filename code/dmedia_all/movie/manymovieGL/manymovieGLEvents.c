/*****************************************************************************
 *
 * File:        manymovieGLEvents.c
 *
 * Description: Part of manymovieGL. X and Movie event handling code.
 *
 * Functions:   SGI Movie Library functions used:
 *
 *              mvGetEventFD()
 *              mvSetSelectEvents()
 *              mvPendingEvents()
 *              mvNextEvent()
 *              mvGetEnableAudio()
 *              mvSetEnableAudio()
 *              mvSetPrimaryAudio()
 *              mvSetCurrentFrame()
 *              mvShowCurrentFrame()
 *              mvResizeOpenGLWindow()
 *              mvQueryViewSize()
 *              mvSetViewSize()
 *              mvGetPlayLoopMode()
 *              mvSetPlayLoopMode()
 *		mvGetErrorStr()
 *		mvGetErrno()
 *
 *****************************************************************************/

#ifndef _SVR4_SOURCE
#define _SVR4_SOURCE
#endif
#include <unistd.h>
#include "manymovieArgs.h"
#include "manymovieEvents.h"
#include "manymovieGLWin.h"
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <bstring.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <dmedia/moviefile.h>
#include <dmedia/movieplay.h>

/*
 * Forward declarations for functions local to this module.
 */

static DMstatus setupFDSet( Display *xDpy, int *movieFD, int *xFD,
                           fd_set *theFDSet );
static DMboolean handleXEvents( Display *xDpy );
static DMboolean handleMovieEvents( void );
static DMboolean handleKeyboard( XEvent *event );
static void stopTheMovies( MVid *theMovies, int numMoviesInWin );
static void playTheMovies( MVid *theMovies, int numMoviesInWin );
static void rewindToBeginning( MVid *theMovies, int numMoviesInWin );
static void toggleAudioMuting( MVid *theMovies , int numMoviesInWin );
static void stepToNextLoopState( MVid *theMovies, int numMoviesInWin );

/*********
 *
 * Initialize the event handling mechanisms, then wait for an X or Movie
 * event. Call the appropriate event handler upon event arrival.
 *
 *********/

void handleEvents( void )
{
    int	      		movieFD;
    int			xFD;
    fd_set    		theFDSet;
    Display		*xDpy = getXDisplay();

    /*
     * Set up the file descriptor set for event handling by select().
     */

    if ( setupFDSet( xDpy, &movieFD, &xFD, &theFDSet ) != DM_SUCCESS ) {
        destroyMovieList();
        exit( EXIT_FAILURE ); 
    }

    /* 
     * Decide which movie events we're interested in hearing.
     */

    mvSetSelectEvents( MV_EVENT_MASK_FRAME | MV_EVENT_MASK_STOP | 
                      MV_EVENT_MASK_ERROR );

    /* 
     * Sit in select(), waiting for a message from the movie
     * player or from the X server. Process events from both. 
     */

    {
        DMboolean	keepPlaying = DM_TRUE;
        fd_set		loopFDSet;
        int		maxFD;

        maxFD = ( ( movieFD > xFD ) ? movieFD : xFD ) + 1;

        while ( keepPlaying ) {
	    loopFDSet = theFDSet;
	    select( maxFD, &loopFDSet, NULL, NULL, NULL );
	    if ( FD_ISSET( movieFD, &loopFDSet ) ) {  	   /* movie event */
                keepPlaying = handleMovieEvents( );
	    } else if ( FD_ISSET( xFD, &loopFDSet ) ) {	   /* X event */
                keepPlaying = handleXEvents( xDpy );
	    }
        }
    }

    /*
     * User wants to quit, or an error was encountered during playback.
     * Clean things up.
     */

    destroyMovieList();
}

/*********
 *
 * Helper function to set up file descriptor set for select.
 *
 *********/

static DMstatus setupFDSet( Display *xDpy, int *movieFD,
                           int *xFD, fd_set *theFDSet )
{
    /*
     * Get the file descriptor responsible for communicating the
     * events we'll get back from the movie library
     */

    if ( mvGetEventFD( movieFD ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Could not get movie event FD.\n", 
	getProgramName() );
        return DM_FAILURE;
    }

    /*
     * Get the file descriptor for the X connection.
     */

    *xFD = ConnectionNumber( xDpy );

    /*
     * Place the event file descriptors in the fd_set for select().
     */

    FD_ZERO( theFDSet );
    FD_SET( *movieFD, theFDSet );
    FD_SET( *xFD, theFDSet );

    return DM_SUCCESS;
}

/*********
 *
 * Interpret and act on movie events.
 *
 *********/

static DMboolean handleMovieEvents( )
{
    MVevent event;
 
    while ( mvPendingEvents() != 0 ) {
	mvNextEvent( &event );
	switch ( event.type ) {
	    case MV_EVENT_FRAME: 		/* a frame played */

		/* Uncomment and recompile to see which frames are played.
		 * 
		 * 
	         * printf( "%s: Played frame %d of movie %d.\n", 
                 *        getProgramName(), event.mvframe.frame, 
                 *        event.mvframe.id );
                 */

	        break;

	    case MV_EVENT_STOP: 		/* end of movie */
	        printf( "%s: Playback of movie %d stopped.\n",
	               getProgramName(), event.mvstop.id );
	        break;

	    case MV_EVENT_ERROR:		/* error */
	        fprintf( stderr, "%s: Error during playback: %s.\n",
	                getProgramName(), mvGetErrorStr( mvGetErrno() ) );
	        return DM_FALSE;
	        break;
	}
    }
    return DM_TRUE;
}

/*********
 *
 * Interpret and act on X events.
 *
 *********/

static DMboolean handleXEvents( Display *xDpy )
{
    XEvent             event;
    XWindowAttributes  winAttrs;
    MVid	       theMovies[MAX_MOVIES];
    int		       numMoviesInWin;
    int		       i;
    int                actualWidth; 
    int		       actualHeight;

    while ( XPending( xDpy ) != 0 ) {

	XNextEvent( xDpy, &event );
	if ( getAllMoviesInWindow( event.xany.window,
                                  ( MVid *)theMovies ) != DM_SUCCESS ) {
            fprintf( stderr, "%s: Invalid window id.\n", getProgramName() );
            return DM_FALSE;
        }
	numMoviesInWin = getNumMoviesInWindow( event.xany.window );

	switch ( event.type ) {
	    case Expose:			/* repaint display */
		for( i = 0; i < numMoviesInWin; i++ ) {
	            mvShowCurrentFrame( theMovies[i] );
		}
	        break;

	    case ConfigureNotify:		/* window was resized */

		for( i = 0; i < numMoviesInWin; i++ ) {
	            /* 
                     * Inform movie lib window was resized.  As a side
	             * effect, this also resizes the GL viewport.
                     */

	             mvResizeOpenGLWindow( xDpy, event.xconfigure.window,
				          getGLXContext( i ) );

                     if ( i < 1 ) {
	                 /* 
                          * Get the new window size 
                          */

	             	 XGetWindowAttributes( xDpy, event.xconfigure.window,
                                              &winAttrs);
                     }
                    /*
                     * Get and print the actual movie size.
                     */
	             mvQueryViewSize( theMovies[i], winAttrs.width,
                                     winAttrs.height, DM_TRUE, 
	                             &actualWidth, &actualHeight );

	             printf( "%s: movie %d, actual width = %d, height = %d\n", 
	                    getProgramName(), theMovies[i],
                            actualWidth, actualHeight );
		}
	        break;

 	    case KeyRelease:		/* keyboard event */
	        return( handleKeyboard( &event ) );
	        break;
        }	
    }
    return DM_TRUE;
}

/*********
 *
 * Interpret keyboard events and take appropriate action.
 *
 *********/

static DMboolean handleKeyboard( XEvent *event )
{
    MVid       theMovies[MAX_MOVIES];
    int	       numMoviesInWin;
    int	       i;
    char       c[2];

    if ( getAllMoviesInWindow( event->xkey.window,
                            theMovies ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Invalid window id.\n", getProgramName() );
        return DM_FALSE;
    }
    numMoviesInWin = getNumMoviesInWindow( event->xkey.window );

    XLookupString( &event->xkey, c, 1, NULL, NULL );
    c[1] = NULL;

    switch ( c[0] ) {
	case 's':
	case 'S':
	    stopTheMovies( theMovies, numMoviesInWin );
	    return DM_TRUE;

	case 'p':
	case 'P':
	    playTheMovies( theMovies, numMoviesInWin );
	    return DM_TRUE;

	case 'r':
	case 'R':
	    rewindToBeginning( theMovies, numMoviesInWin );
	    return DM_TRUE;

	case 'm':
	case 'M':
	    toggleAudioMuting( theMovies, numMoviesInWin );
	    return DM_TRUE;

	case 'l':
	case 'L':
	    stepToNextLoopState( theMovies, numMoviesInWin );
	    return DM_TRUE;

	case 'q':			/* quit */
	case 'Q':
	    printf( "quit\n" );
	    return DM_FALSE;

	default:
	    printf( "unrecognized keycode %s\n", c );
    }
    return DM_TRUE;
}

/*****************************************************************************
 *
 * The functions below are helper functions for keyboard events.
 *
 *****************************************************************************/

/*********
 *
 * Stop all movies in the window from playing.
 *
 *********/

static void stopTheMovies( MVid *theMovies, int numMoviesInWin )
{
    int i;


    for( i = 0;i < numMoviesInWin; i++ ) {
	printf("%s: %d, stop\n", getProgramName(), theMovies[i]);
	mvStop( theMovies[i] );
    }
}

/*********
 *
 * Play all movies in the window.
 *
 *********/

static void playTheMovies( MVid *theMovies, int numMoviesInWin )
{
    int i;

    mvSetPrimaryAudio( theMovies[0] );

    for( i = 0;i < numMoviesInWin; i++ ) {
	printf("%s: %d, play\n", getProgramName(), theMovies[i]);
	mvPlay( theMovies[i] );
    }
}

/*********
 *
 * Toggle audio for all movies in the window.
 *
 *********/

static void rewindToBeginning( MVid *theMovies, int numMoviesInWin )
{
 int i;

    for( i = 0;i < numMoviesInWin; i++ ) {
	printf("%s: %d, rewind\n", getProgramName(), theMovies[i]);
	mvSetCurrentFrame( theMovies[i], 0 );
    }
}

/*********
 *
 * Toggle audio for all movies in the window.
 *
 *********/

static void toggleAudioMuting( MVid *theMovies , int numMoviesInWin )
{
    int i;

    for( i = 0;i < numMoviesInWin; i++ ) {

	printf("%s: %d, toggle mute to ", getProgramName(), theMovies[i]);
	
	if ( mvGetEnableAudio( theMovies[i] ) ) {
	    printf( "OFF\n" );
	    mvSetEnableAudio( theMovies[i], DM_FALSE );
	} else {
	    printf( "ON\n" );
	    mvSetEnableAudio( theMovies[i], DM_TRUE );
	}
    }
}

/*********
 *
 * Advance to the next loop state.
 *
 *********/

static void stepToNextLoopState( MVid *theMovies, int numMoviesInWin )
{
    int i;
    MVloopmode loopMode;

    for( i = 0;i < numMoviesInWin; i++ ) {

	printf("%s: %d, change loop state to ", getProgramName(), theMovies[i]);

	switch ( mvGetPlayLoopMode( theMovies[i] ) ) {
	case MV_LOOP_NONE:
	    loopMode = MV_LOOP_CONTINUOUSLY;
	    printf( "CONTINUOUS\n" );
	    break;
	case MV_LOOP_CONTINUOUSLY:
	    loopMode = MV_LOOP_SWINGING;
	    printf( "SWINGING\n" );
	    break;
	case MV_LOOP_SWINGING:
	    loopMode = MV_LOOP_NONE;
	    printf( "NONE\n" );
	    break;
	}
	mvSetPlayLoopMode( theMovies[i], loopMode );
    }
}
