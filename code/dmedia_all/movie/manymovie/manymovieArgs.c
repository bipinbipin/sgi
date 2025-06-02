/*****************************************************************************
 *
 * File:         manymovieArgs.c
 *
 * Description: Part of manymovie. Code for processing command line
 *              arguments, creating and accessing the movie list,
 *              and recording the number of movies.
 *
 * Functions:   SGI Movie Library functions used:
 *
 *              mvIsMovieFile()
 *		mvSetNumMoviesHint()
 * 		mvGetErrorStr()
 *		mvGetErrno()
 *
 *****************************************************************************/

#include "manymovieArgs.h"
#include "manymovieWin.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>


/*
 * Small structure to hold a linked list of movie instances.
 */

typedef struct _nameList {
    char*       name;
    Window win;                         /* X window */
    MVid movie;
    struct _nameList *  next;
} nameList;

/*
 * Variables restricted to this module.
 */

static char	*programName;		   /* Name of the program. */
static nameList *movieList = NULL;         /* Linked list of movie names. */
static nameList *currentMovie = NULL;      /* The movie in the list affected
                                              by the getCurrentMovieXXX and
                                              setCurrentMovieXXX functions. */
static int	movieCount;                /* The number of movies. */
DMboolean	oneWindow = DM_FALSE;      /* If DM_TRUE, place all movies in
                                              one window, otherwise, play all
                                              movies in separate windows. */ 

/*
 * Forward declarations for local functions.
 */

static void usage( void );
static nameList *getMovieList( void );
static void resetCurrentIfNeeded( void );

/*********
 *
 * processCmdArgs - Process the command line arguments.
 *
 *********/

void processCmdArgs( int argc, char **argv ) 
{
    int		i;
    nameList 	*end = NULL;
    nameList 	*current = NULL;

    programName = argv[0];

    if ( mvSetNumMoviesHint( MAX_MOVIES ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Movie library error: %s\n",
		 programName, mvGetErrorStr( mvGetErrno() ) );
        exit( EXIT_FAILURE );
    }

    if ( argc < 2 ) {
        usage();
    }

    /* 
     * Create the list of movie names. Verify that each is a movie
     * file before continuing.
     */

    movieCount = 0;

    for (i = 1; i < argc; i++) {
        if ( strcmp( argv[i], "-one" ) == 0 ) {
            oneWindow = DM_TRUE;
        }
        else {
            char *movieName = argv[i];
            if ( !( mvIsMovieFile( movieName ) ) ) {
                fprintf( stderr, "%s: %s is not a movie file.\n",
                        programName, movieName );
	        exit( EXIT_FAILURE );
            }
	    current = (nameList *)malloc( sizeof( nameList ) );
	    if ( current == NULL ) {
	        fprintf( stderr, "%s: Movie nameList malloc failed.\n", 
                        programName );
                exit( EXIT_FAILURE );
            }
            current->name = movieName;
            current->next = NULL;
    
	    if ( end != NULL ) {
	        end->next = current;
	    } else {
	        movieList = current;
	    }
	    end = current;
            movieCount++;
        }
    }

    /*
     * If the number of movies entered is greater than MAX_MOVIES, ignore
     * all of them except the first MAX_MOVIES movies. Inform the user
     * of the limitation.
     */

    if ( movieCount > MAX_MOVIES ) {
        fprintf( stderr, "%s: Warning: This program can ", programName );
        fprintf( stderr, "display only %d movies at a time.\n", MAX_MOVIES );
        movieCount = MAX_MOVIES;
    }
}

/*********
 *
 * Print usage message.
 *
 *********/

static void usage( void )
{
    fprintf( stderr, "usage: %s [ -one ] <moviefile1> [moviefile2 ...]\n",
            programName );
    fprintf( stderr, "       -one = place all movies in one window.\n");
    exit( EXIT_FAILURE );
}

/*********
 *
 * Retrieve the movie list.
 *
 *********/

static nameList *getMovieList( void )
{
    return movieList;
}

/*********
 *
 * Retrieve the movie count.
 *
 *********/

