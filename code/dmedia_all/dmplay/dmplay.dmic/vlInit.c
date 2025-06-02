#include "dmplay.h"


/*
 * vlInit
 */
void vlInit
    (
    void
    )
{
    int timing;
    VLControlValue val;

    if (image.display == GRAPHICS)
        return;

/* >>> YDL TEMPORARY FIX */
    if ((video.svr = vlOpenVideo("")) == NULL) {
	VERROR("vlOpenVideo failed");
    }
/* <<< YDL TEMPORARY FIX */

    /* set up memory node as the source */
    if ((video.src = vlGetNode(video.svr, VL_SRC, VL_MEM, VL_ANY)) == NULL) {
        VERROR ("vlGetNode failed");
    }
    /* set up video node as the drain */
    if ((video.drn = vlGetNode(video.svr, VL_DRN, VL_VIDEO, VL_ANY)) == NULL) {
        VERROR ("vlGetNode failed");
    }

    /* In Impact Compression card, we set up the mem to video path when 
     * the display option is not graphics. If the same system has Impact 
     * Video card, the screen node will be added to the path as another drain 
     * node. It will generate the same display behavior as dmplay.cosmo.
     */
    if (HASIMPACTCOMP && !options.videoOnly) {
   	/* set up screen node as the drain */
	if ((video.scrdrn = vlGetNode(video.svr, VL_DRN, VL_SCREEN, VL_ANY))
		 == NULL) {
            VERROR ("vlGetNode failed");
        }
	if ((video.path = vlCreatePath(video.svr, VL_ANY, video.src, 
		video.scrdrn)) != -1) {
	    /* add video output to the path */
	    (void) vlAddNode(video.svr, video.path, video.drn);
	} else {
	    /* create mem->vidout only */
	    video.scrdrn = -1;
	    if ((video.path = vlCreatePath(video.svr, VL_ANY, video.src,
		video.drn)) == -1) {
		VERROR("vlCreatePath failed");
	    }
	}
    }
    else {
	video.scrdrn = -1;
    	if ((video.path = vlCreatePath(video.svr, VL_ANY, video.src, video.drn))
		 == -1) {
            VERROR("vlCreatePath failed");
    	}
    }

    if (vlSetupPaths(video.svr, (VLPathList)&video.path, 1, VL_SHARE, 
		VL_SHARE) == -1) {
        VERROR("vlSetupPath failed");
    }

    if (image.frameRate == 25.0) {
	if (image.pixelAspect == 1.0) {
	    timing = VL_TIMING_625_SQ_PIX;
	} else {
	    timing = VL_TIMING_625_CCIR601;
	}
    } else {
	if (image.pixelAspect == 1.0) {
	    timing = VL_TIMING_525_SQ_PIX;
	} else {
	    timing = VL_TIMING_525_CCIR601;
	}
    }
    val.intVal = timing;
    vlSetControl(video.svr, video.path, video.drn, VL_TIMING, &val);

    vlGetControl(video.svr, video.path, video.drn, VL_TIMING, &val);
    if (val.intVal != timing) {
	    if ((timing == VL_TIMING_525_CCIR601) || (timing == VL_TIMING_625_CCIR601)) {
		fprintf(stderr, "CCIR 601 timing is not available\n");
	    } else {
		fprintf(stderr, "Square pixel timing is not available\n");
	    }
	    exit(1);
    }

    val.intVal = VL_PACKING_YVYU_422_8;
    if (vlSetControl(video.svr, video.path, video.src, VL_PACKING, &val) == -1) {
	VERROR("vlSetControl failed");
    }
    val.intVal = VL_CAPTURE_INTERLEAVED;
    if (vlSetControl(video.svr, video.path, video.src, VL_CAP_TYPE, &val) == -1) {
	VERROR("vlSetControl failed");
    }
    val.xyVal.x = image.width;
    val.xyVal.y = image.height;
    if (vlSetControl(video.svr, video.path, video.src, VL_SIZE, &val) == -1) {
	VERROR("vlSetControl failed");
    }
    if (HASIMPACTCOMP && video.scrdrn != -1) {
	if (vlSetControl(video.svr, video.path, video.scrdrn, VL_SIZE, &val)
				== -1) {
            VERROR("vlSetControl failed");
    	}
    }

    if (DO_FIELD) {
	val.intVal = VL_CAPTURE_NONINTERLEAVED;
	if (vlSetControl(video.svr, video.path, video.src, VL_CAP_TYPE, &val) == -1) {
	    VERROR("vlSetControl failed");
	}
    }

    /* Set up zooming factor */
	/* normalize the factor to the form of 1/n or n/1 */
/*
    if (options.zoom != 1) {
	n = options.zoom * 256;
	d = 256;
	while (d > 1 && n > 1) {
	    if (n >= (d << 1)) {
		n >>= 1;
		d >>= 1;
	    } else if (d >= (n << 1)) {
		n >>= 1;
		d >>= 1;
	    } else 
		break;
	}
	val.fractVal.numerator = n;
	val.fractVal.denominator = d;
	vlSetControl(video.svr, video.path, video.drn, VL_ZOOM, &val);
	vlGetControl(video.svr, video.path, video.drn, VL_ZOOM, &val);
	options.zoom = (float)val.fractVal.numerator/val.fractVal.denominator;
    }

    vlGetControl(video.svr, video.path, video.drn, VL_SIZE, &val);
    val.xyVal.y  = val.xyVal.y - (image.cliptop+image.clipbottom);
    val.xyVal.x  = val.xyVal.x - (image.clipleft+image.clipright);

    if (options.zoom > 1) {
	val.xyVal.x *= options.zoom;
	val.xyVal.y *= options.zoom;
    }
    video.height = val.xyVal.y;
    video.width = val.xyVal.x;
*/

} /* vlInit */ 

/*
 * Used for Impact Compression cards when the display option is not
 * graphics. If the system also has Impact Video, we connect the video
 * screen drain node with the X window created in x_open() routine defined
 * in xInit.c. 
 */
void
vl_start(void)
{
    VLControlValue val;

    val.intVal = video.windowId;
    if (video.scrdrn != -1) {
        /* Associate the video screen node with the window */
        vlSetControl(video.svr, video.path, video.scrdrn, VL_WINDOW, &val);
    }

    if (vlBeginTransfer(video.svr, video.path, 0, NULL)) {
        vlPerror("screenPath couldn't transfer");
    }
}
