/***************************************************************************** *
 * File:	editmovieArgs.c
 *
 * Description: Command line parsing module for editmovie.
 *
 * Functions:	SGI Movie Library functions used;
 *
 *		mvIsMovieFile()
 *
 *****************************************************************************/

#include <dmedia/moviefile.h>
#include "editmovieArgs.h"
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>

/*
 * Variables set in processCmdArgs() which are restricted to this module.
 */

static char 	*programName;              /* Name of the program. */
static char 	*editMovie = NULL;         /* Movie to be edited. */
static char 	*sourceMovie = NULL;       /* Movie to copy data from. */
static char	*outMovie = NULL;	   /* File in which to place final
					      optimized movie. */
static char	*copyFile = NULL;	   /* File in which to place frames
					      copied from editMovie using
					      the -c option. */
static char	copyFileFormat[ 16 ];      /* Image Vision file format for
					      output image file(s). */
static DMboolean editInMem = DM_FALSE;     /* Edit in memory if true. */
static DMmedium trackType;                 /* Track type to be edited. */
static MVframe 	numFrames;                 /* Number of frames to which the 
				 	      editing operation applies. */
static MVframe 	firstEditFrame;            /* First frame to which editing
 					      operation applies in editmovie. */
static MVframe 	firstSourceFrame;          /* First frame from sourceMovie. */
static editOp 	editOperation = unknownOp; /* The editing operation selected. */

/*
 * Forward declarations of local functions defined after main().
 */

static void 	checkCmdArgs( long frameCount );
static void 	usage( void ); 
DMboolean 	editInMemory( void );
static void 	setEditOperation( editOp operationType );
static void	setCopyFileFormat( char *format );
static char 	*getFirstArgStr( char *argList );
static char 	*getStartOfArg( char *argList );
static int  	getArgLength( char *argList );
static void 	getArg( char *argList, char *arg );

/*********
 *
 * Interpret the command line arguments and do the right thing.
 *
 *********/

void processCmdArgs( int argc, char **argv )
{
    int  	ch;
    int  	argLength;
    long  	frameCount;

    editOperation = unknownOp;
    setCopyFileFormat( "sgi" );

    programName = argv[0];

    if ( argc < 4) {
        usage();
    }

    while ( ( ch = getopt( argc, argv, "dipmc:f:s:e:o:" ) ) != -1 ) {
        switch ( ch ) {
            case 'c':
	    	copyFile = optarg;
	    	setEditOperation( copyOp );
            	break;
	    case 'f':
		setCopyFileFormat( optarg );
		break;
            case 'd':
	    	setEditOperation( deleteOp );
            	break;
       	    case 'i':
	    	setEditOperation( insertOp );
            	break;
            case 'p':
	    	setEditOperation( pasteOp );
            	break;
            case 'm':
		editInMem = DM_TRUE;
            	break;
            case 'e':
            	{
                    /*
                     * Information about the movie to be edited.
                     */

	            char *strTrackType;
	            char *strStartFrame;
	            char *strNumFrames;
                    char *argList;

                    /* 
                     * Get the name of the movie to be edited.
                     */

                    argList = optarg;
	            editMovie = getFirstArgStr( argList );

                    /* 
                     * Verify that it's a movie file.
                     */

		    if ( !( mvIsMovieFile( editMovie ) ) ) {
		        fprintf( stderr, "%s: %s is not a movie file.\n",
		 	        programName, editMovie );
		        exit( EXIT_FAILURE );
		    }

	            /*
	             * Get the type of track.
	             */

                    argList = getStartOfArg( argList );
	            strTrackType = getFirstArgStr( argList );
                    if ( strcmp( strTrackType, "image" ) == 0 ) {
                        trackType = DM_IMAGE;
		    }
                    else if ( strcmp( strTrackType, "audio" ) == 0 ) {
                        trackType = DM_AUDIO;
		    }
                    free( strTrackType );

                    /*
                     * Get the index of the first frame of interest 
		     * in editMovie.
                     */

                    argList = getStartOfArg( argList );
	            strStartFrame = getFirstArgStr( argList );
                    firstEditFrame = ( MVframe )atoi( strStartFrame );
                    free( strStartFrame );

                    /*
                     * Get the number of frames of interest in editMovie.
                     */

                    argList = getStartOfArg( argList );
	            strNumFrames = getFirstArgStr( argList );
                    frameCount = atol( strNumFrames );
                    free( strNumFrames );

                    break;
            	}
            case 's':
            	{
                    /*
                     * Information about the source movie.
                     */

	            char *strStartFrame;
	            char *argList;

                    /* 
                     * Get the name of the movie to copy data from.
                     */

                    argList = optarg;
	            sourceMovie = getFirstArgStr( argList );

                    /* 
                     * Verify that it's a movie file.
                     */

		    if ( !( mvIsMovieFile( sourceMovie ) ) ) {
		        fprintf( stderr, "%s: %s is not a movie file.\n",
		  	        programName, sourceMovie );
		        exit( EXIT_FAILURE );
		    }

                    /*
                     * Get the index of the first frame of interest 
		     * in editMovie.
                     */

                    argList = getStartOfArg( argList );
	            strStartFrame = getFirstArgStr( argList );
                    firstSourceFrame = ( MVframe )atoi( strStartFrame );
                    free( strStartFrame );
   
                    break;
            	}
            case 'o':
	    	outMovie = optarg;
            	break;
            case '?':
                usage();
        }
    }

    checkCmdArgs( frameCount );

    /*
     * Since the number of frames to be edited may be valid, set numFrames.
     */

    numFrames = ( MVframe )frameCount;
}

