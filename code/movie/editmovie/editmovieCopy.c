/***************************************************************************** *
 * File:        editmovieCopy.c
 *
 * Description: Part of editmovie. This module contains copyImageFiles()
 *              and copyAudioFiles() which are used by editTheMovie() in
 *              editmovieEdit.c. Uses the ImageVision Library, libil, and
 *              the Audio File Library, libaudiofile.
 *
 * Functions:   SGI Movie Library functions used:
 *
 *              mvGetImageWidth()
 *              mvGetImageHeight()
 *              mvReadFrames()
 *              mvGetAudioChannels()
 *              mvGetAudioFormat()
 *              mvGetAudioRate()
 *              mvGetAudioWidth()
 *
 *              SGI Digital Media Libary functions used:
 *
 *              dmImageFrameSize()
 *              dmAudioFrameSize()
 *
 *****************************************************************************/

#include "editmovieCopy.h"
#include "editmovieArgs.h"
#include <stdio.h>
#include <string.h>
#include <il2.5/ilCdefs.h>
#include <audiofile.h>

/*
 * Forward declarations for local functions.
 */

static void 	    setupOutImage( MVid theTrack, ilSize *copySize,
			           ilConfig *copyConfig, char * outSuffix );
static ilImage 	    *initNewTmpImage( MVid theTrack );
static ilImage      *initNewOutImage( char *outFile, ilSize *outSize );
static AFfilehandle initAudioFile( char *outFile, MVid theTrack );

/*********
 *
 * Copy specified image frame(s) from editMovie to image file(s).
 *
 *********/

