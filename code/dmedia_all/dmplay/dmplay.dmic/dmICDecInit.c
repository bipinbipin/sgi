#include "dmplay.h"


/*
 * dmICDecInit.c: Open dmIC decompressor
 *
 */

void
dmICDecInit (void)
{
    DMparams *p;
    int height;
    DMbufferpool inpool, outpool;

    if (dmICCreate(UseThisdmICDec, &dmID.id) != DM_SUCCESS) {
	ERROR("Uanble to open decompressor");
    }

    /* image.height is frame height, height is frame or field height */
    dmParamsCreate(&p);
    if (DO_FRAME) {
	height = image.height;
    } else {
	height = image.height / 2;
    }

    /* set params for source of dmIC */
    dmSetImageDefaults(p, image.width, height, DM_IMAGE_PACKING_CbYCrY);
    dmParamsSetString(p, DM_IMAGE_COMPRESSION, DM_IMAGE_JPEG);
    dmParamsSetEnum(p,DM_IMAGE_ORIENTATION,DM_IMAGE_TOP_TO_BOTTOM);
    if (dmICSetSrcParams(dmID.id, p) != DM_SUCCESS) {
	ERROR("dmICSetSrcParams failed");
    }

    /* set params for destination of dmIC */
    /* same as those for the source except for DM_IMAGE_COMPRESSION */
    dmParamsSetString(p, DM_IMAGE_COMPRESSION, DM_IMAGE_UNCOMPRESSED);

    if ((image.display == GRAPHICS) && (HASIMPACTCOMP)) {
	/* output packing type and buffer size for Impact/Octane Compression */
	dmParamsSetEnum(p, DM_IMAGE_PACKING, DM_IMAGE_PACKING_RGBX);
	dmID.outbufsize = image.width*height*4;
    }
    else {
	/* use CbYCrY output packing in p param*/
	dmID.outbufsize = image.width*height*2;
    }

    if (dmICSetDstParams(dmID.id, p) != DM_SUCCESS) {
	ERROR("dmICSetDstParams failed");
    }
    dmParamsDestroy(p);

    /* Create dmbuffer pool for input to decompressor */
    dmParamsCreate(&p);
    dmID.inbufsize = image.width*height*2;

    dmBufferSetPoolDefaults(p, dmID.inbufs, dmID.inbufsize, DM_FALSE, DM_TRUE);
    if (dmICGetSrcPoolParams(dmID.id, p) != DM_SUCCESS) {
        ERROR("dmICGetSrcPoolParams failed");
    }
    dmParamsSetEnum(p, DM_POOL_SHARE, DM_POOL_INTERPROCESS);
    if (dmBufferCreatePool(p, &inpool) != DM_SUCCESS) {
        ERROR("dmBufferCreatePool failed");
    }
    dmParamsDestroy(p);
    dmID.inpool = inpool;
    dmID.inpoolfd = dmBufferGetPoolFD(inpool);
    if (dmBufferSetPoolSelectSize(inpool, dmID.inbufsize) != DM_SUCCESS) {
	ERROR("dmBufferSetPoolSelectSize failed\n");
    }
    dmID.inustmsc.msc = 0;

    /* Create dmbuffer pool and use it for output from decompressor and */
    /* input to video devices */

    dmID.outpoolfd = -1;
    dmParamsCreate(&p);
    if (dmID.outbufs <= 0) {
	dmID.outbufs = dmID.inbufs;
    }
    dmBufferSetPoolDefaults(p, dmID.outbufs, dmID.outbufsize,DM_FALSE,DM_TRUE);
    if (dmICGetDstPoolParams(dmID.id, p) != DM_SUCCESS) {
        ERROR("dmICGetSrcPoolParams failed");
    }
    if (image.display != GRAPHICS) {
	vlInit();
	if (vlDMGetParams(video.svr, video.path, video.src, p) < 0) {
	    VERROR("vlDMGetParams failed");
	}
    }
    dmParamsSetEnum(p, DM_POOL_SHARE, DM_POOL_INTERPROCESS);
    if (dmBufferCreatePool(p, &outpool) != DM_SUCCESS) {
        ERROR("dmBufferCreatePool failed");
    }
    if (dmICSetDstPool(dmID.id, outpool) != DM_SUCCESS) {
        ERROR("dmICSetDstPool failed");
    }
    dmParamsDestroy(p);
    dmID.outpool = outpool;
    dmID.outpoolfd = dmBufferGetPoolFD(outpool);
    if (dmBufferSetPoolSelectSize(outpool, dmID.outbufsize) != DM_SUCCESS) {
	ERROR("dmBufferSetPoolSelectSize failed\n");
    }

    if (options.verbose) {
	printf("input pool: %d buffers, %d bytes/buffer\n", dmID.inbufs, dmID.inbufsize);
	printf("outputpool: %d buffers, %d bytes/buffer\n", dmID.outbufs, dmID.outbufsize);
    }

} /* dmICDecInit */