int getMovieCount( void )
{
    return movieCount;
}

/*********
 *
 * Set the list pointer to the first movie on the list.
 *
 *********/

void setCurrentToFirstMovie( void )
{
    currentMovie = getMovieList();
}

/*********
 *
 * Step to the next nameList structure on the list.
 *
 *********/

void setCurrentToNextMovie( void )
{
    if ( movieList == NULL ) {
        return;
    }
    resetCurrentIfNeeded();
    currentMovie = currentMovie->next;
}

/*********
 *
 * Set the MVid for the movie pointed to by setName.
 *
 *********/

void setCurrentMovieID( MVid theMovie )
{
    if ( movieList == NULL ) {
        return;
    }
    resetCurrentIfNeeded();
    currentMovie->movie = theMovie;
}

/********
 *
 * Get the MVid for the movie pointed to by setName.
 *
 *********/

MVid getCurrentMovieID( void )
{
    resetCurrentIfNeeded();
    return currentMovie->movie;
}

/*********
 *
 * Set the X Window id for the movie pointed to by setName.
 *
 *********/

void setCurrentWinID( Window win )
{
    if ( movieList == NULL )
        return;
    resetCurrentIfNeeded();
    currentMovie->win = win;
}

/*********
 *
 * Return the name of the current movie, if it exists, NULL otherwise.
 *
 *********/

char *getCurrentMovieName( void )
{
    if ( movieList == NULL )
        return NULL;
    resetCurrentIfNeeded();
    return currentMovie->name;
}

/*********
 *
 * Return the Window corresponding to the current movie.
 *
 *********/

Window getCurrentMovieWin( void )
{
    if ( movieList == NULL )
        return NULL;
    resetCurrentIfNeeded();
    return currentMovie->win;
}

/*********
 *
 * Set currentMovie to movieList if currentMovie is null.
 *
 *********/

static void resetCurrentIfNeeded( void )
{
    if ( currentMovie == NULL )
        currentMovie = movieList;
}

/*********
 *
 * Get the number of movies in a given X window.
 *
 *********/

int getNumMoviesInWindow( Window win )
{
    int numMovies = 0;
    nameList *current = getMovieList();

    while ( current != NULL ) {
	if ( current->win == win ) {
            numMovies++;
	}
	current = current->next;
    }
    return numMovies;
}

/*********
 *
 * Return a list of the MVid's of the movies in a given X window. "theMovies"
 * must be an array of MVid's equal in length to the number of movies in
 * the X window; i.e. the value returned by getNumMoviesInWindow().
 *
 *********/

DMstatus getAllMoviesInWindow( Window win, MVid *theMovies )
{
    MVid *theMovieIDs = theMovies;
    nameList *current = getMovieList();
    DMboolean winFound = DM_FALSE;

    while ( current != NULL ) {
	if ( current->win == win ) {
	    winFound = DM_TRUE;
            *theMovieIDs = current->movie;
            theMovieIDs++;
	}
	current = current->next;
    }
    if ( winFound ) {
        return DM_SUCCESS;
    }
    return DM_FAILURE;
}

/*********
 *
 * Return the name of the program. 
 *
 *********/

char *getProgramName( void )
{
    return programName;
}

/*********
 *
 * Return value of oneWindow.
 *
 *********/

DMboolean useOneWindow( void )
{
    return oneWindow;
}

/*********
 *
 * Zap the movie list, stoping and closing the movies, and freeing the
 * nameList structure.
 *
 *********/

void destroyMovieList( void )
{
    nameList *last;
    nameList *current = movieList;
    Display  *dpy = getXDisplay();
    Window   theOneWin;

    if( ( useOneWindow() ) ) {
       theOneWin = current->win;
    }
	
    while ( current != NULL ) {
        mvStop( current->movie );
        mvClose( current->movie );
        if ( !( useOneWindow() ) && ( dpy != NULL ) )
            XDestroyWindow( dpy, current->win );
        last = current;
        current = current->next;
        free( last );
    }
    if ( ( useOneWindow() ) ) {
        XDestroyWindow( dpy, theOneWin );
    }
}

