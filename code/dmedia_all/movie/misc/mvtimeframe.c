/*****************************************************************************
 *
 * File:         mvtimeframe.c
 *
 * Usage:        mvtimeframe < {-f framenum }, { -t time} > <moviefile>
 *
 * Description:  Returns either the SMPTE start time of a frame or the time
 *  		 corresponding to the given frame of a movie file. Minimally
 *	 	 demonstrates usage of the time code utilities in the
 *		 accompanying file mvPlayTimeUtils.c.
 *
 * Functions:    SGI Movie Library functions used:
 *
 *		 mvIsMovieFile()
 *               mvOpenFile()
 *               mvClose()
 * 
 *****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <dmedia/moviefile.h>
#include "mvPlayTimeUtils.h"

/*
 *
 * Global programe name variable.
 *
 */

char	 *programName;   /* Name of the program */

/*
 *
 * Forward declarations for functions used in main() and defined later.
 *
 */

static void usage( void );

/**********************************************************************
*
* main
*
**********************************************************************/

main ( int argc, char** argv ) 
{
    MVid         theMovie;       /* The movie instance */
    MVframe	 frameNum;	 /* The frame of interest */
    char	 *fileName;	 /* Name of the movie file */
    int		 ch;

    /*
     * Process the arguments.
     */

    programName = argv[0];

    if ( argc != 4 ) {
	usage();
    }

    while ( ( ch = getopt( argc, argv, "f:" ) ) != -1 ) {
        switch ( ch ) {
            case 'f':
		frameNum = ( MVframe )atol( optarg );
                break;
            case '?':
                usage();
        }
    }

    fileName = argv[ optind ];

    if ( !( mvIsMovieFile( fileName ) ) ) {
	fprintf( stderr, "%s: %s is not a movie file.\n",
		programName, fileName );
	exit( EXIT_FAILURE ); 
    }

    /*
     * Open the movie.
     */
    
    if ( mvOpenFile( fileName, O_RDONLY, &theMovie ) != DM_SUCCESS ) { 
	fprintf( stderr, "%s: Could not open movie file %s.\n",
		programName, fileName );
	exit( EXIT_FAILURE );
    }

    {
        MVframe frame;
        int hour, minute, second;
        mvFrameToTime(theMovie, frameNum, MV_TIME_SMPTE_30,
                      &hour, &minute, &second, &frame);

        printf ("%.2d:%.2d:%.2d:%.2d\n", hour, minute, second, frame);
    }
    
    mvClose( theMovie );
    exit( EXIT_SUCCESS );
}

/*********
 *
 * Print usage message and exit.
 *
 *********/

static void usage( void )
{
    fprintf( stderr, "usage: %s -f frameNum moviefile\n",
	    programName );
    exit( EXIT_FAILURE ); 
}
