/*
** handle the compression library initialization
**
** things to do:
**	1) open the compressor
**	2) create the ring buffers
**	3) setup parameters for various attributes of the source
*/

#include <unistd.h>
#include "blendicomp.h"

#define COMP_RB_SIZE	    (1024 * 1024 * 2)

clInit()
{
    int n, paramBuf[50];		/* cl parameters and how many */
    int status;
    int h, w;
    int scheme;

    n = 0;
    paramBuf[ n++ ] = CL_IMAGE_WIDTH;
    paramBuf[ n++ ] = image.source_width;

    paramBuf[ n++ ] = CL_IMAGE_HEIGHT;
    paramBuf[ n++ ] = image.source_height;

    paramBuf[ n++ ] = CL_STREAM_HEADERS;
    paramBuf[ n++ ] = 1;

    paramBuf[ n++ ] = CL_ORIENTATION;
    paramBuf[ n++ ] = CL_TOP_DOWN;

    scheme = clQuerySchemeFromName( CL_ALG_VIDEO, "impact" );
    if (scheme < 0) {
	fprintf(stderr,"unable to find cl scheme for compressor >%s<\n",
	    "impact");
	exit(1);
    }

    status = clOpenDecompressor( scheme, &codec.hdl );
    if (status != SUCCESS) {
	fprintf(stderr,"failed to open decompression codec\n");
	return -1;
    }

    if (clSetParams( codec.hdl, paramBuf, n ) != SUCCESS) {
	fprintf(stderr,"failed to set parameters on codec\n");
	return -1;
    }

    codec.vlcodec = clGetParam(codec.hdl, CL_IMPACT_VIDEO_INPUT_CONTROL);
    if (codec.vlcodec < 0) {
	fprintf(stderr,"failed to get the vl codec cookie\n");
	return -1;
    }

    codec.compRB = clCreateBuf( codec.hdl, CL_DATA, COMP_RB_SIZE, 1, NULL );
    if (codec.compRB == NULL) {
	fprintf(stderr,"failed to create compressed ring buffer\n");
	return -1;
    }

    return 0;
}

clStartDecompressor()
{
    /*
     * Start the compressor and tell it where to get data and where to
     * put data.  The uncompressed data buffer passed in is NULL,
     * which means to use the buffer created above.  The same is true
     * for the uncompressed frame buffer for displaying to graphics;
     * for displaying to video, we tell the decompressor to use its
     * direct connection to the video device.
     */

    if ( clDecompress( codec.hdl,
		CL_CONTINUOUS_NONBLOCK,	    /* run asynchronously */
		0,			    /* forever */
		NULL,			    /* compressed source = implicit */
		CL_EXTERNAL_DEVICE	    /* out to video */
				    ) != SUCCESS ) {
	fprintf(stderr,"clDecompress failed ... \n");
    }

    return 0;
}
