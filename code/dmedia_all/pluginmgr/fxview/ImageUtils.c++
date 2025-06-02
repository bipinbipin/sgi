////////////////////////////////////////////////////////////////////////
//
// ImageUtils.c++
//	
//	Assorted image processing functions.
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

#include "ImageUtils.h"


#include <bstring.h>	// bzero()
#include <il/ilMemoryImg.h>
#include <il/ilRotZoomImg.h>

////////
//
// resizeImage
//
////////

void resizeImage(
    int		srcWidth,
    int		srcHeight,
    int		srcRowBytes,
    void*	srcBuffer,
    int		dstWidth,
    int		dstHeight,
    int		dstRowBytes,
    void*	dstBuffer
    )
{
#ifdef IL_WORKS

    //
    // Clear the buffer in case something goes wrong.
    //
    
    bzero( dstBuffer, dstRowBytes * dstHeight );

    //
    // Make the IL objects for resizing.
    //

    iflSize srcSize( srcWidth, srcHeight, 1 );
    ilMemoryImg* src = 
	new ilMemoryImg( srcBuffer, srcSize, iflUChar, iflInterleaved );
    if ( src == NULL ) {
	return;
    }
    src->setColorModel( iflABGR );
    
    ilRotZoomImg* dst = new ilRotZoomImg( src );
    if ( dst == NULL ) {
	return;
    }
    dst->sizeToFit( dstWidth, dstHeight, TRUE );

    //
    // Center the image in the final buffer
    //

    iflSize dstSize;
    dst->getSize( dstSize );
    int x = ((int)dstSize.x - (int)dstWidth ) / 2;
    int y = ((int)dstSize.y - (int)dstHeight) / 2;
    
    //
    // Get the resized image.
    //

    dst->getTile( x, y, dstWidth, dstHeight, dstBuffer );

    //
    // Clean up.
    //

    delete dst;
    delete src;
    
#else // ! IL_WORKS

    //
    // This is a simple nearest-pixel resizer.  It uses Bresenham's
    // algorithm across each line to avoid a multiply/divide in the
    // inner loop.
    //

    uint32_t* dst = (uint32_t*) dstBuffer;
    int       srcRowPixels = srcRowBytes / 4;
    int       dstBump = dstRowBytes / 4 - dstWidth;

    for ( int y = 0;  y < dstHeight;  y++ )
    {
	uint32_t* src = 
	    ( (uint32_t*)srcBuffer ) + 
	    ( srcRowPixels * ( y * srcHeight / dstHeight ) );
	int f = 0;
	for ( int x = 0;  x < dstWidth;  x++ )
	{
	    *dst++ = *src;
	    f += srcWidth;
	    while ( dstWidth <= f )
	    {
		f -= dstWidth;
		src++;
	    }
	}
	dst += dstBump;
    }
    
#endif // IL_WORKS
}

////////
//
// setOrigin
//
// Sets up the coordinate space of the passed in image, and returns
// whether or not x and y coordinates need to be swapped to achieve the
// desired coordinate space.
//
////////

int setOrigin(
    ilRotZoomImg *img,
    iflOrientation newSpace)
{
    //
    // set so that the top left corner of the image is the first byte
    // of the image buffer when we do a getTile().
    //

    iflFlip flip = iflNoFlip;
    int transXY = 0;

    iflOrientation space = img->mapFlipTrans(img->getOrientation(),
      flip, transXY, newSpace);

    img->setOrientation(newSpace);

    //
    // the setOrientation seems to correctly take care of flips.
    // XXX
    // img->setFlip(flip);
    
    return transXY;
}
