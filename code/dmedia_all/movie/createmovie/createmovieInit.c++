/******************************************************************************
 *
 * File:         createmovieInit.c++
 *
 * Description:  Part of createmovie. Code for initializing a new movie file.
 *
 * Functions:    The following SGI Movie Library functions are used:
 *
 *               mvOpenFile()
 *               mvSetMovieDefaults()
 *		 mvSetLoopMode()
 *               mvCreateFile()
 *               mvGetErrorStr()
 *               mvGetErrno()
 *               mvGetParams()
 *               mvAddUserParam()
 *		 mvSetParams()
 *               mvAddTrack()
 *               mvFindTrackByMedium()
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
    
static void findFrameSize( int *width, int *height );

static void getDefaultWidthAndHeight( int *width, int *height );

static void getFirstMovieWidthAndHeight( int *width, int* height );

static void getFirstImageWidthAndHeight( int *width, int* height );

static void getFirstMovieFrameRate( double *frameRate );

static void setUpImageTrack( MVid movie );

static void setUpAudioTrack( MVid movie );

static DMstatus initAudioTrack( MVid theMovie );

static void initAudioTrackFromAF( MVid theMovie );

static void initAudioTrackFromMovie( MVid theMovie );


/*********
 *
 * Create the movie.
 *
 ********/

