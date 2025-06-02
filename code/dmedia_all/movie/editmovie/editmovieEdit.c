/***************************************************************************** *
 * File:	editmovieEdit.c
 *
 * Description: Part of editmovie. This module contains editTheMovie(), which
 *		is the only external function, and its supporting functions.
 *
 * Functions:	SGI Movie Library functions used:
 *
 *		mvGetParams()
 *		mvDeleteFrames() 
 *		mvReadFrames()
 *		mvInsertFrames()
 *		mvPasteFrames()
 *		mvGetCompressedImageSize()
 *		mvInsertCompressedImage()
 *		mvReadCompressedImage()
 *
 *		SGI Digital Media Libary functions used:
 *
 *		dmImageFrameSize()
 *		dmAudioFrameSize()
 *
 *****************************************************************************/

#include "editmovieArgs.h"
#include "editmovieCopy.h"
#include "editmovieEdit.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <il2.5/ilCdefs.h>
#include <il2.5/ilGenericImgFile.h>
#include <audiofile.h>

/*
 * Forward declarations for local functions.
 */

static void verifyEditFrameRange( MVid theTrack, MVframe theStartFrame );

static void verifySourceFrameRange( MVid theTrack, MVframe theStartFrame );

static AFfilehandle initAudioFile( char *outFile, MVid theTrack );

static DMboolean deleteAllFrames( MVid theTrack, int numFrames );

static DMboolean useCompressedDataFunctions( MVid theEditTrack,
					    MVid theSourceTrack );

static void insertCompressedFrames( MVid theEditMovie, MVid theEditTrack, 
			 	   MVid theSourceMovie, MVid theSourceTrack );

static void insertFrames( MVid theEditMovie, MVid theEditTrack, 
			 MVid theSourceMovie, MVid theSourceTrack );

/*********
 *
 * Edit the movie.
 *
 *********/

void editTheMovie( MVid theEditMovie, MVid theEditTrack,
                         MVid theSourceMovie, MVid theSourceTrack ) 
{
    MVframe  editStartFrame   = getFirstEditFrame();
    MVframe  sourceStartFrame = getFirstSourceFrame();
    MVframe  numFrames 	      = getNumEditFrames();

    verifyEditFrameRange( theEditTrack, editStartFrame );

    if ( ( getEditOperation() != copyOp ) && 
	 ( getEditOperation() != deleteOp ) ) {
        verifySourceFrameRange( theSourceTrack, sourceStartFrame );
    }

    switch( getEditOperation() ) {
	case copyOp:
    	    if ( getEditTrackType() == DM_IMAGE ) {
		copyImageFrames ( theEditTrack, ( int )editStartFrame,
				 ( int )numFrames );
    	    }
    	    else if ( getEditTrackType() == DM_AUDIO ) {
	    	copyAudioFrames ( theEditTrack, ( int )editStartFrame,
				 ( int )numFrames );
    	    }
	break;
	case deleteOp:
            if ( deleteAllFrames( theEditTrack, ( int ) numFrames ) ) {
        	if ( mvRemoveTrack( theEditMovie, theEditTrack ) 
		    != DM_SUCCESS ) {
            	    fprintf( stderr, "%s: Couldn't remove track from %s: %s.\n",
                    getProgramName(), getEditMovieName(),
		    mvGetErrorStr( mvGetErrno() ) );
            	    exit( EXIT_FAILURE );
            	}

	    }
	    else {
            	if ( mvDeleteFrames( theEditTrack, editStartFrame, 
                                    numFrames ) != DM_SUCCESS ) {
                    fprintf( stderr, "%s: Could not delete %d frames starting",
                            getProgramName(), numFrames);
                    fprintf( stderr," at %d from %s.\n",
                            editStartFrame, getEditMovieName() );
                    exit( EXIT_FAILURE );
                }
	    }
	break;
	case insertOp:
            {
		/*
		 * NOTE: mvPasteFrames() automatically copies compressed
		 *	 frames when possible, so in most cases users of
		 * 	 the library will probably not have to deal with
		 *	 the compressed/uncompressed issue. The following
		 *       is included primarily as an example of use of both
		 *	 types of functions.
		 */

                if ( useCompressedDataFunctions( theEditTrack, 
						theSourceTrack ) ) {
		    insertCompressedFrames( theEditMovie, theEditTrack,
					    theSourceMovie, theSourceTrack);
		}
		else {
		    insertFrames( theEditMovie, theEditTrack, theSourceMovie, 
                                 theSourceTrack ); 
		}
            }
	break;
	case pasteOp:

            if ( mvPasteFrames( theSourceTrack, sourceStartFrame, numFrames, 
                               theEditTrack, editStartFrame) != DM_SUCCESS ) {
                fprintf( stderr, "%s: Could not paste %d frames from %s",
                        getProgramName(), numFrames, getSourceMovieName() );
                fprintf( stderr, " to  %s: error: %d: %s\n",
 			getEditMovieName(), mvGetErrno(),
			mvGetErrorStr( mvGetErrno() ) );
                exit( EXIT_FAILURE );
            }

	break;
    }
}

