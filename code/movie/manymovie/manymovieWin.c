/******************************************************************************
 *
 * File:        manymovieWin.c
 *
 * Description: Part of manymovie. Code for creating X windows suitable for
 *              GL rendering, placing movies in them, and displaying them.
 *
 * Functions:   SGI Movie Library functions used:
 *
 *              mvOpenFile()
 *              mvFindTrackByMedium()
 *              mvGetImageWidth()
 *              mvGetImageHeight()
 *              mvBindWindow()
 *
 ******************************************************************************/

#include "manymovieWin.h"
#include "manymovieArgs.h"
#include "glxhelper.h"
#include <X11/Xutil.h>
#include <stdio.h>
#include <dmedia/moviefile.h>
#include <dmedia/movieplay.h>

/*
 * Layout constants for the one window, many movies case.
 */

#define WIN_MARGIN 5              /* Inside margin around the window. */ 
#define MOVIE_MARGIN 20           /* Minimum margin between movies. */ 

/*
 * The X Display. Local to this module.
 */

static Display 	*dpy;

/*
 * Forward declarations for functions local to this module.
 */

static void initManyWindows( void );
static void initOneWindow( void );
static DMstatus findOneWindowSize( int *xWinWidth, int *xWinHeight );
static DMstatus getMovieWidthAndHeight( MVid theMovie,
                                       int *width, int *height );
static void getWinWidthAndHeight( int *width, int *height );
static DMstatus bindWinToMovies( Window win );
static DMstatus setMovieViewOffsets( void );
static DMstatus initMovie( char *name );
static void mapWindows( void );
static DMstatus createXWindow( int width, int height, Window *winReturn );

/*********
 *
 * Open the movies and create and open windows for them.
 *
 *********/

void openAllMovies( void )
{
    if ( useOneWindow() ) {
        initOneWindow();
    } else {
        initManyWindows();
    }

    /*
     * Open the movie windows.
     */

    mapWindows();
}

/*********
 *
 * Open the movies, and create separate windows for each in which to play them.
 *
 *********/

static void initManyWindows( void )
{
    int 	i;
    char 	*name;
    int 	numMovies = getMovieCount();

    /*
     * Make sure we're pointing to the first movie.
     */
 
    setCurrentToFirstMovie();

    for( i = 0; i < numMovies; i++ ) { 

        name = getCurrentMovieName();

        if ( initMovie( name ) != DM_SUCCESS ) {
            exit( EXIT_FAILURE );
        }

        /*
         * Increment the list pointer for getCurrentMovieName(),
         * setCurrentMovieID(), and setCurrentWinID().
         */
 
        setCurrentToNextMovie();
    }
}

/*********
 *
 * Create a window in which to play the movies.
 *
 *********/

