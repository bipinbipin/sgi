/***************************************************************************** *
 * File:	editmovie.c, editmovieArgs.c, editmovieEdit.c, editmovieArgs.h,
 *		editmovieEdit.h
 *
 * Usage:       editmovie <-e editMovie,trackType,startFrame,numFrames>
 *                        <-d> or <-s sourceMovie,startFrame [-i] [-p] >
 *              -e is required, as is either -d or either -i or -p.
 *              These correspond to delete, insert, and paste.
 *              Thus, only one operation at a time may be used. The -i and -p
 *		options require the -s option.
 *
 * Description: Simple command line movie editor. Operations available are
 *		delete, insert, and paste. Only one operation at
 *		a time may be selected.
 *
 * Functions:	SGI Movie Library functions used:
 *
 *		mvOpenFile()
 *		mvCreateMem()
 *		mvFindTrackByMedium() 
 *		mvGetParams()
 *		mvAddTrack()
 *		mvCreateFile()
 *		mvOptimize()
 *		mvClose()
 *
 * 		SGI Digital Media functions used:
 *
 *		dmParamsCreate()
 *		dmParamsDestroy()
 *		dmAudioFrameSize()
 *
 *****************************************************************************/

#include "editmovieArgs.h"
#include "editmovieEdit.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
 * Forward declarations for functions defined after main.
 */

static void   addEditTrack( MVid theEditMovie, MVid theSourceTrack, 
                          MVid *theEditTrack );

static void   addAudioTrack( MVid theEditMovie, MVid theSourceTrack,
                           MVid *theEditTrack );

static void   addImageTrack( MVid theEditMovie, MVid theSourceTrack,
                           MVid *theEditTrack );

static void   optimizeMovie( MVid theEditMovie );

static void   initMovie( MVid *theMovie );

static void   initMemoryMovie( MVid *theMovie, MVid theSourceMovie );

static size_t findMemMovieBuffSize( MVid theSourceMovie );

/*****************************************************************************
 *
 * main
 *
 *****************************************************************************/

main( int argc, char **argv )
{
    MVid     theEditMovie;
    MVid     theSourceMovie;
    MVid     theEditTrack;
    MVid     theSourceTrack;

    processCmdArgs( argc, argv );

    /*
     * Open the source movie if one is expected.
     */

    if ( ( getEditOperation() != copyOp ) &&
	 ( getEditOperation() != deleteOp ) ) {
        if ( mvOpenFile( getSourceMovieName(), O_RDONLY, 
                        &theSourceMovie ) != DM_SUCCESS ) {
            fprintf( stderr, "%s: Could not open movie file %s.\n",
                    getProgramName(), getSourceMovieName() );
            exit( EXIT_FAILURE );
        }

        if ( mvFindTrackByMedium( theSourceMovie, getEditTrackType(), 
                                 &theSourceTrack ) != DM_SUCCESS ) {
            fprintf( stderr, "%s: Could not find track in movie file %s.\n",
                    getProgramName(), getSourceMovieName() );
            exit( EXIT_FAILURE );
        }
    }

    /*
     * Open the movie to be edited.
     */

    if ( editInMemory() ) {

        /*
         * Make an in-memory copy of the movie to edit.
         */

        MVid theFileMovie;

        initMemoryMovie( &theEditMovie, theSourceMovie );

        if ( mvOpenFile( getEditMovieName(), O_RDONLY, &theFileMovie )
            != DM_SUCCESS ) {
            fprintf( stderr, "%s: Could not open movie file %s.\n",
                    getProgramName(), getEditMovieName() );
            exit( EXIT_FAILURE );
        }

	/*
	 * Copy the movie to be edited into memory.
	 */

	if ( mvOptimize( theFileMovie, theEditMovie ) != DM_SUCCESS ) {
            fprintf( stderr, "%s: Could not copy movie %s to memory.\n",
                    getProgramName(), getEditMovieName() );
            exit( EXIT_FAILURE );
        }
    }
    else {
	int openFlag;

	/*
	 * Open read only if copying since the movie will not be changed.
	 */

        if ( getEditOperation() != copyOp ) {
            openFlag = O_RDWR;
	}
	else {
	    openFlag = O_RDONLY;
	}
        if ( mvOpenFile( getEditMovieName(), openFlag, &theEditMovie ) 
            != DM_SUCCESS ) {
            fprintf( stderr, "%s: Could not open movie file %s.\n",
                    getProgramName(), getEditMovieName() );
            exit( EXIT_FAILURE );
        }
    }

    /*
     * Find the track to be edited. If it's not found, and the operation
     * is other than delete add a track of the appropriate type.
     */

    if ( mvFindTrackByMedium( theEditMovie, getEditTrackType(), 
                             &theEditTrack ) != DM_SUCCESS ) {
        if ( getEditOperation() != deleteOp ) {
            addEditTrack( theEditMovie, theSourceTrack, &theEditTrack);
        } 
	else {
            fprintf( stderr, "%s: Can't delete a track that doesn't exist.", 
		    getProgramName() );
            exit( EXIT_FAILURE );
        }
    }

    editTheMovie( theEditMovie, theEditTrack, theSourceMovie, theSourceTrack );

    if ( getOutMovieName() != NULL ) {
	optimizeMovie( theEditMovie );
    }
    mvClose( theEditMovie );
    if ( getSourceMovieName() != NULL ) 
        mvClose( theSourceMovie );
    exit( EXIT_SUCCESS );
}

/*********
 *
 * Add a new track to theEditMovie;
 *
 *********/

