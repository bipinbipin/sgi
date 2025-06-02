////////////////////////////////////////////////////////////////////////
//
// ImageSource.c++
//	
//	Abstract base class for reading images.  The source can be
//	an image file, a movie file, ...
//
// Copyright 1995, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//
////////////////////////////////////////////////////////////////////////

#include "ImageSource.h"

#include "ImageUtils.h"

#include <bstring.h>
#include <dmedia/moviefile.h>
#include <il/ilABGRImg.h>
#include <il/ilConfigure.h>
#include <il/ilDisplay.h>
#include <il/ilFileImg.h>
#include <il/ilRotZoomImg.h>

static void reportDMError(
    )
{
    int errorNum;
    char detail[DM_MAX_ERROR_DETAIL];
    const char* message;
    message = dmGetError( &errorNum, detail );
    assert( message != NULL );
    fprintf( stderr, "Error: %s\n", detail );
    exit( 1 );
}

static void copyBuffer(
    int  width,
    int  height,
    void *src,
    int  srcRowBytes,
    void *dst,
    int  dstRowBytes
    )
{
    int y;
    int copyRowBytes = width * sizeof(uint32_t);
    char *dp = (char *)dst;
    char *sp = (char *)src;

    assert(copyRowBytes <= dstRowBytes);
    assert(copyRowBytes <= srcRowBytes);

    for (y = 0; y < height; y++)
    {
        memcpy(dp, sp, copyRowBytes);

        dp += dstRowBytes;
        sp += srcRowBytes;
    }
}


////////////////////////////////////////////////////////////////////////
//
// ImageSourceCommon class
//
// This class has the common data members and methods for all of the
// different kinds of image sources.
//
////////////////////////////////////////////////////////////////////////

class ImageSourceCommon : public ImageSource
{
public:
    //
    // Constructor and Destructor
    //
    
    ImageSourceCommon( int width, int height, 
		       DMfxbuffer* buffer )
    {
	myWidth      = width;
	myHeight     = height;
	myBuffer     = buffer;

	DMstatus status = dmParamsCreate( &myImageFormat );
	if ( status != DM_SUCCESS ) 
	{
	    fprintf( stderr, "Out of memory.\n" );
	    assert( FALSE );
	    exit( 1 );
	}

	status = dmSetImageDefaults( myImageFormat, myWidth, myHeight,
				     DM_IMAGE_PACKING_ABGR );
	if ( status != DM_SUCCESS ) 
	{
	    fprintf( stderr, "Out of memory.\n" );
	    assert( FALSE );
	    exit( 1 );
	}
	
	status = dmParamsSetEnum( myImageFormat, DM_IMAGE_ORIENTATION,
				  DM_IMAGE_TOP_TO_BOTTOM );
	if ( status != DM_SUCCESS ) 
	{
	    fprintf( stderr, "Out of memory.\n" );
	    assert( FALSE );
	    exit( 1 );
	}
    }
    
    ~ImageSourceCommon()
    {
	dmParamsDestroy( myImageFormat );
    }
    
    //
    // ImageSource methods that this class implements:
    //
    
    int getWidth()
    {
	return myWidth;
    }
    
    int getHeight()
    {
	return myHeight;
    }
    
    DMfxbuffer* getBuffer()
    {
	return myBuffer;
    }
    
    //
    // ImageSource methods for which this class provides default
    // implementations that may be overridden:
    //

    VfxTime getDuration()
    {
	return VfxTime::fromNanoseconds( 0 );
    }
    
    Boolean setTime( VfxTime )
    {
	return FALSE;
    }
    
    void clear( int, int, int, int )
    {
	assert( FALSE );
    }
    
    void loadImage( int, int, int, void* )
    {
	assert( FALSE );
    }
    
protected:
    int		myWidth;
    int		myHeight;
    DMfxbuffer*	myBuffer;
    DMparams*	myImageFormat;
};

////////////////////////////////////////////////////////////////////////
//
// ImageFileSource implementation class
//
////////////////////////////////////////////////////////////////////////