void initMovie( MVid *theMovie )
{
    DMparams* movieParams;
    
    if ( dmParamsCreate( &movieParams ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to create default params.\n",
                getProgramName() );
	exit( EXIT_FAILURE );
    }
    
    if ( mvSetMovieDefaults( movieParams, getMovieFormat() ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to set default params.\n",
                getProgramName() );
        dmParamsDestroy( movieParams );
	exit( EXIT_FAILURE );
    }

    if ( getUserParamName() != NULL ) {
        if ( mvAddUserParam( getUserParamName() ) != DM_SUCCESS ) {
	    fprintf( stderr, "%s: Unable to create default params.\n",
		    getProgramName() );
	exit( EXIT_FAILURE );
        }
        if ( strcmp( getUserParamType(), "movie") == 0 ) {
            if ( dmParamsSetString( movieParams, getUserParamName(), 
			           getUserParamValue() ) != DM_SUCCESS ) {
	        fprintf( stderr, "%s: Unable to set user param.\n", 
			getProgramName() );
                dmParamsDestroy( movieParams );
	        exit( EXIT_FAILURE );
 	    }
        }
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

    if ( mvSetLoopMode( *theMovie, getLoopMode() ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to set movie loop mode.\n",
                getProgramName() );
	exit( EXIT_FAILURE );
    }

    setUpImageTrack( *theMovie );

    if ( ( haveSrcFileType( audioType ) ) 
	|| ( haveSrcFileType( movieType ) ) ) {
      	setUpAudioTrack( *theMovie );
    }
}

/*********
 *
 * Set up the parameter list for the image track. The properties that 
 * must always be set for an image track are width and height. If the
 * function dmSetImageDefaults() is used, then other properties, such
 * as compression,  require setting only if they differ from the
 * defaults.
 *
 *********/

static void setUpImageTrack( MVid movie )
{

    DMparams *imageTrackParams;
    if ( dmParamsCreate( &imageTrackParams ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to create image track params.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }

    int width;
    int height;

    findFrameSize( &width, &height );

    // set the default image params for this movie format
    if ( mvSetImageDefaults( imageTrackParams, width, height, getMovieFormat()
                            ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Unable to set image defaults.\n",
		getProgramName() ); 
        dmParamsDestroy( imageTrackParams );
        exit( EXIT_FAILURE );
    }

    DMimageorientation orientation = getOrientation();
    if ( dmParamsSetEnum( imageTrackParams,
			  DM_IMAGE_ORIENTATION,
			  orientation ) 
 	!= DM_SUCCESS ) {
	fprintf( stderr,
		 "%s: Unable to set orientation.\n", getProgramName() );
	exit( EXIT_FAILURE );
    }

    DMimageinterlacing interlacing = getInterlacing();
    if ( dmParamsSetEnum( imageTrackParams,
			  DM_IMAGE_INTERLACING,
			  interlacing ) 
 	!= DM_SUCCESS ) {
	fprintf( stderr,
		 "%s: Unable to set interlacing.\n", getProgramName() );
	exit( EXIT_FAILURE );
    }
    
    double frameRate = getFrameRate();
    if ( frameRate == OPTION_NOT_SET ) {
        if ( haveSrcFileType( movieType ) ) {
	    getFirstMovieFrameRate( &frameRate );
        }
	else {
	    frameRate = 15.0;
	}
    }
    if ( dmParamsSetFloat( imageTrackParams, DM_IMAGE_RATE, frameRate ) 
 	!= DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to set frame rate.\n", getProgramName() );
	exit( EXIT_FAILURE );
    }
    double spatialQuality = getSpatialQuality();
    if ( spatialQuality != OPTION_NOT_SET ){
	if ( dmParamsSetFloat( imageTrackParams,
			       DM_IMAGE_QUALITY_SPATIAL,
			       spatialQuality ) != DM_SUCCESS ) {
	    fprintf( stderr, "%s: Unable to set quality.\n",
		     getProgramName() );
	    exit( EXIT_FAILURE );
	}
    }
    int keyFrameRate = getKeyFrameRate();
    if ( keyFrameRate != OPTION_NOT_SET ){
	if ( dmParamsSetInt( imageTrackParams,
			     DM_IMAGE_KEYFRAME_DISTANCE,
			     keyFrameRate ) != DM_SUCCESS ) {
	    fprintf( stderr, "%s: Unable to set key frame rate.\n",
		     getProgramName() );
	    exit( EXIT_FAILURE );
	}
    }
    if ( dmParamsSetString( imageTrackParams, DM_IMAGE_COMPRESSION, 
		           getCompressionScheme() ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to set compression.\n", getProgramName() );
	exit( EXIT_FAILURE );
    }

    if ( getUserParamName() != NULL ) {
        if ( strcmp( getUserParamType(), "image") == 0 ) {
            if ( dmParamsSetString( imageTrackParams, getUserParamName(), 
			           getUserParamValue() ) != DM_SUCCESS ) {
	        fprintf( stderr, "%s: Unable to set user param.\n",
			getProgramName() );
                dmParamsDestroy( imageTrackParams );
	        exit( EXIT_FAILURE );
	    }
        }
    }

    

    /*
     * Add an image track to the movie.
     */

    MVid imageTrack;
    if ( mvAddTrack( movie, DM_IMAGE, imageTrackParams, 
                    NULL, &imageTrack ) == DM_FAILURE) {
        fprintf( stderr, "%s: Unable to add image track to movie.\n",
                getProgramName() );
        dmParamsDestroy( imageTrackParams );
        exit( EXIT_FAILURE );
    }
    dmParamsDestroy( imageTrackParams );
}

/*********
 *
 * Find the frame size for the movie. Use the width and height entered
 * by the user if available. Otherwise, use the width and height from the
 * first file, either image or movie, having that information.
 *
 *********/

static void findFrameSize( int *width, int *height )
{
    if ( haveWidthAndHeight() ) {

        *width = getFrameWidth();
        *height = getFrameHeight();
    }
    else {

	getDefaultWidthAndHeight( width, height );
    }
}

/*********
 *
 * Get the width and height from the first file having that information.
 *
 *********/

static void getDefaultWidthAndHeight( int *width, int *height )
{
    int imgWidth;
    int imgHeight;
    int movieWidth;
    int movieHeight;

    if ( haveSrcFileType( imageType ) ) {
        getFirstImageWidthAndHeight( &imgWidth, &imgHeight );
    }
    if ( haveSrcFileType( movieType ) ) {
        getFirstMovieWidthAndHeight( &movieWidth, &movieHeight );
    }

    if ( !( haveSrcFileType( movieType ) ) ) {
	*width = imgWidth;
	*height = imgHeight;
	return;
    }
    if ( !( haveSrcFileType( imageType ) ) ) {
	*width = movieWidth;
	*height = movieHeight;
	return;
    }
    if ( getFilePosition( getFirstFileName( imageType ) ) >
	getFilePosition( getFirstFileName( movieType ) ) ) {
	*width = movieWidth;
	*height = movieHeight;
    }
    else {
	*width = imgWidth;
	*height = imgHeight;
    }
}

/*********
 *
 * Get the width and height from the first movie file.
 *
 *********/

static void getFirstMovieWidthAndHeight( int *width, int *height )
{
    MVid theMovie;

    assert( haveSrcFileType( movieType ) );

    if ( mvOpenFile( getFirstFileName( movieType ), O_RDONLY, &theMovie ) 
        == DM_FAILURE ) {
	fprintf( stderr, "%s: Unable to open movie file %s.\n",
                getProgramName(), getFirstFileName( movieType ) );
	fprintf( stderr,"    error = %s.\n", 
                mvGetErrorStr( mvGetErrno() ) );
        exit( EXIT_FAILURE );
    }
    MVid imageTrack;
    if ( mvFindTrackByMedium( theMovie, DM_IMAGE, &imageTrack )
    	!= DM_SUCCESS ) {

	/*
	 * Something is wrong with this movie file.
	 */

	fprintf( stderr, "%s: Unable to find image track.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }

    *width = mvGetImageWidth( imageTrack );
    *height = mvGetImageHeight( imageTrack );
}
	
/*********
 *
 * Get the width and height from the first image file.
 *
 *********/

static void getFirstImageWidthAndHeight( int *width, int* height )
{
    assert( haveSrcFileType( imageType ) );

    ilImage* image = ilOpenImgFile( getFirstFileName( imageType ), "r" );
    if ( ( image == NULL ) || ( image->getStatus() != ilOKAY ) ) {
	fprintf( stderr, "%s: Unable to open image file - %s.\n",
                getProgramName(), getFirstFileName( imageType ) );
	exit( EXIT_FAILURE );
    }
    ilSize size = image->getSize();
    *width = size.x;
    *height = size.y;
}

/*********
 *
 * Get the frame rate from the first movie file.
 *
 *********/

static void getFirstMovieFrameRate( double *frameRate )
{
    MVid theMovie;

    assert( haveSrcFileType( movieType ) );

    char *sourceMovie = getFirstFileName( movieType );

    if ( mvOpenFile( sourceMovie, O_RDONLY, &theMovie )  == DM_FAILURE ) {
	fprintf( stderr, "%s: Unable to open movie file %s.\n",
                getProgramName(), sourceMovie );
	fprintf( stderr,"    error = %s.\n", 
                mvGetErrorStr( mvGetErrno() ) );
        exit( EXIT_FAILURE );
    }
    MVid imageTrack;
    if ( mvFindTrackByMedium( theMovie, DM_IMAGE, &imageTrack )
    	!= DM_SUCCESS ) {

	/*
	 * Something is wrong with this movie file.
	 */

	fprintf( stderr, "%s: Unable to find image track.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }

    *frameRate = mvGetImageRate( imageTrack );
}
	
/*********
 *
 * SetUpAudioTrack
 *
 *********/

static void setUpAudioTrack( MVid theMovie )
{
    /* 
     * If there is no possibility of an audio track, return.
     */

    if ( !( haveSrcFileType( audioType ) ) 
	&& !( haveSrcFileType( movieType ) ) ) {
	return;
    }
    
    /* 
     * If this fails, then we're making a silent movie.
     */

    if ( initAudioTrack( theMovie ) != DM_SUCCESS ) {
	return;
    }
}

/*********
 *
 * Set the DMparams for the audio track from the first file, audio or
 * movie, which was entered on the command line that has that information.
 *
 *********/

static DMstatus initAudioTrack( MVid theMovie )
{
    int firstAudioFilePos;
    int firstAudioMoviePos;
    DMboolean movieHasAudio = DM_FALSE;

    if ( haveSrcFileType( movieType ) ) {
	char *audioMovie = getFirstAudioMovie();
	if ( audioMovie != NULL ) {
	    movieHasAudio = DM_TRUE;
	    firstAudioMoviePos = getFilePosition( audioMovie );
	}
    }
    else if ( haveSrcFileType( audioType ) ) {
	initAudioTrackFromAF( theMovie );
	return DM_SUCCESS;
    }
    else {
	return DM_FAILURE;
    }

    if ( haveSrcFileType( audioType ) && movieHasAudio ) {
	firstAudioFilePos = getFilePosition( getFirstFileName( audioType ) );
	if ( firstAudioFilePos > firstAudioMoviePos ) {
            initAudioTrackFromMovie( theMovie );
	}
        else {
	    initAudioTrackFromAF( theMovie );
	}
    }
    else if ( haveSrcFileType( audioType ) && !movieHasAudio ) {
	initAudioTrackFromAF( theMovie );
    }
    else if ( !( haveSrcFileType( audioType ) ) && movieHasAudio ) {
	initAudioTrackFromMovie( theMovie );
    }
    else {
	return DM_FAILURE;
    }

    return DM_SUCCESS;
}
    
/*********
 *
 * Set up audio track parameters from audio file.
 *
 *********/

static void initAudioTrackFromAF( MVid theMovie )
{
    AFfilehandle audioFile = AFopenfile( getFirstFileName( audioType ) , "r",
					AF_NULL_FILESETUP );

    if ( audioFile == AF_NULL_FILEHANDLE ) {
	fprintf( stderr, "Unable to open audio file - %s.\n",
                getProgramName(), getFirstFileName( audioType ) );
	exit( EXIT_FAILURE );
    }
    
    /*
     * The properties that must always be set for an audio track are:
     * sample width, sample rate, sample format, number of channels.
     */
 
    int	fileWidth;
    int	fileFormat;
    AFgetsampfmt( audioFile, AF_DEFAULT_TRACK, &fileFormat, &fileWidth );
    
    double fileRate   = AFgetrate( audioFile, AF_DEFAULT_TRACK );
    long fileChannels = AFgetchannels( audioFile, AF_DEFAULT_TRACK );
    
    DMparams *audioTrackParams;
    if ( dmParamsCreate( &audioTrackParams ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Couldn't create audio params.\n",
		getProgramName() ); 
        exit( EXIT_FAILURE );
    }

    if ( mvSetAudioDefaults( audioTrackParams, ( int )fileWidth,
			    fileRate, ( int )fileChannels, getMovieFormat() ) 
	!= DM_SUCCESS ) {
                            
        fprintf( stderr, "%s: Couldn't set audio defaults.\n",
		getProgramName() ); 
        exit( EXIT_FAILURE );
    }
    
    if ( dmParamsSetEnum( audioTrackParams, DM_AUDIO_FORMAT, 
                         ( int )fileFormat ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Couldn't set audio format.\n",
		getProgramName() ); 
        exit( EXIT_FAILURE );
    }

    if ( getUserParamName() != NULL ) {
        if ( strcmp( getUserParamType(), "audio") == 0 ) {
            if ( dmParamsSetString( audioTrackParams, getUserParamName(), 
			           getUserParamValue() ) != DM_SUCCESS ) {
	        fprintf( stderr, "%s: Unable to set user param.\n",
			getProgramName() );
            	dmParamsDestroy( audioTrackParams );
	    	exit( EXIT_FAILURE );
	    }
        }
    }

    /*
     * Add an audio track to the movie.
     */

    MVid audioTrack;
    if ( mvAddTrack( theMovie, DM_AUDIO, audioTrackParams, NULL, 
        &audioTrack ) == DM_FAILURE) {
	fprintf( stderr, "%s: Unable to add audio track to movie.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }
    dmParamsDestroy( audioTrackParams );
}
    
/*********
 *
 * Set up audio track parameters for theMovie from a movie file.
 *
 *********/

static void initAudioTrackFromMovie( MVid theMovie )
{
    MVid theSrcMovie; 
    if ( mvOpenFile( getFirstAudioMovie(), O_RDONLY, &theSrcMovie ) 
        == DM_FAILURE ) {
	fprintf( stderr, "%s: Unable to open movie file. Error = %s.\n",
                getProgramName(), mvGetErrorStr( mvGetErrno() ) );
        exit( EXIT_FAILURE );
    }
    
    MVid audioSrcMovieTrack;
    if ( mvFindTrackByMedium( theSrcMovie, DM_AUDIO, &audioSrcMovieTrack )
	!= DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to find audio track.\n",
		getProgramName() );
        exit( EXIT_FAILURE );
    }

    /*
     * Add an audio track to the movie.
     */

    MVid audioTrack;
    if ( mvAddTrack( theMovie, DM_AUDIO, mvGetParams(audioSrcMovieTrack), NULL,
        &audioTrack ) == DM_FAILURE) {
	fprintf( stderr, "%s: Unable to add audio track to movie.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }

    if ( getUserParamName() != NULL ) {
        if ( strcmp( getUserParamType(), "audio") == 0 ) {
            DMparams *audioTrackParams;
            if ( dmParamsCreate( &audioTrackParams ) != DM_SUCCESS ) {
                fprintf( stderr, "%s: Couldn't create audio params.\n",
		        getProgramName() ); 
                exit( EXIT_FAILURE );
            }

            if ( dmParamsSetString( audioTrackParams, getUserParamName(), 
			           getUserParamValue() ) != DM_SUCCESS ) {
	        fprintf( stderr, "%s: Unable to set user param value.\n",
	 	        getProgramName() );
                dmParamsDestroy( audioTrackParams );
	        exit( EXIT_FAILURE );
	    }

	    if ( mvSetParams( audioTrack, audioTrackParams, NULL )
		!= DM_SUCCESS ) { 
	        fprintf( stderr, "%s: Unable to set user param.\n",
			getProgramName() );
            	dmParamsDestroy( audioTrackParams );
	    	exit( EXIT_FAILURE );
	    }
            dmParamsDestroy( audioTrackParams );
        }
    }

}
