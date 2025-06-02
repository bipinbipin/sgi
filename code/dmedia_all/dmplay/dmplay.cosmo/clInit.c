/*
 * clInit.c: Does libcl (compression library) initialization
 *
 * 
 * Silicon Graphics Inc., June 1994
 */
#include "dmplay.h"

static int first = 1;

/*
 * clInit: Does cl initialization
 *
 *  mode 1 => decompressor
 *  mode 0 => compressor
 */
void
clInit (int mode)
{
    int n, paramBuf [50];
    int status;
    int externalHeight;
    int externalWidth;
    int internalHeight;

    if (mode) {  /* decompress */
	if ((status=clOpenDecompressor(codec.engine, &codec.Hdl)) != SUCCESS) {
	    if (status == CL_SCHEME_NOT_AVAILABLE)
		fprintf (stderr, "Decompressor is not installed.\n");
	    else
	    if (status == CL_SCHEME_BUSY)
		fprintf (stderr, "Decompressor is in use.\n");
	    else
		fprintf (stderr, "Unable to open decompressor - %u\n",status);
	    stopAllThreads ();
	}
    } else {    /* compress */
	if ((status=clOpenCompressor(codec.engine, &codec.Hdl)) != SUCCESS) {
	    if (status == CL_SCHEME_NOT_AVAILABLE)
		fprintf (stderr, "Compressor is not installed.\n");
	    else
	    if (status == CL_SCHEME_BUSY)
		fprintf (stderr, "Compressor is in use.\n");
	    else
		fprintf (stderr, "Unable to open compressor - %u\n",status);
	    stopAllThreads ();
	}
    }

    externalHeight = image.height;
    externalWidth  = image.width;
    internalHeight = image.height;

    if (image.interlacing != DM_IMAGE_NONINTERLACED) {
	externalHeight /= 2;
	internalHeight /= 2;
    }
    if (codec.engine!= CL_JPEG_SOFTWARE){
	internalHeight = (internalHeight+7)/8*8;
	externalHeight = (externalHeight+7)/8*8;
    }

    n = 0;
    if (mode == 1 && image.display != GRAPHICS) {
	/*
	 * Set up video scaling
	 *
	 * CL_IMAGE_WIDTH and CL_IMAGE_HEIGHT are the dimensions we want
	 * to uncompress to.
	 * CL_INTERNAL_IMAGE_WIDTH and CL_INTERNAL_IMAGE_HEIGHT are
	 * the dimensions of the image as it was originally compressed.
	 * The CL will scale the image from CL_INTERNAL_IMAGE_WIDTH/HEIGHT to
	 * CL_IMAGE_WIDTH/HEIGHT as it decompresses.  This only works
	 * for Cosmo and IMPACT Compression.
	 *
	 * note that the width 704 is the CCIR sizes minus 16 pixels
	 * to produce a width that is a multiple of 16 pixels wide.
	 */
	if ( image.width == 640/2
	        || image.width == 704/2 
		|| image.width== (((720/2)+16)/16*16)
	        || image.width == 768/2 )
	    externalWidth  *= 2;

	if ( abs(image.height-240) < 8 || abs(image.height-288) < 8)
	    externalHeight *= 2;

	paramBuf [n++] = CL_INTERNAL_IMAGE_WIDTH;
	paramBuf [n++] = image.width;
	paramBuf [n++] = CL_INTERNAL_IMAGE_HEIGHT;
	paramBuf [n++] = internalHeight;
    }

    /* Impact supports scaling down */
    if (mode == 1 && image.display == GRAPHICS && 
       !strcmp(options.image_engine, "impact") && options.zoom < 1) {
	externalHeight *= options.zoom;
	externalWidth *= options.zoom;

	/* make sure width is multiple of 2 RGB pixels */
	if (externalWidth % 2) externalWidth++;

        image.mem_width = externalWidth;
        image.mem_height = externalHeight * 2;	/* Hardware interleave */
	
	if (first) {
	    image.cliptop *= options.zoom;
	    image.clipbottom *= options.zoom;
	}

	paramBuf [n++] = CL_INTERNAL_IMAGE_WIDTH;
	paramBuf [n++] = image.width;
	paramBuf [n++] = CL_INTERNAL_IMAGE_HEIGHT;
	paramBuf [n++] = internalHeight;
    }

    paramBuf [n++] = CL_IMAGE_WIDTH;
    paramBuf [n++] = externalWidth;
    paramBuf [n++] = CL_IMAGE_HEIGHT;
    paramBuf [n++] = externalHeight;
    image.externalWidth = externalWidth;
    image.externalHeight = externalHeight;



    if (image.display == GRAPHICS) {
	paramBuf [n++] = CL_ORIGINAL_FORMAT;
	paramBuf [n++] = CL_RGBX;
    }

    if (codec.engine == CL_JPEG_COSMO) {
	paramBuf [n++] = CL_ENABLE_IMAGEINFO;
	paramBuf [n++] = 1;
        if (image.orientation != DM_TOP_TO_BOTTOM) {
            fprintf(stderr, 
     "Cosmo can only process images with ``top to bottom'' orientation.\n");
	    stopAllThreads ();
        }
    }

    paramBuf [n++] = CL_ORIENTATION;
    paramBuf [n++] = CL_TOP_DOWN;

    /* Impactcomp does hardware interleaving */
    if (codec.hardwareInterleave) {
	codec.codecPort = clGetParam(codec.Hdl, CL_IMPACT_VIDEO_INPUT_CONTROL);
        if (image.interlacing != DM_IMAGE_NONINTERLACED && 
	    image.display == GRAPHICS) {
    	     paramBuf [n++] = CL_IMPACT_FRAME_INTERLEAVE;
    	     paramBuf [n++] = 1;

	     paramBuf [n++] = CL_IMPACT_INTERLEAVE_MODE;
	     if (image.interlacing == DM_IMAGE_INTERLACED_EVEN) {
		paramBuf [n++] = CL_IMPACT_INTERLEAVE_EVEN;
	     }
	     else {
		paramBuf [n++] = CL_IMPACT_INTERLEAVE_ODD;
	     }
	}
    }

    /* 
     * For decompression to graphics on impact, advise it that we won't 
     * be accessing the (uncompressed) data. It uses this advice to avoid
     * flushing the cache on the uncompressed ring buffer before starting
     * each dma, thus saving considerable cpu overhead.
     */
    if (mode == 1 && image.display == GRAPHICS && 
       !strcmp(options.image_engine, "impact")) {
	 paramBuf[n++] = CL_IMPACT_BUFFER_ADVISE;
	 paramBuf[n++] = CL_BUFFER_ADVISE_NOACCESS;
    }

    if (clSetParams(codec.Hdl, paramBuf, n) != SUCCESS) {
	fprintf (stderr, "Error Setting Parameters.\tExiting\n");
	stopAllThreads ();
    }
    first = 0;
}