void copyImageFrames( MVid theTrack, int firstFrame, int numFrames )
{
    int          i;
    char         outFile[ 512 ];
    char         outSuffix[ 12 ];
    ilSize       copySize;
    ilImage      *copyImage       = NULL;
    ilImage      *tmpImage        = NULL;
    ilRGBImg     *convertImage    = NULL;
    ilConfig     *copyConfig      = NULL;
    void         *readBuff        = NULL;
    DMparams     *params          = mvGetParams( theTrack );
    size_t	 readBuffSize     = NULL;
    
    dmParamsSetEnum( params,
		     DM_IMAGE_PACKING,
		     DM_IMAGE_PACKING_XBGR );
    dmParamsSetEnum( params,
		     DM_IMAGE_INTERLACING,
		     DM_IMAGE_NONINTERLACED );
    dmParamsSetEnum( params,
		     DM_IMAGE_ORIENTATION,
		     DM_IMAGE_BOTTOM_TO_TOP );

    readBuffSize = dmImageFrameSize( params );

    readBuff = malloc( ( int ) readBuffSize );
    if ( readBuff == NULL ) {
        fprintf( stderr, "%s: Unable to allocate image buffer.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }

    tmpImage = initNewTmpImage( theTrack );

    setupOutImage( theTrack, &copySize, copyConfig, outSuffix );

    for ( i = 0; i < numFrames; i++ ) {

        if ( mvReadFrames ( theTrack, firstFrame + i, 1,
                           readBuffSize, readBuff ) != DM_SUCCESS ) {
            fprintf( stderr, "%s: Could not read frame %d from %s.",
                    getProgramName(), firstFrame + i, getEditMovieName() );
            exit( EXIT_FAILURE );
        }

        /*
         * Copy the image frame just read into a temporary image in memory.
         */

        if ( ilSetTile( ( ilImage *) tmpImage, 0, 0, copySize.x, copySize.y,
                       readBuff, copyConfig) != ilOKAY ) {
            fprintf( stderr, "%s: Unable to copy data to memory image.\n",
                    getProgramName() );
            exit( EXIT_FAILURE );
        }

        /*
         * Convert the image data into RGB format.
         */

        convertImage = ilRGBImgCreate( ( ilImage *) tmpImage );
        if ( convertImage == NULL ) {
            fprintf( stderr, "%s: Unable to create RGB image.\n",
                    getProgramName() );
            exit( EXIT_FAILURE );
        }

        /*
         * Create the output image file.
         */

        sprintf( outFile, "%s%06d.%s", getCopyFileName(), i, outSuffix );
        copyImage = initNewOutImage( outFile, &copySize );

        /*
         * Copy the image frame to the output image file.
         */

        if ( ilCopyTile( copyImage, 0, 0, copySize.x, copySize.y,
                        ( ilImage *) convertImage, 0, 0, NULL, 1 ) != ilOKAY ) {
            fprintf( stderr, "%s: Unable to copy data to image file %s.\n",
                    getProgramName(), outFile );
            exit( EXIT_FAILURE );
        }
        ilImageDelete( copyImage );
        ilImageDelete( ( ilImage *) convertImage );
    }
    ilImageDelete( ( ilImage *) tmpImage );
    ilConfigDelete( copyConfig );
    free( readBuff );
}

/*********
 *
 * Does some necessary preliminaries for later use by libil.
 *
 *********/

static void setupOutImage( MVid theTrack, ilSize *copySize,
				 ilConfig *copyConfig, char * outSuffix )
{
    copySize->x = mvGetImageWidth( theTrack );
    copySize->y = mvGetImageHeight( theTrack );
    copySize->z = 1;
    copySize->c = 3;
  
    if ( strcmp( getCopyFileFormat(), "FIT" ) == 0 ) {
        sprintf( outSuffix, "fit" );
    }
    else if ( strcmp( getCopyFileFormat(), "TIFF" ) == 0 ) {
        sprintf( outSuffix, "tiff" );
    }
    else {
        sprintf( outSuffix, "sgi" );
    }

    copyConfig = ilConfigCreate( ilUChar, ilInterleaved, 4, NULL, 0,
                                    ilLowerLeftOrigin, ilABGR );
    if ( copyConfig == NULL ) {
        fprintf( stderr, "%s: Unable to allocate new ilConfig.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }
}

/*********
 *
 * Create a temporary image file in memory for copying image frames from
 * the movie.
 *
 *********/

static ilImage *initNewTmpImage( MVid theTrack )
{
    ilSize       tmpSize;
    ilMemoryImg  *memImage = NULL;
    
    tmpSize.x = mvGetImageWidth( theTrack );
    tmpSize.y = mvGetImageHeight( theTrack );
    tmpSize.z = 1;
    tmpSize.c = 4;
  
    memImage = ilMemoryImgCreate( &tmpSize, ilUChar, ilInterleaved );
    if( memImage == NULL ) {
       fprintf( stderr, "%s: Unable to create new memory image.\n",
               getProgramName() );
       exit( EXIT_FAILURE );
    }
    return ( ilImage *) memImage;
}

/*********
 *
 * Create a new output image file .
 *
 *********/

static ilImage *initNewOutImage( char *outFile, ilSize *outSize )
{
    ilImage *outImage = ( ilImage *) ilCreateImgFile( outFile, outSize,
                                                     ilUChar, ilInterleaved,
                                                     getCopyFileFormat(), NULL);
    if ( outImage == NULL ) {
        fprintf( stderr, "%s: Unable to create image file %s.\n",
                getProgramName(), outFile );
        exit( EXIT_FAILURE );
    }

    if ( strcmp( getCopyFileFormat(), "TIFF" ) == 0 ) {
        ilSetCompression( outImage, ilLZW );
    }
    return outImage;
}

/*********
 *
 * Copy specified audio frames from editMovie to audio file outFile.
 *
 *********/

void copyAudioFrames( MVid theTrack, int firstFrame, int numFrames )
{
    size_t       readBuffSize     = 0;
    char         outFile[ 512 ];
    void         *readBuff        = NULL;
    AFfilehandle audioFile        = NULL;
    DMparams     *params          = NULL;
    
    params = mvGetParams( theTrack );

    dmParamsSetEnum( params,
		     DM_AUDIO_FORMAT,
		     DM_AUDIO_TWOS_COMPLEMENT );
    
    readBuffSize = numFrames *
                   dmAudioFrameSize( params );

    readBuff = malloc( ( int ) readBuffSize );
    if ( readBuff == NULL ) {
        fprintf( stderr, "%s: Unable to allocate audio buffer.\n",
                getProgramName() );
        exit( EXIT_FAILURE );
    }

    if ( mvReadFrames( theTrack, firstFrame, numFrames,
                      readBuffSize, readBuff ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Could not read %d audio frames from %s.",
                getProgramName(), numFrames, getSourceMovieName() );
        exit( EXIT_FAILURE );
    }

    /*
     * Initialize a new writable audio file.
     */

    sprintf( outFile, "%s.aiffc", getCopyFileName() );
    audioFile = initAudioFile( outFile, theTrack );

    {
        int numFramesWritten;

        if ( ( numFramesWritten = AFwriteframes( audioFile, AF_DEFAULT_TRACK,
                                                readBuff, numFrames ) )
     	     < numFrames ) {
            fprintf( stderr, "%s: Only wrote %d audio frames of %d to %s.",
                    getProgramName(), numFrames, outFile );
    	    AFclosefile( audioFile );
            exit( EXIT_FAILURE );
 	}
    }
    AFclosefile( audioFile );
    free( readBuff );
}

/*********
 *
 * Initialize a new audio file, and return a handle to it.
 *
 *********/

static AFfilehandle initAudioFile( char *outFile, MVid theTrack )
{
    AFfilesetup fileSetup = AFnewfilesetup();

    AFinitfilefmt( fileSetup, AF_FILE_AIFFC );

    AFinitchannels( fileSetup, AF_DEFAULT_TRACK,
		   mvGetAudioChannels( theTrack ) );

    AFinitrate( fileSetup, AF_DEFAULT_TRACK, mvGetAudioRate( theTrack ) );

    AFinitsampfmt( fileSetup, AF_DEFAULT_TRACK, mvGetAudioFormat( theTrack ),
                  mvGetAudioWidth( theTrack ) );

    return( AFopenfile( outFile, "w", fileSetup) );

}
