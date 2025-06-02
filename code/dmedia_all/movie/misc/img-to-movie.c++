/******************************************************************************
 *
 * File:         img-to-movie.c++
 *
 * Usage         img-to-movie <imagefile> <newmoviefile>
 *
 * Description:  Program to convert a sequenced image file to a movie.
 *
 * Functions:    The following SGI Movie Library functions are used:
 *
 *               mvSetMovieDefaults()
 *               mvCreateFile()
 *               mvGetErrorStr()
 *               mvGetErrno()
 *               mvClose()
 *               mvGetParams()
 *               mvAddTrack()
 *               mvFindTrackByMedium()
 *               mvInsertFrames()
 *
 *               The following SGI Digital Media Library functions are used:
 *
 *               dmParamsCreate()
 *               dmParamsDestroy()
 *               dmSetImageDefaults()
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

/*
 * Include the declarations for the movie library so we can create a
 * movie.
 */

#include <dmedia/moviefile.h>

/*
 * Include the declarations we need from ImageVision so that we can 
 * read an image file.
 */

#include <il2.5/ilImage.h>
#include <il2.5/ilGenericImgFile.h>
#include <il2.5/ilABGRImg.h>

/*
 * Forward declarations of functions that appear below.
 */
    
static DMstatus SetUpImageTrack( MVid movie, int width, int height );
static DMstatus CopyImages     ( ilImage* image, MVid movie );

/*
 * Global program name variable used for error messages.
 */

static char* programName;

/**********************************************************************
 *
 * main
 *
 **********************************************************************/