static void initOneWindow( void )
{
    Window theWin;
    MVid   theMovie;
    char   *name;
    int    winWidth;
    int    winHeight;
    int    i;
    int    numMovies = getMovieCount();
    
    /*
     * Make sure we're pointing to the first movie.
     */
 
    setCurrentToFirstMovie();

    for ( i = 0; i < numMovies; i++ ) {
    
        /*
         * Create a new movie instance.
         */

        name = getCurrentMovieName();

        if ( mvOpenFile( name, O_RDONLY, &theMovie ) == DM_FAILURE ) {
    	    fprintf( stderr, "%s: Could not open movie.\n", getProgramName() );
	    exit( EXIT_FAILURE );
        }

        setCurrentMovieID( theMovie );

        /*
         * Increment the list pointer for getCurrentMovieName() and
         * setCurrentMovieID().
         */
 
        setCurrentToNextMovie();
    }

    if ( findOneWindowSize( &winWidth, &winHeight ) != DM_SUCCESS ) {
        exit( EXIT_FAILURE );
    }

    if ( createXWindow( winWidth, winHeight, &theWin ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Could not create window.\n", getProgramName() );
        exit( EXIT_FAILURE );
    }

    if ( bindWinToMovies( theWin ) != DM_SUCCESS ) {
        exit( EXIT_FAILURE );
    }
}

/*********
 *
 * Figure out how big the window has to be to hold all the movies.
 * Note that this minimal layout calculation scheme really assumes 
 * a maximum of 4 movies. Extensions are left as an exercise for
 * the reader.
 *
 *********/

static DMstatus findOneWindowSize( int *xWinWidth, int *xWinHeight )
{ 
    MVid   theMovie;
    char   *name;
    int	   winWidth;
    int    winHeight;
    int    lastStartRowWidth  = 0;
    int    lastStartRowHeight = 0;
    int    lastEndRowWidth    = 0;
    int    totalWinWidth      = 0;             
    int    totalWinHeight     = 0; 
    int    numMovies          = getMovieCount();
    int	   i;
    
    /*
     * Make sure we're pointing to the first movie.
     */
 
    setCurrentToFirstMovie();

    for ( i = 0; i < numMovies; i++ ) {

        if ( getMovieWidthAndHeight( theMovie, &winWidth, &winHeight )
            != DM_SUCCESS ) {
            return DM_FAILURE;
        }
        if ( i == 0 ) {                       /* First movie. */

            totalWinWidth+=winWidth; 
            totalWinHeight+=winHeight; 
            lastStartRowWidth = winWidth;
            lastStartRowHeight = winHeight;
        } 
	else {
            if ( ( i % 2 ) == 0 ) {            /* New row of movies. */ 

                totalWinHeight += winHeight + MOVIE_MARGIN; 

                if ( winWidth > lastStartRowWidth ) {
                    totalWinWidth += winWidth - lastStartRowWidth;
                }
                lastStartRowWidth = winWidth;
                lastStartRowHeight = winHeight;
            } 
	    else {                           /* End of row of movies. */
                if ( i == 1 ) {

                   totalWinWidth += MOVIE_MARGIN; 
		}
                if ( winWidth > lastEndRowWidth ) {
                    totalWinWidth += winWidth - lastEndRowWidth;
                }
                if ( winHeight  > lastStartRowHeight ) {
                    totalWinHeight += winHeight - lastStartRowHeight; 
                }
                lastEndRowWidth = winWidth;
            }
        }
        setCurrentToNextMovie();
    }

    totalWinWidth += ( 2 * WIN_MARGIN );             
    totalWinHeight += ( 2 * WIN_MARGIN ); 

    /*
     * Make sure the current movie is still the first movie.
     */
 
    setCurrentToFirstMovie();

    *xWinWidth = totalWinWidth;
    *xWinHeight = totalWinHeight;
    return DM_SUCCESS;
}

/*********
 *
 * Helper function to get the width and height of a movie.
 *
 ********/

static DMstatus getMovieWidthAndHeight( MVid theMovie, int *width, int *height )
{
    MVid imageTrack;

    theMovie = getCurrentMovieID();
    if ( mvFindTrackByMedium( theMovie, DM_IMAGE,
                             &imageTrack ) == DM_FAILURE ) {
        fprintf( stderr, "%s: Could not find image track.\n",
                getProgramName() );
        return DM_FAILURE; 
    }
    *width  = mvGetImageWidth( imageTrack );
    *height = mvGetImageHeight( imageTrack );
    return DM_SUCCESS;
}

/*********
 *
 * Associate the X window with the movies, and bind it.
 *
 ********/

static DMstatus bindWinToMovies( Window win )
{
    int  i;
    MVid theMovie;
    int  numMovies = getMovieCount();

    for( i = 0; i < numMovies; i++ ) {

        theMovie = getCurrentMovieID();

        /*
         * Set the X window id for the movies.
         */

        setCurrentWinID( win );

        /* 
         * Bind the GL/X window we just created to the movie.
         */
        if ( mvBindWindow( theMovie, dpy, win ) != DM_SUCCESS ) {
 	    fprintf( stderr, "%s: Could not bind movie to window.\n",
                    getProgramName() );
	    return DM_FAILURE;
        }

        mvSetViewBackground( theMovie, 0, 0, 0 );

        setCurrentToNextMovie();
    }

    if ( setMovieViewOffsets() != DM_SUCCESS ) {
        return DM_FAILURE;
    }

    /*
     * Reset the current movie to the first movie.
     */
 
    setCurrentToFirstMovie();

    return DM_SUCCESS;
}

/*********
 *
 * Arrange the movies in the window. Crude and rude, and works only for
 * a maximum of four movies. Extensions are left as an exercise for the
 * reader.
 *
 *********/

static DMstatus setMovieViewOffsets( void )
{
    int i;
    int movieWidth;
    int movieHeight;
    int winWidth;
    int winHeight;
    int xOffset;
    int yOffset;
    MVid theMovie;
    int numMovies = getMovieCount();

    for( i = 0; i < numMovies; i++ ) {

        theMovie = getCurrentMovieID();
        if ( getMovieWidthAndHeight( theMovie, &movieWidth, &movieHeight )
            != DM_SUCCESS ) {
            return DM_FAILURE;
        }
        getWinWidthAndHeight( &winWidth, &winHeight );

        mvSetViewSize( theMovie, movieWidth, movieHeight, DM_TRUE );

        switch( numMovies ) {
	    case 1:
		xOffset = WIN_MARGIN;
		yOffset = WIN_MARGIN;
		break;	
	    case 2:
                switch( i ) {
		    case 0:
		        xOffset = WIN_MARGIN;
		        yOffset = WIN_MARGIN;
		        break;	
		    case 1:
		        xOffset = winWidth - ( movieWidth + WIN_MARGIN );
		        yOffset = WIN_MARGIN;
		        break;	
                }
		break;	
	    case 3:
                switch( i ) {
		    case 0:
		        xOffset = WIN_MARGIN;
		        yOffset = winHeight - ( movieHeight + WIN_MARGIN );
		        break;	
		    case 1:
		        xOffset = winWidth - ( movieWidth + WIN_MARGIN );
		        yOffset = winHeight - ( movieHeight + WIN_MARGIN );
		        break;	
		    case 2:
		        xOffset = WIN_MARGIN;
		        yOffset = WIN_MARGIN;
		        break;	
                }
		break;	
	    case 4:
                switch( i ) {
		    case 0:
		        xOffset = WIN_MARGIN;
		        yOffset = winHeight - ( movieHeight + WIN_MARGIN );
		        break;	
		    case 1:
		        xOffset = winWidth - ( movieWidth + WIN_MARGIN );
		        yOffset = winHeight - ( movieHeight + WIN_MARGIN );
		        break;	
		    case 2:
		        xOffset = WIN_MARGIN;
		        yOffset = WIN_MARGIN;
		        break;	
		    case 3:
		        xOffset = winWidth - ( movieWidth + WIN_MARGIN );
		        yOffset = WIN_MARGIN;
		        break;	
                }
		break;	
        }
        mvSetViewOffset( theMovie, xOffset, yOffset, DM_TRUE );

        setCurrentToNextMovie();
    }

    return DM_SUCCESS;
}

/*********
 *
 * Return the width and height of the X window for the current movie.
 *
 ********/

static void getWinWidthAndHeight( int *width, int *height )
{
    XWindowAttributes winAttribs;
    XGetWindowAttributes( getXDisplay(), getCurrentMovieWin(), &winAttribs );
    *width = winAttribs.width;
    *height = winAttribs.height;
}

/*********
 *
 * Open a movie, and create a window in which to play it.
 *
 *********/

static DMstatus initMovie( char *name )
{
    int	   winWidth;
    int    winHeight;
    Window tmpWin;
    MVid   theMovie;

    /*
     * Create a new movie instance.
     */

    if ( mvOpenFile( name, O_RDONLY, &theMovie ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Could not open movie.\n", getProgramName() );
	return DM_FAILURE;
    }

    /*
     * Set the MVid for the current movie.
     */

    setCurrentMovieID( theMovie );

    /* 
     * Create a GL/X window to display the movie.
     */

    if ( getMovieWidthAndHeight( theMovie, &winWidth, &winHeight )
        != DM_SUCCESS ) {
        return DM_FAILURE;
    }

    if ( createXWindow( winWidth, winHeight, &tmpWin ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Could not create window.\n", getProgramName() );
	return DM_FAILURE;
    }

    /*
     * Set the X window id for the current movie.
     */

    setCurrentWinID( tmpWin );

    /* 
     * Bind the GL/X window we just created to the movie.
     */

    if ( mvBindWindow( theMovie, dpy, tmpWin ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Could not bind movie to window.\n",
		 getProgramName() );
	return DM_FAILURE;
    }
    return DM_SUCCESS;
}

/*********
 *
 * Map the movie windows.
 *
 *********/

static void mapWindows( void )
{
    int numMovies = getMovieCount();
    setCurrentToFirstMovie();

    if ( !( useOneWindow() ) ) {
        while ( numMovies > 0 ) {
 	    XMapWindow( dpy, getCurrentMovieWin() );
            setCurrentToNextMovie();
            numMovies--;
        }
    } else {
        XMapWindow( dpy, getCurrentMovieWin() );
    }
    XFlush( dpy );
}

/********* 
 *
 * Open a connection to the X server, and create an X window
 * suitable for GL rendering here.  All of this
 * work is normally handled by the toolkit you are using
 * to build your application (e.g. IRIS IM or some similar
 * X-based user interface toolkit). 
 *
 *********/

static DMstatus createXWindow( int width, int height, Window *winReturn )
{
    unsigned long        black;
    XColor               c;
    XSetWindowAttributes childWinAttribs;

    /* 
     * Open a connection to the X server.
     */

    if ( dpy == NULL ) {
	dpy = XOpenDisplay( 0 );
	if ( dpy == NULL ) {
	    fprintf( stderr, "%s: Cannot open X display.\n", getProgramName() );
	    return DM_FAILURE;
	}
    }

    /*
     * Get a pixel value for true black for the background pixel color.
     */

    c.red = c.green = c.blue = 0;
    if ( !XAllocColor( dpy, DefaultColormap( dpy,
                                            DefaultScreen( dpy ) ), &c ) ) {
        black = BlackPixel( dpy, DefaultScreen( dpy ) );
    } else {
        black = c.pixel;
    }
    
    childWinAttribs.colormap = DefaultColormap( dpy, DefaultScreen( dpy ) );

    childWinAttribs.background_pixel = black;

    childWinAttribs.border_pixel = black;

    /* 
     * Create an X window configured for GL rendering, using
     * the helper functions defined in glxhelper.c 
     */

    *winReturn = GLXCreateWindow( dpy, 
                                  RootWindow( dpy, DefaultScreen( dpy ) ),
                                  0, 0, width, height, 0,
                                  CWBackPixel|CWColormap|CWBorderPixel,
				  &childWinAttribs, GLXrgbSingleBuffer );

    XSelectInput( dpy, *winReturn,
                 ExposureMask | StructureNotifyMask | KeyReleaseMask );
    return DM_SUCCESS;
}

/*********
 *
 * Retrieve the X Display.
 *
 *********/

Display *getXDisplay( void )
{
    return dpy;
}