class ImageFileSource : public ImageSourceCommon
{
public:
    //
    // Constructors and destructors.
    //

    ImageFileSource( int width, int height, DMfxbuffer* buffer ) :
	ImageSourceCommon( width, height, buffer )
    {
	myImage = NULL;
    }
    
    ~ImageFileSource()
    {
	if ( myImage != NULL )
	{
	    delete myImage;
	}
    }
    
    void getImageAtTime(
	VfxTime,
	int width,
	int height,
	int rowBytes,
	void* buffer 
	);
    
    Boolean readFile(
	Boolean	keepAspect,
	const char*	fileName 
	);
    
private:
    void*	myImage;
};

////////
//
// getImageAtTime
//
////////

void ImageFileSource::getImageAtTime(
    VfxTime,
    int width,
    int height,
    int rowBytes,
    void* buffer
    )
{
    assert( width == myWidth && height == myHeight );
    
    copyBuffer(
	width, height,
	myImage, myWidth * 4,
	buffer, rowBytes
	);
}

////////
//
// readFile
//
////////

Boolean ImageFileSource::readFile(
    Boolean	keepAspect,
    const char* fileName
    )
{
    ilFileImg*		myOrigImage = NULL;
    ilRotZoomImg*	myResizedImage = NULL;
    ilABGRImg*		myAbgrImage = NULL;

    //
    // open the file
    //
    
    myOrigImage = new ilFileImg( fileName );
    if ( myOrigImage->getStatus() != ilOKAY ) {
	delete myOrigImage;
	return FALSE;
    }

    //
    // Resize the image as needed.
    //
    
    myResizedImage = new ilRotZoomImg( myOrigImage );
    if ( myResizedImage->getStatus() != ilOKAY )
    {
	delete myResizedImage;
	delete myOrigImage;
	myOrigImage = NULL;
	return NULL;
    }

    //
    // set so that the top left corner of the image is the first byte
    // of the image buffer when we do a getTile().
    //

    setOrigin(myResizedImage, iflUpperLeftOrigin);

    myResizedImage->sizeToFit( myWidth, myHeight, keepAspect );

    //
    // create the ABGR format image.
    //
    
    myAbgrImage = new ilABGRImg( myResizedImage );
    if ( myAbgrImage->getStatus() != ilOKAY )
    {
	delete myAbgrImage;
	delete myResizedImage;
	delete myOrigImage;
	myResizedImage = NULL;
	myOrigImage = NULL;
	return NULL;
    }
    // unnecessary?   XXX
    myAbgrImage->setOrientation(iflUpperLeftOrigin);

    //
    // center the image in the buffer
    // 
    // XXX there must be a better way of doing this.
    // srcX and srcY might be negative, that way we get
    // blank space to the left/top of the image (in order to
    // center it).  ImageVision states that when you access
    // pixels "out of bounds", the fillPixel is returned.
    //

    iflSize imgSize;
    myAbgrImage->getSize( imgSize );

    int srcX = ((int)imgSize.x - (int)myWidth ) / 2;
    int srcY = ((int)imgSize.y - (int)myHeight) / 2;

    //
    // Get the image
    //

    DMstatus status;
    status = dmFXSetupOutputImageBuffer(
	bufOutputDirect,
	bufInputAll,
	myBuffer
	);
    if ( status != DM_SUCCESS )
    {
	reportDMError();
    }
    
    myImage = malloc( myWidth * myHeight * 4 );
    if ( myImage == NULL )
    {
	fprintf( stderr, "malloc failed\n" );
	exit( 1 );
    }
    
    void* myBufferData = dmFXGetDataPtr( myBuffer );
    int myBufferWidth = dmFXGetRowLength( myBuffer );
    
    myAbgrImage->getTile( srcX, srcY, myWidth, myHeight, myImage );
    
    copyBuffer(
	myWidth, myHeight,
	myImage, myWidth * 4,
	myBufferData, myBufferWidth * 4
	);

    status = dmFXCleanupOutputImageBuffer( myBuffer );
    if ( status != DM_SUCCESS )
    {
	reportDMError();
    }
    
    //
    // All done.
    //

    delete myAbgrImage;
    delete myResizedImage;
    delete myOrigImage;
    return TRUE;
}

