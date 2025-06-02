/******************************************************************************
 *
 * File:         createmovieResize.c++
 *
 * Description:  Filtering operations on image frames
 *
 *****************************************************************************/

#include <il2.5/ilImage.h>
#include <il2.5/ilMemoryImg.h>
#include <il2.5/ilRotZoomImg.h>
#include <il2.5/ilTypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "createmovieArgs.h"
#include "createmovieResize.h"

/*
 * Forward declarations for local functions.
 */

float computeZoom( int  oldWidth, int  oldHeight,
                   int  newWidth, int  newHeight,
                   int& xStart,   int& yStart );

inline float fmin( float a, float b )
{
    if ( a < b )   return a;
    else           return b;
}

/********
 *
 * resizeImageFrame
 *
 ********/

void resizeImageFrame( int oldWidth, int oldHeight, void* oldBuffer,
		         int newWidth, int newHeight, void* newBuffer )
{
    if ( ( newWidth < 0 ) || ( newHeight < 0 ) ) {
	fprintf( stderr, "%s: Can't make image dimensions < 0\n",
		getProgramName() );
	exit( EXIT_FAILURE );
    }
    
    ilSize oldSize( oldWidth, oldHeight, 1, 4 );
    ilSize newSize( newWidth, newHeight, 1, 4 );

    ilMemoryImg oldImage( oldBuffer, oldSize, ilUChar, ilInterleaved );
    ilMemoryImg newImage( newBuffer, newSize, ilUChar, ilInterleaved );

    if ( ( oldImage.getStatus() != ilOKAY ) || 
	 ( newImage.getStatus() != ilOKAY )) {
	fprintf( stderr, "%s: ImageVision Library error - ",
                getProgramName() );
	fprintf( stderr, "ilMemoryImg failed.\n");
	exit( EXIT_FAILURE );
    }
    
    int xStart;
    int yStart;
    float sizeFactor = computeZoom( oldWidth, oldHeight,
				    newWidth, newHeight,
				    xStart,   yStart );
    
    ilRotZoomImg resized( &oldImage,    // image to transform
			  0.0,         // rotation angle
			  sizeFactor,  // X zoom
			  sizeFactor,  // Y zoom
			  ilNoFlip,   
			  ilBiLinear );
    if ( resized.getStatus() != ilOKAY ) {
	fprintf( stderr, "%s: ImageVision Library error - ",
                getProgramName() );
	fprintf( stderr, "ilRotZoomImg failed.\n");
	exit( EXIT_FAILURE );
    }
    
    memset( newBuffer, 0, (int) newSize );

    newImage.copyTile( xStart, yStart, newWidth, newHeight, &resized, 0, 0 );
    if ( newImage.getStatus() != ilOKAY ) {
	fprintf( stderr, "%s: ImageVision Library error - ",
                getProgramName() );
	fprintf( stderr, "Couldn't copy image.\n");
	exit( EXIT_FAILURE );
    }
}

/********
 *
 * computeZoom
 *
 ********/

float computeZoom( int  oldWidth, int  oldHeight,
		   int  newWidth, int  newHeight,
		   int& xStart,   int& yStart )
{
    float zoom;
    
    float zoomX = (float)newWidth  / oldWidth;
    float zoomY = (float)newHeight / oldHeight;
    
    zoom = fmin( zoomX, zoomY );

    // These should be rounded rather than truncated !!
    
    xStart = int( ( newWidth  - zoom * oldWidth  ) / 2.0 );
    yStart = int( ( newHeight - zoom * oldHeight ) / 2.0 );
	
    return zoom;
}