/*********
 *
 * Perform a first stage check on the arguments. Invalid file names and some
 * out of range frame numbers will still be possible. These can't be
 * checked until we open the movies; they will be caught later.  Pass in 
 * frameCount, since numFrames is  not yet set.
 *
 *********/

static void checkCmdArgs( long frameCount )
{
    /*
     * If either a movie to be edited or a valid edit operation has not 
     * been specified, print instructions and exit.
     */

    if ( ( editMovie == NULL ) || ( editOperation == unknownOp ) ) {
	usage();
    }

    if ( ( editInMem ) && ( outMovie == NULL ) ) {
	usage();
    }

    /*
     * If the track type is incorrectly specified, inform the user and exit.
     */

    if ( ( trackType != DM_IMAGE ) && ( trackType != DM_AUDIO ) ) {
        fprintf( stderr, "%s: Unknown track type specified.\n",
	        programName );
	usage();
    }

    /*
     * If the number of frames specified for editing is <= 0, there's no
     * point in continuing, either.
     */

    if ( frameCount <= 0 ) {
        fprintf( stderr, "%s: Number of frames to be edited must be > 0.\n",
	        programName );
	exit( EXIT_FAILURE );	
    }

    /*
     * If the first frame specified for either the edit or source movie is
     * impossibly low, inform the user and exit.
     */

    if ( ( ( int )firstEditFrame < 0 ) || ( ( int ) firstSourceFrame < 0 ) ) {
        fprintf( stderr, "%s: The starting frame numbers must be >= 0.\n",
	        programName );
	exit( EXIT_FAILURE );	
    }

    /*
     * Since the -s option is pointless in the case of delete, if the user
     * has specified it anyway, ignore it but send a warning.
     */

    if ( ( sourceMovie != NULL ) && ( editOperation == deleteOp ) ) {
        fprintf( stderr, "%s: Warning: -s option not used with -d. Ignored.\n",
	        programName );
        free( sourceMovie );
	sourceMovie = NULL;
    }
}

/*********
 *
 * Print usage message and exit the program.
 *
 *********/

static void usage( void )
{
    fprintf( stderr, 
	    "usage: %s -e editMovie,trackType,firstEditFrame,numFrames\n",
            programName);
    fprintf( stderr, "       [ -c copyFile [ -f imageFormat ] ] [ -d ]\n");
    fprintf( stderr, "       [ -s sourceMovie,firstSrcFrame [ -i ] [ -p ] ] \n" );
    fprintf( stderr, "       [ -m ][-o outMovie ]\n\n" );
    fprintf( stderr, "       ");
    fprintf( stderr, "The movie, track, and frames to edit are set by -e.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "\"trackType\" must be image or audio.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "\"firstEditFrame\" and \"firstSrcFrame\" must be >= 0.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "\"numFrames\" is the number of frames to edit.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "For insert and paste, \"numFrames\" + " );
    fprintf( stderr, "\"firstSrcFrame\"\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "must not exceed the last frame number in \"sourceMovie\".\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "For delete, \"numFrames\" + \"firstEditFrame\"\n" );
    fprintf( stderr, "       ");
    fprintf( stderr, "must not exceed the last frame number in \"editMovie\".\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "Insert and paste require the -s option, which sets\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "the movie and the first frame to copy from.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "Only one operation may be selected at a time.\n\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "Available editing operations are:\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "     -c Copy frames from \"editMovie\" to copyFile.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "        The default output image format for copyFile\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "        is ilSGI, but may be selected with the -f option\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "        where imageFormat may be sgi, fit, or tiff for\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "        ilSGI, ilFIT, or ilTIFF image files, respectively.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "        ilSGI and ilFIT image files are uncompressed,\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "        while ilTIFF image files use LZW compression.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "        Audio files will be in AIFFC format.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "     -d Delete frames from \"editMovie\". If all frames\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "        are deleted, the track will be removed.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "     -i Insert frames from \"sourceMovie\" into \"editMovie\".\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "     -p Paste frames from \"sourceMovie\" into \"editMovie\".\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "        \"sourceMovie\".\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "-o outFile means optimize for playback and place the\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "optimized movie file in \"outMovie\".\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "If -o is not used, the edited file, \"editMovie\",\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "will not be optimized for playback.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "-m means perform the editing on an in-memory copy of");
    fprintf( stderr, " the movie.\n");
    fprintf( stderr, "       ");
    fprintf( stderr, "Use of the -m option requires the -o option, also.\n");
    exit( EXIT_FAILURE );
}

