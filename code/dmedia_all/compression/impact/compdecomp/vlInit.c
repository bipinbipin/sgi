/*
** compdecomp
** Indigo2 IMPACT Compression example program
**
** Silicon Graphics, Inc, 1996
*/

#include "compdecomp.h"

vl_capture_init()
{
    int rc;
    VLControlValue  val;

    if (global.capture.svr == 0)
	global.capture.svr = global.playback.svr = vlOpenVideo("");

    if (global.capture.svr == NULL) {
	vlPerror("vlOpenVideo");
	return -1;
    }

    global.capture.src = vlGetNode( global.capture.svr, 
		    VL_SRC, VL_VIDEO, VL_ANY);
    global.capture.drn = vlGetNode( global.capture.svr, 
		VL_DRN, VL_CODEC, global.comp.video_port );

    global.capture.path = vlCreatePath( global.capture.svr,
	    VL_ANY, global.capture.src, global.capture.drn );
    if (global.capture.path < 0) {
	vlPerror("create capture path");
	return -1;
    }

    rc = vlSetupPaths( global.capture.svr, &(global.capture.path), 1,
		VL_SHARE, VL_SHARE );
    if (rc) {
	vlPerror("setup capture path");
	return -1;
    }

    rc = vlGetControl( global.capture.svr, global.capture.path,
		global.capture.src, VL_SIZE, &val );
    if (rc) {
	vlPerror("query of current video image size");
	return -1;
    }

    /* pad up to a valid jpeg size */
    global.image.width = ((val.xyVal.x + 15) / 16) * 16;
    global.image.height = (((val.xyVal.y + 15) / 16) * 16);/* fields */

    global.image.height /= 2;

    val.xyVal.x = global.image.width;
    val.xyVal.y = global.image.height;


    rc = vlSetControl( global.capture.svr, global.capture.path,
		global.capture.drn, VL_SIZE, &val );
    if (rc) {
	vlPerror("set of jpeg padded video image size");
	return -1;
    }

    rc = vlBeginTransfer( global.capture.svr,
		    global.capture.path, 0, NULL);
    if (rc) {
	vlPerror("begin transfer, capture path");
	return -1;
    }

    return 0;
}



vl_play_init()
{
    int rc;
    VLControlValue  val;

    if (global.playback.svr == 0)
	global.capture.svr = global.playback.svr = vlOpenVideo("");

    if (global.playback.svr == NULL) {
	fprintf(stderr,"programming error, vl_playback_init\n");
	return -1;
    }

    global.playback.src = vlGetNode( global.playback.svr, 
		VL_SRC, VL_CODEC, global.decomp.video_port );
    global.playback.drn = vlGetNode( global.playback.svr, 
		    VL_DRN, VL_VIDEO, VL_ANY);

    global.playback.path = vlCreatePath( global.playback.svr,
	    VL_ANY, global.playback.src, global.playback.drn );
    if (global.playback.path < 0) {
	vlPerror("create playback path");
	return -1;
    }

    rc = vlSetupPaths( global.playback.svr, &(global.playback.path), 1,
		VL_SHARE, VL_SHARE );
    if (rc) {
	vlPerror("setup playback path");
	return -1;
    }

    val.xyVal.x = global.image.width;
    val.xyVal.y = global.image.height;
    rc = vlSetControl( global.playback.svr, global.playback.path,
		global.playback.src, VL_SIZE, &val );
    if (rc) {
	vlPerror("play: jpeg padded video image size");
	return -1;
    }

    rc = vlBeginTransfer( global.playback.svr,
		    global.playback.path, 0, NULL);
    if (rc) {
	vlPerror("begin transfer, playback path");
	return -1;
    }

    return 0;
}