////////////////////////////////////////////////////////////////////////
//
// MovieFileSource implementation class
//
////////////////////////////////////////////////////////////////////////

class MovieFileSource : public ImageSourceCommon
{
private:
    //
    // Object state.
    //
    
    VfxTime		myTime;

    Boolean		myMovieValid;
    MVid		myMovie;
    
    void getImage( VfxTime );
    
public:
    //
    // Constructors and destructors.
    //

    MovieFileSource( int width, int height, DMfxbuffer* buffer );
    ~MovieFileSource();
    
    Boolean openFile(
	Boolean	keepAspect,
	const char*	fileName 
	);
    
    void getImageAtTime(
	VfxTime,
	int width,
	int height,
	int rowBytes,
	void* buffer 
	);
    
    //
    // ImageSource methods.
    //

    VfxTime getDuration();
    Boolean setTime( VfxTime );
};

////////
//
// constructor
//
////////

MovieFileSource::MovieFileSource(
    int width,
    int height,
    DMfxbuffer* buffer
    ) :
    ImageSourceCommon( width, height, buffer )
{
    myTime         = VfxTime::fromNanoseconds( 0 );
    myMovieValid   = FALSE;
}

////////
//
// destructor
//
////////

MovieFileSource::~MovieFileSource(
    )
{
    if ( myMovieValid ) {
	mvClose( myMovie );
	myMovieValid = FALSE;
    }
}

////////
//
// getDuration
//
////////

VfxTime MovieFileSource::getDuration(
    )
{
    return VfxTime::fromNanoseconds(  mvGetMovieDuration( myMovie, BILLION ) );
}

////////
//
// getImage
//
////////

void MovieFileSource::getImage(
    VfxTime	time
    )
{
    DMstatus status;
    
    status = dmFXMovieRenderImage( 
	myMovie,
	time.toNanoseconds(), BILLION,
	bufInputAll,
	myBuffer
	);
    if ( status != DM_SUCCESS )
    {
	reportDMError();
    }
}

////////
//
// getImageAtTime
//
////////

void MovieFileSource::getImageAtTime(
    VfxTime time,
    int width,
    int height,
    int rowBytes,
    void* buffer
    )
{
    DMstatus status;

    assert( width == myWidth && height == myHeight );
    
    DMparams* format;
    status = dmParamsCreate( &format );
    if ( status != DM_SUCCESS )
    {
	reportDMError();
    }
    status = dmSetImageDefaults( format, width, height, DM_IMAGE_PACKING_ABGR);
    if ( status != DM_SUCCESS )
    {
	reportDMError();
    }
    status = dmParamsSetEnum( format, DM_IMAGE_ORIENTATION,
			      DM_IMAGE_TOP_TO_BOTTOM );
    if ( status != DM_SUCCESS )
    {
	reportDMError();
    }
    status = dmParamsSetInt( format, MV_IMAGE_STRIDE,
			     rowBytes/4 - myWidth );
    if ( status != DM_SUCCESS )
    {
	reportDMError();
    }
    
    status = mvRenderMovieToImageBuffer(
	myMovie,
	time.toNanoseconds(), BILLION,
	buffer, format 
	);
    if ( status != DM_SUCCESS )
    {
	reportDMError();
    }
    
    dmParamsDestroy( format );
}

////////
//
// openFile
//
////////

Boolean MovieFileSource::openFile(
    Boolean	/*keepAspect*/,
    const char* fileName
    )
{
    //
    // Open the movie file.
    //

    DMstatus status;
    status = mvOpenFile( fileName, O_RDONLY, &myMovie );
    if ( status != DM_SUCCESS ) {
	return FALSE;
    }
    myMovieValid = TRUE;

    //
    // Read the first frame
    //

    this->getImage( myTime );
    
    //
    // All done.
    //

    return TRUE;
}

////////
//
// setTime
//
////////

