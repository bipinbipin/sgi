/*
** compdecomp
** Indigo2 IMPACT Compression example program
**
** Silicon Graphics, Inc, 1996
*/

#include "compdecomp.h"

#define COMP_RB_SIZE	    (1024 * 1024 * 1)
#define DECOMP_RB_SIZE	    (1024 * 1024 * 1)

/*
*****************************************************************************
*****************************************************************************
** open the compressor
*/
cl_comp_open()
{
    int scheme;
    int status;

    scheme = clQuerySchemeFromName( CL_ALG_VIDEO, "impact" );
    if (scheme < 0) {
	fprintf(stderr,"failed to find IMPACT compression CL codec\n");
	return -1;
    }

    status = clOpenCompressor( scheme, &global.comp.handle );
    if (status != SUCCESS) {
	fprintf(stderr,"failed to open compressor\n");
	return -1;
    }

    global.comp.video_port = clGetParam( global.comp.handle, 
		    CL_IMPACT_VIDEO_INPUT_CONTROL );
    return 0;
}


/*
*****************************************************************************
*****************************************************************************
** open the decompressor
*/
cl_decomp_open()
{
    int scheme;
    int status;

    scheme = clQuerySchemeFromName( CL_ALG_VIDEO, "impact" );
    if (scheme < 0) {
	fprintf(stderr,"failed to find IMPACT compression CL codec\n");
	return -1;
    }

    status = clOpenDecompressor( scheme, &global.decomp.handle );
    if (status != SUCCESS) {
	fprintf(stderr,"failed to open decompressor\n");
	return -1;
    }

    global.decomp.video_port = clGetParam( global.decomp.handle, 
		    CL_IMPACT_VIDEO_INPUT_CONTROL );
    return 0;
}

/*
*****************************************************************************
*****************************************************************************
** start the compressor
*/
cl_comp_init()
{
    int paramBuf[100];	
    int n = 0;
    int bitrate;
    int rc;

    paramBuf[n++] = CL_IMAGE_WIDTH;
    paramBuf[n++] = global.image.width;
    paramBuf[n++] = CL_IMAGE_HEIGHT;
    paramBuf[n++] = global.image.height;
    paramBuf[n++] = CL_ENABLE_IMAGEINFO;
    paramBuf[n++] = 1;

    bitrate = (global.image.height * global.image.width * 2 * 8 * 60) / 
	    INIT_COMP_RATIO;

    paramBuf[n++] = CL_BITRATE;
    paramBuf[n++] = bitrate;

    rc = clSetParams( global.comp.handle, paramBuf, n );
    if (rc != SUCCESS) {
	fprintf(stderr,"failed to set cl parameters on compressor\n");
	return -1;
    }

    global.comp.buffer = clCreateBuf( global.comp.handle, 
		    CL_DATA, 1, COMP_RB_SIZE, 0 );
    if (global.comp.buffer == NULL) {
	fprintf(stderr,"failed to allocate compressor's rb\n");
	return -1;
    }

    rc = clCompress( global.comp.handle, CL_CONTINUOUS_NONBLOCK,
	    CL_EXTERNAL_DEVICE, 0, NULL);
    if (rc != SUCCESS) {
	fprintf(stderr,"failed to start compressor\n");
	return -1;
    }

    return 0;
}

/*
*****************************************************************************
*****************************************************************************
** start the decompressor
*/
cl_decomp_init()
{
    int paramBuf[100];	
    int n = 0;
    int bitrate;
    int rc;

    paramBuf[n++] = CL_IMAGE_WIDTH;
    paramBuf[n++] = global.image.width;
    paramBuf[n++] = CL_IMAGE_HEIGHT;
    paramBuf[n++] = global.image.height;

    rc = clSetParams( global.decomp.handle, paramBuf, n );
    if (rc != SUCCESS) {
	fprintf(stderr,"failed to set cl parameters on decompressor\n");
	return -1;
    }

    global.decomp.buffer = clCreateBuf( global.decomp.handle,
		    CL_DATA, 1, DECOMP_RB_SIZE, 0 );
    if (global.decomp.buffer == NULL) {
	fprintf(stderr,"failed to allocate compressor's rb\n");
	return -1;
    }

    rc = clDecompress( global.decomp.handle, CL_CONTINUOUS_NONBLOCK,
	    0, NULL, CL_EXTERNAL_DEVICE);
    if (rc != SUCCESS) {
	fprintf(stderr,"failed to start decompressor\n");
	return -1;
    }

    return 0;
}
