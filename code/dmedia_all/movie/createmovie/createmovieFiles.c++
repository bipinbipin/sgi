/******************************************************************************
 *
 * File:         createmovieFiles.c++
 *
 * Description:  Part of createmovie. Code for putting files into a new movie.
 *
 * Functions:    The following SGI Movie Library functions are used:
 *
 *               mvOpenFile()
 *               mvSetMovieDefaults()
 *               mvCreateFile()
 *               mvGetErrorStr()
 *               mvGetErrno()
 *               mvClose()
 *               mvGetParams()
 *               mvAddTrack()
 *               mvFindTrackByMedium()
 *               mvInsertFrames()
 *               mvGetAudioRate()
 *               mvGetImageRate()
 *               mvGetAudioWidth()
 *               mvGetTrackLength()
 *
 *               The following SGI Digital Media Library functions are used:
 *
 *               dmParamsCreate()
 *               dmParamsDestroy()
 *               dmSetImageDefaults()
 *               dmSetAudioDefaults()
 *               dmParamsSetInt()
 *               dmParamsSetFloat()
 *               dmParamsSetEnum()
 *               dmParamsSetString()
 *               dmImageFrameSize()
 *
 ******************************************************************************/

/*
 * Standard Unix stuff.
 */

#include <sys/types.h>	/* For open(2)  */
#include <sys/stat.h>	/* For open(2)  */
#include <fcntl.h>	/* For open(2)  */
#include <stdlib.h>	/* For exit(2)  */
#include <unistd.h>	/* For close(2) */
#include <string.h>	/* For close(2) */
#include <assert.h>

#include "createmovieArgs.h"
#include "createmovieResize.h"
#include <dmedia/moviefile.h>
#include <il2.5/ilImage.h>
#include <il2.5/ilGenericImgFile.h>
#include <il2.5/ilABGRImg.h>
#include <dmedia/audiofile.h>

/*
 * Forward declarations of functions that appear below.
 */
    
static void putImageInMovie( char *imageFileName, MVid theMovie );

static void putAudioInMovie( char *audioFileName, MVid theMovie );

static void putMovieInMovie( char *movieFileName, MVid theMovie );

static void putTrackInMovie( MVid theSourceMovie, MVid theMovie,
			    DMmedium trackType );

static void insertFrames( MVid theSourceTrack, MVid theTrack );

DMboolean isResizeNeeded( MVid theSourceTrack, MVid theTrack );

static void insertResizeImageFrames( MVid theSourceTrack, MVid theTrack );

/*********
 *
 * Put the image, movie, and/or audio files into the movie.
 *
 *********/

void putFilesInMovie( MVid theMovie )
{
    srcFileType  theType;
    char	 *theFile;

    for( int i = 0; i < getNumFiles(); i++ ) {

	getFileAndTypeByPos( i, &theFile, &theType );

	switch( theType ) {
	    case imageType:
		putImageInMovie( theFile, theMovie );
		break;
	    case audioType:
		putAudioInMovie( theFile, theMovie );
		break;
	    case movieType:
		putMovieInMovie( theFile, theMovie );
		break;
	} 
    }
}

/*********
 *
 * CopyImages
 *
 *********/

