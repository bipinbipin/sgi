/*****************************************************************************
 *
 * File:        manymovieArgs.h
 *
 * Description: Part of manymovie. Code for processing command line arguments,
 *              and accessing the movie list and number of movies. Visible
 *		functions are declared here.
 *
 ****************************************************************************/

#include <X11/Xlib.h>
#include <dmedia/moviefile.h>
#include <dmedia/movieplay.h>

/*
 * Maximum number of movies supported for simultaneous playback. Note that
 * this is not a limitation of the SGI Movie Library, but only a limitation
 * of manymovie itself due to the crude layout engine it employs.
 */

#define MAX_MOVIES 4

extern void 	 processCmdArgs( int argc, char **argv );

extern int 	 getMovieCount( void );

extern void 	 setCurrentToFirstMovie( void );

extern void 	 setCurrentToNextMovie( void );

extern void 	 setCurrentMovieID(  MVid theMovie );

extern void 	 setCurrentWinID( Window win );

extern MVid 	 getCurrentMovieID( void );

extern char 	 *getCurrentMovieName( void );

extern Window 	 getCurrentMovieWin( void );

extern int 	 getNumMoviesInWindow( Window win );

extern DMstatus  getAllMoviesInWindow( Window win, MVid *theMovies );

extern char 	 *getProgramName( void );

extern DMboolean useOneWindow( void );

extern void 	 destroyMovieList( void );