static void addEditTrack( MVid theEditMovie, MVid theSourceTrack,
                         MVid *theEditTrack )
{
    if ( getEditTrackType() == DM_AUDIO ) {
        addAudioTrack( theEditMovie, theSourceTrack, theEditTrack );
    }
    else if ( getEditTrackType() == DM_IMAGE ) {
        addImageTrack( theEditMovie, theSourceTrack, theEditTrack );
    }
}

/*********
 *
 * Add an audio track to theEditMovie;
 *
 *********/

static void addAudioTrack( MVid theEditMovie, MVid theSourceTrack,
                           MVid *theEditTrack )
{
    if ( mvAddTrack( theEditMovie, DM_AUDIO, mvGetParams( theSourceTrack ),
                     NULL, theEditTrack ) == DM_FAILURE) {
        fprintf( stderr, "%s: Unable to add audio track to movie.\n",
                getProgramName() );
        exit ( EXIT_FAILURE );
    }
}

/*********
 *
 * Add an image track to theEditMovie;
 *
 *********/

static void addImageTrack( MVid theEditMovie, MVid theSourceTrack,
                           MVid *theEditTrack )
{
    if ( mvAddTrack( theEditMovie, DM_IMAGE, mvGetParams( theSourceTrack ),
                     NULL, theEditTrack ) == DM_FAILURE) {
        fprintf( stderr, "%s: Unable to add image track to movie.\n",
                getProgramName() );
        exit ( EXIT_FAILURE );
    }
}

/*********
 *
 * Create the movie file, outMovie,  optimized for playback.
 *
 *********/

static void optimizeMovie( MVid theEditMovie )
{

    MVid theOutMovie;

    initMovie( &theOutMovie );

    if ( mvOptimize( theEditMovie, theOutMovie ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Unable to optimize movie file: %s\n",
                getProgramName(), mvGetErrorStr( mvGetErrno() ) );
        exit( EXIT_FAILURE );
    }
    mvClose( theOutMovie );
}

/*********
 *
 * Create a new movie file.
 *
 *********/

static void initMovie( MVid *theMovie )
{
    DMparams* movieParams;
   
    if ( dmParamsCreate( &movieParams ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Unable to create default params.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }
   
    if ( mvSetMovieDefaults( movieParams, MV_FORMAT_SGI_3 ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Unable to set default params for %s.\n",
                getProgramName(), getOutMovieName() );
        dmParamsDestroy( movieParams );
        exit( EXIT_FAILURE );
    }

    if ( mvCreateFile( getOutMovieName(), movieParams,
                      NULL, theMovie ) == DM_FAILURE ) {
        fprintf( stderr, "%s: Unable to create movie file %s: error =  %s.\n",
                getProgramName(), getOutMovieName(),
                mvGetErrorStr( mvGetErrno() ) );
        dmParamsDestroy( movieParams );
        exit( EXIT_FAILURE );
    }

    dmParamsDestroy( movieParams );
}

/*********
 *
 * Create an in-memory movie.
 *
 *********/

static void initMemoryMovie( MVid *theMovie, MVid theSourceMovie )
{
    void*        mem = NULL;
    size_t	 memBuffSize;
    DMparams*    movieParams;

    memBuffSize = findMemMovieBuffSize( theSourceMovie );

    mem = ( void *) calloc( memBuffSize, sizeof( char ) );

    if ( mem == NULL ) {
        fprintf( stderr,
                "%s: Cannot allocate memory for in-memory movie.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }
  
    if ( dmParamsCreate( &movieParams ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Unable to create default params in memory.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }
  
    if ( mvSetMovieDefaults( movieParams, MV_FORMAT_SGI_3 ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Unable to set default params in memory.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }

    if ( mvCreateMem( mem, memBuffSize, movieParams, NULL, theMovie )
	!= DM_SUCCESS ) {
        fprintf( stderr, "%s: Unable to create in-memory movie.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }

    dmParamsDestroy( movieParams );
}

/*********
 *
 * Determine the size of memory buffer required for the in-memory movie.
 *
 *********/

static size_t findMemMovieBuffSize( MVid theSourceMovie )
{
    MVid         theSourceTrack;
    size_t	 memSize;	        /* Size of the buffer to be allocated
					   for the in-memory movie. */
    struct stat  statbuf;

    stat( getEditMovieName(), &statbuf );

    memSize = statbuf.st_size;

    /*
     * Rough heuristic for limiting the size of the buffer to be allocated
     * for the in-memory movie. The buffer needs to be larger than the movie
     * itself in the cases of insert and paste operations.
     */

    if ( getEditOperation() != deleteOp ) {

	size_t insertSize = 0;

        if ( mvFindTrackByMedium( theSourceMovie, getEditTrackType(), 
                                 &theSourceTrack ) != DM_SUCCESS ) {
            fprintf( stderr, "%s: Could not find track in movie file %s.\n",
                    getProgramName(), getSourceMovieName() );
            exit( EXIT_FAILURE );
        }

        if ( getEditTrackType() == DM_IMAGE ) {

	    int frame;
	    int firstFrame; 
	    int lastFrame; 

            firstFrame = ( int )getFirstSourceFrame();
            lastFrame = firstFrame + ( int )getNumEditFrames() - 1;

	    for( frame = firstFrame; frame <= lastFrame; frame++ ) {
            	insertSize += mvGetCompressedImageSize( theSourceTrack, frame );  
	    }
	} 
	else {
            
	    size_t aFrameSize = dmAudioFrameSize( mvGetParams( theSourceTrack ) );

	    insertSize +=  ( aFrameSize * getNumEditFrames() );
	}
	memSize += ( insertSize + insertSize/4 );
    }
    return( memSize );
}