/*********
 *
 * Check if editing is to be performed on in-memory copy of editMovie.
 *
 *********/

DMboolean editInMemory( void )
{
    return editInMem;
}

/*********
 *
 * Validate and set the format for the output image file for copy.
 *
 *********/

static void setCopyFileFormat( char *format )
{
    if ( strcmp( format, "sgi" ) == 0 ) {
	sprintf( copyFileFormat, "SGI" );
    }
    else if ( strcmp( format, "fit" ) == 0 ) {
	sprintf( copyFileFormat, "FIT" );
    }
    else if ( strcmp( format, "tiff" ) == 0 ) {
	sprintf( copyFileFormat, "TIFF" );
    }
    else {
	fprintf( stderr, "%s: Unknown or unavailable image format requested.\n",
		getProgramName() );
	fprintf( stderr, "Available selections are sgi, fit, and tiff\n" );
	exit( EXIT_FAILURE );
    }
}

char *getCopyFileFormat()
{
    return copyFileFormat;
}

/*********
 *
 * Check if an edit operation has already been entered. If so, print the
 * usage message and exit. If not, set the value of editOperation.
 *
 *********/

static void setEditOperation( editOp operationType )
{
    if ( editOperation != unknownOp )
        usage();
    editOperation = operationType;
}

/*********
 *
 * Return the selected edit operation.
 *
 *********/

editOp getEditOperation( void )
{
    return editOperation;
}

/*********
 *
 * Return a pointer to malloced memory containing the first argument
 * string in argList, which is a string with commas dividing the arguments.
 *
 *********/

static char *getFirstArgStr( char *argList )
{
    int argLength = getArgLength( argList );
    char *argString = ( char * )calloc( argLength + 1, sizeof( char ) );
    getArg( argList, argString );
    return( argString );
}

/*********
 *
 * Return a pointer to the start of the next argument, defined as the position
 * of the next argument delimiter, a comma, plus one. If there are no more
 * argument delimiters, return NULL.
 *
 *********/

static char *getStartOfArg( char *argList )
{
    char *argMarker = strchr( argList, ',' );

    if ( argMarker == NULL )
        return( NULL );
    argMarker++;
    return( argMarker );
}

/*********
 *
 * Return the length of the first argument in argList.
 *
 *********/

static int getArgLength( char *argList )
{
    int argLength;
    char *endArg = getStartOfArg( argList );

    /*
     * If argList points to the last argument, call strlen to find the length,
     * otherwise, calculate it.
     */

    if ( endArg == NULL )
        return( strlen( argList ) );
    return( endArg - argList - 1);
}

/*********
 *
 * Copy the first argument in argList to arg.
 *
 *********/

static void getArg( char *argList, char *arg )
{
    int argLength = getArgLength ( argList );
    strncpy( arg, argList, argLength );
    arg[ argLength ] = '\0';
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
 * Return the name of the movie to be edited. 
 *
 *********/

char *getEditMovieName( void )
{
    return editMovie;
}

/*********
 *
 * Return the name of the source movie.
 *
 *********/

char *getSourceMovieName( void )
{
    return sourceMovie;
}

/*********
 *
 * Return the name of the file to which frames are to be copied.
 *
 *********/

char *getCopyFileName( void )
{
    return copyFile;
}

/*********
 *
 * Return the name of the output movie.
 *
 *********/

char *getOutMovieName( void )
{
    return outMovie;
}

/*********
 *
 * Return the type of the track to be edited..
 *
 *********/

DMmedium getEditTrackType( void )
{
    return trackType;
}

/*********
 *
 * Return the number of frames to be inserted, deleted, or pasted.
 *
 *********/

MVframe getNumEditFrames( void )
{
    return numFrames;
}

/*********
 *
 * Return the first frame affected in editMovie.
 *
 *********/

MVframe getFirstEditFrame( void )
{
    return firstEditFrame;
}

/*********
 *
 * Return the first frame to be copied from sourceMovie.
 *
 *********/

MVframe getFirstSourceFrame( void )
{
    return firstSourceFrame;
}