static void putImageInMovie( char *imageFileName, MVid theMovie )
{
    /*
     * Open the image file.
     */
    
    ilImage* image = ilOpenImgFile( imageFileName, "r" );
    if ( ( image == NULL ) || ( image->getStatus() != ilOKAY ) ) {
	fprintf( stderr, "%s: Unable to open image file - %s.\n",
                getProgramName(), imageFileName );
	exit( EXIT_FAILURE );
    }

    /*
     * Convert it to ABGR if it is not already.
     */

    ilImage* convertedImage = NULL;
    ilImage* useImage       = image;      // Don't delete this
                                          // pointer.  It points to
                                          // one of the other two images.
   
    if ( image->getColorModel() != ilABGR )  {
        convertedImage = new ilABGRImg( image );
        useImage       = convertedImage;
    }

    /*
     * Locate the image track of the movie.
     */

    MVid imageTrack;
    if ( mvFindTrackByMedium( theMovie, DM_IMAGE, &imageTrack )
	!= DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to find image track.\n",
		getProgramName() );
        exit( EXIT_FAILURE );
    }

    /*
     * Make an ilConfig to tell the image what kind of data to give us.
     * We want 4 channels, interleaved, as unsigned chars.
     */

    ilConfig imageConfig( ilUChar, ilInterleaved, 4, NULL ); 

    /*
     * Allocate a buffer big enough to hold the image.
     */

    DMparams *params = mvGetParams( imageTrack );
    dmParamsSetEnum( params,
		     DM_IMAGE_PACKING,
		     DM_IMAGE_PACKING_XBGR );
    dmParamsSetEnum( params,
		     DM_IMAGE_INTERLACING,
		     DM_IMAGE_NONINTERLACED );
    dmParamsSetEnum( params,
		     DM_IMAGE_ORIENTATION,
		     DM_IMAGE_BOTTOM_TO_TOP );
    size_t imageSize = dmImageFrameSize( params );

    ilSize size = useImage->getSize();

    {
        size_t sourceImageSize = size.x * size.y * size.c;

        if ( imageSize < sourceImageSize ) {
	    imageSize = sourceImageSize; 
        }

    }
    
    void* imageBuff = malloc( ( int ) imageSize );
    if ( imageBuff == NULL ) {
	fprintf( stderr, "%s: Unable to allocate buffer.\n", getProgramName() );
	exit( EXIT_FAILURE );
    }

    /*
     * Loop through the Z axis to pick up all of the images in the
     * file.  For each one, copy to the buffer and then put it into the
     * movie. 
     */ 

    int insFrame = ( int )mvGetTrackLength( imageTrack );

    for ( int z = 0;  z < size.z;  z++ ) {

	useImage->getTile3D( 0, 0, z, 		/* Origin in image file. */
			    size.x, size.y, 1,  /* Size of tile. */
			    imageBuff,          /* Destination buffer */
			    &imageConfig );     /* What kind of data to get */

	/*
	 * Resize the image if necessary.
	 */

	if ( ( size.x != mvGetImageWidth( imageTrack ) ) 
	    || ( size.y != mvGetImageHeight( imageTrack ) ) ) {

	    void *oldBuffer = imageBuff;
    	    imageBuff = malloc( ( int ) imageSize );
    	    if ( imageBuff == NULL ) {
		fprintf( stderr, "%s: Unable to allocate buffer.\n", 
			getProgramName() );
		exit( EXIT_FAILURE );
    	    }
	    resizeImageFrame( size.x, size.y, oldBuffer,
                             mvGetImageWidth( imageTrack ),
			     mvGetImageHeight( imageTrack ), 
			     imageBuff );
	    free( oldBuffer );
	}

	if ( mvInsertFrames( imageTrack, insFrame, 1, 
                            imageSize, imageBuff ) != DM_SUCCESS ) {
	    fprintf( stderr, "%s: Unable to write image to movie.\n",
                    getProgramName());
            free( imageBuff );
	    exit( EXIT_FAILURE );
	}
	insFrame ++;
    }
    
    free( imageBuff );
    delete( image );
    if ( convertedImage != NULL ) {
	delete( convertedImage );
    }
}

/*********
 *
 * Put an audio file into the new movie file.
 *
 *********/

static void putAudioInMovie( char *audioFileName, MVid theMovie )
{
    /*
     * Open the audio file.
     */
    
    AFfilehandle audioFile;
    audioFile = AFopenfile( audioFileName, "r", AF_NULL_FILESETUP );
    if ( audioFile == AF_NULL_FILEHANDLE) {
	fprintf( stderr, "Unable to open audio file - %s.\n",
                getProgramName(), audioFileName );
	exit( EXIT_FAILURE );
    }
    
    /*
     * We know that the movie has an image track and an audio track,
     * so we can compute the number of audio samples per image frame.
     * Writing the audio in chunks that are this size is not
     * necessary, but it is a convenient size to use.
     */
    
    MVid	audioTrack;
    if ( mvFindTrackByMedium( theMovie, DM_AUDIO, &audioTrack )
	!= DM_SUCCESS ) {
        fprintf( stderr, "%s: Couldn't find audio track.\n", getProgramName() );
        AFclosefile( audioFile );
	exit( EXIT_FAILURE );
    }
    MVid	imageTrack;
    if ( mvFindTrackByMedium( theMovie, DM_IMAGE, &imageTrack ) 
	!= DM_SUCCESS ) {
        fprintf( stderr, "%s: Couldn't find image track.\n", getProgramName() );
        AFclosefile( audioFile );
	exit( EXIT_FAILURE );
    }
	
    double audioRate = mvGetAudioRate( audioTrack );
    double imageRate = mvGetImageRate( imageTrack );
    int chunkSize = ( int ) ( audioRate / imageRate );

    /*
     * Allocate a buffer big enough to hold that many audio samples.
     */
    
    int sampleSize = mvGetAudioWidth( audioTrack );

    void *buffer = malloc( chunkSize * sampleSize );
    if ( buffer == NULL ) {
        fprintf( stderr, "%s: Couldn't allocate audio buffer.\n",
		getProgramName() );
        AFclosefile( audioFile );
	exit( EXIT_FAILURE );
    }
    
    /*
     * Copy the audio in chunks corresponding to image frames.
     */

    long audioDataUnwritten = AFgetframecnt( audioFile, AF_DEFAULT_TRACK );

    int insFrame = ( int )mvGetTrackLength( audioTrack );
    while( audioDataUnwritten > 0 ) {
	long framesReadAF = 0;
	memset( buffer, 0, chunkSize * sampleSize );
	framesReadAF = AFreadframes( audioFile, 
 				    AF_DEFAULT_TRACK, buffer, chunkSize );
	audioDataUnwritten -= framesReadAF;
	if ( mvInsertFrames( audioTrack, insFrame * chunkSize, chunkSize,
                           ( size_t ) chunkSize * sampleSize, 
                           buffer ) != DM_SUCCESS ) {
             fprintf( stderr, "%s: Couldn't write audio data frame.\n",
                     getProgramName() );
             free( buffer );
             AFclosefile( audioFile );
	     exit( EXIT_FAILURE );
        }
	insFrame++;
    }
    AFclosefile( audioFile );
    free( buffer );
}