/*********
 *
 * Make sure that the first frame to be edited is within the frame range
 * of the movie to be edited. For delete, make sure that the last frame
 * to be deleted is also inside the frame range.
 *
 *********/

static void verifyEditFrameRange( MVid theTrack, MVframe theStartFrame )
{
    MVframe trackLength   = ( MVframe )mvGetTrackLength( theTrack );
    MVframe lastEditFrame = theStartFrame + ( getNumEditFrames() ) - 1;

    if ( ( theStartFrame > trackLength ) && ( theStartFrame > 0 ) ) {

	fprintf( stderr, "%s: Starting frame number %d is outside ",
		getProgramName(), theStartFrame );
	fprintf( stderr, "the range of frames in %s.\n",
		 getEditMovieName() );
        exit( EXIT_FAILURE );
    }
    if ( ( lastEditFrame > trackLength )
        && ( getEditOperation() == deleteOp ) ) {

	fprintf( stderr, "%s: Can't delete past the end of the movie. ",
		getProgramName() );
	fprintf( stderr, "%s has only %d frames in this track.\n",
		getEditMovieName(), trackLength );
        exit( EXIT_FAILURE );
    }
}

/*********
 *
 * Make sure that the start and end frames specified for the source movie
 * are actually inside that movie.
 *
 *********/

static void verifySourceFrameRange( MVid theTrack, MVframe theStartFrame )
{
    MVframe trackLength = ( MVframe )mvGetTrackLength( theTrack );
    MVframe lastFrame   = theStartFrame + ( getNumEditFrames() ) - 1;

    if ( ( theStartFrame > ( trackLength - 1 ) ) && ( theStartFrame > 0 ) ) {

	fprintf( stderr, "%s: Starting frame number %d is outside ",
		getProgramName(), theStartFrame );
	fprintf( stderr, "the range of frames in %s.\n",
		getSourceMovieName() );
        exit( EXIT_FAILURE );
    }

    if ( lastFrame > trackLength ) {

	fprintf( stderr, "%s: Ending frame is outside the range of %s, ",
		getProgramName(), getSourceMovieName() );
	fprintf( stderr, "which has only %d frames in this track.\n",
		trackLength );
        exit( EXIT_FAILURE );
    }
}

/*********
 *
 * Are all frames in a track to be deleted?
 *
 *********/

static DMboolean deleteAllFrames( MVid theTrack, int numFrames )
{

    if ( mvGetTrackLength( theTrack ) == numFrames ) {
	return DM_TRUE;
    }
    return DM_FALSE;
}

/*********
 *
 * Decide whether or not to use the compressed data functions to do the insert.
 * Only use compressed if inserting image data, and the two movies have image
 * data with the same compression scheme.
 *
 *********/

static DMboolean useCompressedDataFunctions( MVid theEditTrack,
					    MVid theSourceTrack )
{
    if ( ( getEditTrackType() == DM_IMAGE )
        && ( strcmp( mvGetImageCompression( theEditTrack), 
                    mvGetImageCompression( theSourceTrack) ) == 0 ) ) {
	return DM_TRUE;
    }
    return DM_FALSE;
}

/*********
 *
 * Use the compressed data functions to do the insert.
 *
 *********/