Boolean MovieFileSource::setTime(
    VfxTime	newTime
    )
{
    if ( newTime == myTime )
    {
	return FALSE;
    }
    else
    {
	myTime = newTime;
	this->getImage( myTime );
	return TRUE;
    }
}

////////////////////////////////////////////////////////////////////////
//
// ImageBuffer implementation class
//
////////////////////////////////////////////////////////////////////////

class ImageBuffer : public ImageSourceCommon
{
public:
    //
    // Constructors and destructors
    //
    
    ImageBuffer( int width, int height, DMfxbuffer* buffer ) :
	ImageSourceCommon( width, height, buffer )
    {
	myImage = malloc( width * height * 4 );
	assert( myImage != NULL );
    }
    
    //
    // ImageSource methods.
    //

    void clear( int r, int g, int b, int a )
    {
	uint32_t pixel = ( ( a << 24 ) | ( b << 16 ) | ( g << 8 ) | r );
	uint32_t* ptr = (uint32_t*) myImage;
	for ( int i = 0;  i < myWidth * myHeight;  i++ )
	{
	    *ptr++ = pixel;
	}
	this->copyToFxBuffer();
    }
    
    void loadImage( int width, int height, int rowBytes, void* image )
    {
	resizeImage( 
	    width,   height,   rowBytes,   image,
	    myWidth, myHeight, myWidth*4,  myImage );
	this->copyToFxBuffer();
    }
    
    void getImageAtTime(
	VfxTime,
	int width,
	int height,
	int rowBytes,
	void* buffer 
	)
    {
	assert( width == myWidth && height == myHeight );
	copyBuffer(
	    width, height,
	    myImage, myWidth * 4,
	    buffer, rowBytes 
	    );
    }
    
private:
    void* myImage;
    
    void copyToFxBuffer()
    {
	DMstatus status;
	status = dmFXSetupOutputImageBuffer(
	    bufOutputDirect, bufInputAll, myBuffer );
	if ( status != DM_SUCCESS )
	{
	    reportDMError();
	}
	copyBuffer(
	    myWidth, myHeight,
	    myImage, myWidth * 4,
	    dmFXGetDataPtr( myBuffer ),
	    dmFXGetRowLength( myBuffer ) * 4
	    );
	status = dmFXCleanupOutputImageBuffer( myBuffer );
	if ( status != DM_SUCCESS )
	{
	    reportDMError();
	}
    }
    
};

////////////////////////////////////////////////////////////////////////
//
// ImageSource 
//
////////////////////////////////////////////////////////////////////////

////////
//
// openFile
//
////////

ImageSource* ImageSource::openFile(
    int		width,
    int		height,
    DMfxbuffer* buffer,
    Boolean	keepAspect,
    const char* fileName
    )
{
    //
    // Try opening the file as an image file.
    //

    {
	ImageFileSource* source = new ImageFileSource( width, height,
						       buffer );
	if ( source->readFile( keepAspect, fileName ) ) {
	    return source;
	}
	delete source;
    }
    
    //
    // Try opening the file as a movie file.
    //

    {
	MovieFileSource* source = new MovieFileSource( width, height,
						       buffer );
	if ( source->openFile( keepAspect, fileName ) ) {
	    return source;
	}
	delete source;
    }
    
    //
    // This file could not be opened as any of the known file formats.
    //

    return NULL;
}

////////
//
// solidColor
//
////////

ImageSource* ImageSource::solidColor(
    int 		width,
    int 		height,
    DMfxbuffer*		buffer,
    unsigned char 	r,
    unsigned char 	g,
    unsigned char 	b,
    unsigned char 	a
    )
{
    ImageBuffer* imageBuffer = new ImageBuffer( width, height, buffer );
    imageBuffer->clear( r, g, b, a );
    return imageBuffer;
}

////////
//
// createBuffer
//
////////

ImageSource* ImageSource::createBuffer(
    int 		width,
    int 		height,
    DMfxbuffer*		buffer
    )
{
    return ImageSource::solidColor( width, height, buffer, 
				    80, 80, 80, 255 );
}

////////
//
// Destructor
//
////////

ImageSource::~ImageSource(
    )
{
}