/*********
 *
 * Put movie files into the new movie.
 *
 *********/

static void putMovieInMovie( char *movieFileName, MVid theMovie )
{

    MVid theSourceMovie;
    if ( mvOpenFile( movieFileName, O_RDONLY, &theSourceMovie )
	!= DM_SUCCESS ) {
        fprintf( stderr, "%s: Could not open movie file %s.\n",
                getProgramName(), movieFileName );
        exit( EXIT_FAILURE );
    }

    putTrackInMovie( theSourceMovie, theMovie, DM_IMAGE );

    MVid theSourceTrack;
    if ( mvFindTrackByMedium( theSourceMovie, DM_AUDIO,
                             &theSourceTrack ) == DM_SUCCESS ) {
    	putTrackInMovie( theSourceMovie, theMovie, DM_AUDIO );
    }

    mvClose( theSourceMovie );
}

/*********
 *
 * For tracks of type trackType, put the track from theSourceMovie into 
 * theMovie at the end of the track of the same type in theMovie.
 *
 *********/

static void putTrackInMovie( MVid theSourceMovie, MVid theMovie,
				  DMmedium trackType) 
{
    MVid theSourceTrack;

    if ( mvFindTrackByMedium( theSourceMovie, trackType,
                             &theSourceTrack ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Could not find track in movie file.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }
    MVid theTrack;
    if ( mvFindTrackByMedium( theMovie, trackType, &theTrack ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Could not find track in movie file %s.\n",
                getProgramName(), getOutMovieName() );
        exit( EXIT_FAILURE );
    }
    insertFrames( theSourceTrack, theTrack );
}

/*********
 *
 * Insert all frames from theSourceTrack into theTrack.
 *
 *********/

static void insertFrames( MVid theSourceTrack, MVid theTrack )
                         
{
    if ( isResizeNeeded( theSourceTrack, theTrack ) ) {

 	insertResizeImageFrames( theSourceTrack, theTrack );
    }
    else {
        size_t  insBuffSize;
        void    *insBuff;
	int     insFrames;
        int     srcFrames = ( int )mvGetTrackLength( theSourceTrack );
        int     insertStartFrame = ( int )mvGetTrackLength( theTrack );

        if ( mvGetTrackMedium( theSourceTrack ) == DM_IMAGE ) {
	    insFrames = 1;

	    DMparams *params = mvGetParams( theSourceTrack );
	    
	    dmParamsSetEnum( params,
			     DM_IMAGE_PACKING,
			     DM_IMAGE_PACKING_XBGR );
	    dmParamsSetEnum( params,
			     DM_IMAGE_INTERLACING,
			     DM_IMAGE_NONINTERLACED );
	    dmParamsSetEnum( params,
			     DM_IMAGE_ORIENTATION,
			     DM_IMAGE_BOTTOM_TO_TOP );
	    
	    insBuffSize = ( size_t ) dmImageFrameSize( params );
        }
        else if ( mvGetTrackMedium( theSourceTrack ) == DM_AUDIO ) {
	    insFrames = srcFrames;

	    DMparams *params = mvGetParams( theSourceTrack );
	    
	    dmParamsSetEnum( params,
			     DM_AUDIO_FORMAT,
			     DM_AUDIO_TWOS_COMPLEMENT );
	    
            insBuffSize = ( size_t )( insFrames * dmAudioFrameSize( params ) );
        }
        insBuff = malloc( ( int ) insBuffSize );
        if ( insBuff == NULL ) {
            fprintf( stderr, "%s: Unable to allocate insert buffer.\n",
                    getProgramName() );
            exit( EXIT_FAILURE );
        }
        for( int theFrame = 0; theFrame < srcFrames/insFrames; theFrame++ ) {

            if ( mvReadFrames ( theSourceTrack, theFrame, insFrames,
                               insBuffSize, insBuff ) != DM_SUCCESS ) {
                fprintf( stderr, 
			"%s: Could not read frame %d from movie file.",
                        theFrame, getProgramName() );
                exit( EXIT_FAILURE );
            }

            if ( mvInsertFrames ( theTrack, insertStartFrame + theFrame,
				 insFrames, insBuffSize, insBuff ) 
				 != DM_SUCCESS ) {
                fprintf( stderr, "%s: Could not insert frame at %d ",
                        getProgramName(), theFrame );
                fprintf( stderr, "into %s at %d.\n",
                        getOutMovieName(), insertStartFrame + theFrame );
                exit( EXIT_FAILURE );
            }
        }
        free( insBuff );
    }
}

/*********
 *
 * Decide if resizing the image is necessary.
 *
 *********/

DMboolean isResizeNeeded( MVid theSourceTrack, MVid theTrack )
{
    if ( ( mvGetTrackMedium( theTrack ) == DM_IMAGE ) 
	&& !( ( mvGetImageWidth( theSourceTrack ) 
	    == mvGetImageWidth( theTrack ) ) 
	    && ( mvGetImageHeight( theSourceTrack )
	        == mvGetImageHeight( theTrack ) ) ) ) {

	return( DM_TRUE );
    }
    return( DM_FALSE );
}

/*********
 *
 * Resize the images from theSourceTrack, then insert them in theTrack.
 *
 *********/

static void insertResizeImageFrames( MVid theSourceTrack, MVid theTrack )
{
    DMparams *params = mvGetParams( theSourceTrack );
    dmParamsSetEnum( params,
		     DM_IMAGE_PACKING,
		     DM_IMAGE_PACKING_XBGR );
    dmParamsSetEnum( params,
		     DM_IMAGE_INTERLACING,
		     DM_IMAGE_NONINTERLACED );
    dmParamsSetEnum( params,
		     DM_IMAGE_ORIENTATION,
		     DM_IMAGE_BOTTOM_TO_TOP );
    size_t oldImageSize = dmImageFrameSize( params );

    void *oldBuff = malloc( ( int ) oldImageSize );
    if ( oldBuff == NULL ) {
	fprintf( stderr, "%s: Unable to allocate %d bytes of memory.\n", 
		getProgramName(), oldImageSize );
	exit( EXIT_FAILURE );
    }

    params = mvGetParams( theTrack );
    dmParamsSetEnum( params,
		     DM_IMAGE_PACKING,
		     DM_IMAGE_PACKING_XBGR );
    dmParamsSetEnum( params,
		     DM_IMAGE_INTERLACING,
		     DM_IMAGE_NONINTERLACED );
    dmParamsSetEnum( params,
		     DM_IMAGE_ORIENTATION,
		     DM_IMAGE_BOTTOM_TO_TOP );
    size_t imageSize = dmImageFrameSize( params );
    void *imageBuff = malloc( ( int ) imageSize );
    if ( imageBuff == NULL ) {
	fprintf( stderr, "%s: Unable to allocate %d bytes of memory.\n", 
		getProgramName(), imageSize );
	exit( EXIT_FAILURE );
    }

    int numFrames        = ( int )mvGetTrackLength( theSourceTrack );
    int insertStartFrame = ( int )mvGetTrackLength( theTrack );
    int insFrame         = insertStartFrame;

    for( int i = 0; i < numFrames; i++ ) {
        if ( mvReadFrames ( theSourceTrack, i, 1,
                           oldImageSize, oldBuff ) != DM_SUCCESS ) {
            fprintf( stderr, "%s: Could not read frame %d from movie.",
                    getProgramName(), i );
            exit( EXIT_FAILURE );
        }
	resizeImageFrame( mvGetImageWidth( theSourceTrack ), 
		         mvGetImageHeight( theSourceTrack ), 
			 oldBuff,
                         mvGetImageWidth( theTrack ),
			 mvGetImageHeight( theTrack ), 
			 imageBuff );

        if ( mvInsertFrames( theTrack, insFrame, 1,
			    imageSize, imageBuff ) != DM_SUCCESS ) {
            fprintf( stderr, "%s: Could not insert frame %d in %s.\n",
                    getProgramName(), insFrame, getOutMovieName );
            exit( EXIT_FAILURE );
        }
	insFrame++;
    }
    free( oldBuff );
    free( imageBuff );
}