main( int argc, char** argv )
{
    /*
     * Check usage.
     */
    
    programName = argv[0];
    
    if ( argc != 3 ) {
	fprintf( stderr, "Usage: %s imagefile newmoviefile.\n", programName );
	exit( EXIT_FAILURE );
    }

    /*
     * Open the image file.
     */
    
    ilImage* image = ilOpenImgFile( argv[1], "r" );
    if ( ( image == NULL ) || ( image->getStatus() != ilOKAY ) ) {
	fprintf( stderr, "%s: Unable to open image file - %s.\n",
                programName, argv[1] );
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
     * Create the movie file.
     */
    
    DMparams* movieParams;
    
    if ( dmParamsCreate( &movieParams ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to create default params - %s.\n",
                programName, argv[2] );
        delete( image );
        if ( convertedImage != NULL )
            delete( convertedImage );
	exit( EXIT_FAILURE );
    }
    
    if ( mvSetMovieDefaults( movieParams, MV_FORMAT_SGI_3 ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to set default params - %s.\n",
                programName, argv[2] );
        dmParamsDestroy( movieParams );
        delete( image );
        if ( convertedImage != NULL )
            delete( convertedImage );
	exit( EXIT_FAILURE );
    }

    MVid movie;
    if ( mvCreateFile( argv[2], movieParams, NULL, &movie ) == DM_FAILURE ) {
	fprintf( stderr, "%s: Unable to create movie file %s: error =  %s.\n",
		programName, argv[2], mvGetErrorStr( mvGetErrno() ) );
        dmParamsDestroy( movieParams );
        delete( image );
        if ( convertedImage != NULL )
            delete( convertedImage );
	exit( EXIT_FAILURE );
    }

    dmParamsDestroy( movieParams );

    /*
     * Set up the image track and then copy the frames from the image
     * to the movie.
     */
    
    ilSize size;
    useImage->getSize( size );
    if ( SetUpImageTrack( movie, size.x, size.y ) != DM_SUCCESS ) {
        delete( image );
        if ( convertedImage != NULL )
            delete( convertedImage );
	exit( EXIT_FAILURE );
    }

    CopyImages( useImage, movie );
    
    /*
     * All done!  Close the image and the movie.
     */
    
    mvClose( movie );
    delete( image );
    if ( convertedImage != NULL )
        delete( convertedImage );
    exit( EXIT_SUCCESS );
}

/*********
 *
 * SetUpImageTrack
 *
 *********/

static DMstatus SetUpImageTrack( MVid movie, int width, int height )
{
    /*
     * Set up the parameter list for the image track. The properties that 
     * must always be set for an image track are width and height. If the
     * function dmSetImageDefaults() is used, then other properties, such
     * as compression,  require setting only if they differ from the
     * defaults.
     */

    DMparams *imageTrackParams;
    if ( dmParamsCreate( &imageTrackParams ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to create image track params.\n",
                programName );
        return DM_FAILURE;
    }

    if ( dmSetImageDefaults( imageTrackParams, width, height, 
                            DM_PACKING_RGBX ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Unable to set image defaults.\n", programName ); 
        dmParamsDestroy( imageTrackParams );
        return DM_FAILURE;
    }

    /*
     * NOTE: The following is equivalent to the single dmSetImageDefaults()
     *       call above. 
     * 
     * dmParamsSetInt( imageTrackParams, DM_IMAGE_WIDTH, width );
     * dmParamsSetInt( imageTrackParams, DM_IMAGE_HEIGHT, height );
     * dmParamsSetFloat( imageTrackParams, DM_IMAGE_RATE, 15.0 );
     * dmParamsSetEnum( imageTrackParams, DM_IMAGE_INTERLACING, 
                       DM_IMAGE_NONINTERLEAVED );
     * dmParamsSetEnum( imageTrackParams, DM_IMAGE_PACKING, DM_PACKING_RGBX );
     * dmParamsSetEnum( imageTrackParams, DM_IMAGE_ORIENTATION,
                       DM_BOTTOM_TO_TOP );
     */

    dmParamsSetString( imageTrackParams, DM_IMAGE_COMPRESSION, DM_IMAGE_MVC1 );

    /*
     * Add an image track to the movie.
     */

    MVid imageTrack;
    if ( mvAddTrack( movie, DM_IMAGE, imageTrackParams, 
                    NULL, &imageTrack ) == DM_FAILURE) {
        fprintf( stderr, "%s: Unable to add image track to movie.\n",
                programName);
        dmParamsDestroy( imageTrackParams );
        return DM_FAILURE;
    }
    dmParamsDestroy( imageTrackParams );

    return DM_SUCCESS;
}

/*********
 *
 * CopyImages
 *
 *********/

static DMstatus CopyImages( ilImage* image, MVid movie )
{
    /*
     * Locate the image track of the movie.
     */
    
    MVid imageTrack;
    if ( mvFindTrackByMedium( movie, DM_IMAGE, &imageTrack ) != DM_SUCCESS ) {
	fprintf( stderr, "%s: Unable to find image track.\n", programName );
        return DM_FAILURE;
    }

    /*
     * Allocate a buffer big enough to hold the image.
     */

    size_t imageSize = dmImageFrameSize( mvGetParams( imageTrack ) );

    void* imageBuff = malloc( ( int ) imageSize );
    if ( imageBuff == NULL ) {
	fprintf( stderr, "%s: Unable to allocate buffer.\n", programName );
        return DM_FAILURE;
    }
    
    /*
     * Make an ilConfig to tell the image what kind of data to give us.
     * We want 4 channels, interleaved, as unsigned chars.
     */

    ilConfig imageConfig( ilUChar, ilInterleaved, 4, NULL ); 

    /*
     * Loop through the Z axis to pick up all of the images in the
     * file.  For each one, copy to the buffer and then put it into the
     * movie. 
     */ 

    ilSize size = image->getSize();
    
    for ( int z = 0;  z < size.z;  z++ ) {

	image->getTile3D( 0, 0, z, 		/* Origin in image file. */
			size.x, size.y, 1,      /* Size of tile. */
			imageBuff,              /* Destination buffer */
			&imageConfig );         /* What kind of data to get */

	if ( mvInsertFrames( imageTrack, z, 1, 
                            imageSize, imageBuff ) != DM_SUCCESS ) {
	    fprintf( stderr, "%s: Unable to write image to movie.\n",
                    programName);
            free( imageBuff );
            return DM_FAILURE;
	}
    }
    
    free( imageBuff );
    return DM_SUCCESS;
}