static void insertCompressedFrames(MVid theEditMovie, MVid theEditTrack, 
			 	   MVid theSourceMovie, MVid theSourceTrack )
{
    int     i;
    MVframe editStartFrame    = getFirstEditFrame();
    MVframe sourceStartFrame  = getFirstSourceFrame();
    MVframe insFrame  	      = editStartFrame;
    MVframe numFrames 	      = getNumEditFrames();
    size_t  insBuffSize       = 0;
    size_t  lastInsBuffSize   = 0;
    void    *insBuff 	      = NULL;

    for( i = sourceStartFrame; i < numFrames; i++ ) {

    	insBuffSize =  mvGetCompressedImageSize( theSourceTrack, i );

	assert( insBuffSize != 0 );

	/*
	 * Malloc a new buffer only if we need to.
	 */

	if ( ( insBuffSize > lastInsBuffSize ) || ( insBuff == NULL ) ) {

	    lastInsBuffSize = insBuffSize;
            if ( insBuff != NULL ) {
		free( insBuff );
	    }
    	    insBuff = malloc( ( int ) insBuffSize );
    	    if ( insBuff == NULL ) {
	        fprintf( stderr, "%s: Unable to allocate insert buffer.\n",
       	  	        getProgramName() );
	        exit( EXIT_FAILURE );
            }
        }

        if ( mvReadCompressedImage ( theSourceTrack, i, insBuffSize, insBuff ) 
	    != DM_SUCCESS ) {
            fprintf( stderr, "%s: Could not read %d frames from %s.",
                    getProgramName(), numFrames, getSourceMovieName() );
            exit( EXIT_FAILURE );
        }

        if ( mvInsertCompressedImage ( theEditTrack, insFrame, insBuffSize, 
		                      insBuff ) != DM_SUCCESS ) {
	    fprintf( stderr, "%s: Could not insert frame %d from %s",
                    getProgramName(), i, getSourceMovieName() );
            fprintf( stderr, " at %d into %s at %d.\n",
                    sourceStartFrame, getEditMovieName(), insFrame );
            exit( EXIT_FAILURE );
        }
	insFrame++;
    }

    if ( insBuff != NULL ) {
    }   free( insBuff );
}

/*********
 *
 * Use the regular ( uncompressed ) data functions to do the insert.
 *
 *********/

static void insertFrames( MVid theEditMovie, MVid theEditTrack,
                         MVid theSourceMovie, MVid theSourceTrack ) 
{
    MVframe  editStartFrame;
    MVframe  sourceStartFrame;
    MVframe  numFrames = getNumEditFrames();
    size_t   insBuffSize;
    void     *insBuff;
    DMparams *params;

    if ( getEditTrackType() == DM_IMAGE ) {
	params = mvGetParams( theSourceTrack );
	dmParamsSetEnum( params,
			 DM_IMAGE_PACKING,
			 DM_IMAGE_PACKING_XBGR );
	dmParamsSetEnum( params,
			 DM_IMAGE_INTERLACING,
			 DM_IMAGE_NONINTERLACED );
	dmParamsSetEnum( params,
			 DM_IMAGE_ORIENTATION,
			 DM_IMAGE_BOTTOM_TO_TOP );
	insBuffSize = numFrames * 
                      dmImageFrameSize( params );
    }
    else if ( getEditTrackType() == DM_AUDIO ) {
	params = mvGetParams( theSourceTrack );
	dmParamsSetEnum( params,
			 DM_AUDIO_FORMAT,
			 DM_AUDIO_TWOS_COMPLEMENT );
	insBuffSize = numFrames * 
                              dmAudioFrameSize( params );
    }

    insBuff = malloc( ( int ) insBuffSize );
    if ( insBuff == NULL ) {
	fprintf( stderr, "%s: Unable to allocate insert buffer.\n",
       		getProgramName() );
	exit( EXIT_FAILURE );
    }

    {
    	editStartFrame    = getFirstEditFrame();
    	sourceStartFrame  = getFirstSourceFrame();
    }

    if ( mvReadFrames ( theSourceTrack, sourceStartFrame, numFrames,
	               insBuffSize, insBuff ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Could not read %d frames from %s.",
                getProgramName(), numFrames, getSourceMovieName() );
        exit( EXIT_FAILURE );
    }

    if ( mvInsertFrames ( theEditTrack, editStartFrame, numFrames,
		         insBuffSize, insBuff ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Could not insert %d frames from %s",
                getProgramName(), numFrames, getSourceMovieName() );
        fprintf( stderr, " at %d into %s at %d.\n",
                sourceStartFrame, getEditMovieName(), editStartFrame );
        exit( EXIT_FAILURE );
    }

    free( insBuff );
}

