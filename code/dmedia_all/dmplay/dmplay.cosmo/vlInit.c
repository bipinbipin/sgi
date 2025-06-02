/*
 * vlInit.c: Does libvl (video library) initialization
 *
 * 
 * Silicon Graphics Inc., June 1994
 */
#include "dmplay.h"
#ifdef	COSMO
#include <dmedia/vl_ev1.h>
#endif	/* COSMO */

static VLDevice *vlConnect(char *);
static void vlCreateMainPath(VLDevice *);
static void vlStartTimingPath(VLDevice *);
static void vlSetVideoSync(VLDevice *);

static VLDevList devList;

/*
 * vlInit: Does initial vl settings.
 */
void vlInitEv1()
{
    VLDevice *devPtr;

    if (image.display == GRAPHICS)
	return;

    /*
     * Connect to vl server and find our video device
     */
    devPtr = vlConnect("ev1");

    if (devPtr == NULL) {
	fprintf(stderr, "IndyVideo/Galileo not found\n");
	exit (1);
    }

    /*
     * Cosmo doesn't work well playing back in slave mode, so change
     * it to internal if set to slave.  If it is in genlock, leave it alone.
     */
    vlSetVideoSync(devPtr);

    /*
     * Cosmo needs to get video timing (black burst) from the video device
     * during playback.
     * We setup a path for just this and start it right away.  Later, when
     * we are actually ready to send video data back from Cosmo to video,
     * we will start using a path that is a superset of this one which
     * will also include the path from cosmo back to video.
     */
    vlStartTimingPath(devPtr);

    /*
     * This is the path we will use when everything is going.  We get it
     * all ready here, but we don't actually start it up yet since Cosmo
     * doesn't have good frames to display yet.
     */
    vlCreateMainPath(devPtr);
}

static VLDevice *
vlConnect(char *devname)
{
    VLDevice *devPtr;
    int n;

    if (!(video.svr = vlOpenVideo("")) ) {
        vlPerror("Unable to open video");
        exit(1);
    }
    
    vlGetDeviceList(video.svr, &devList);
    
    /*
     * Search for IndyVideo/Galileo/Indigo2Video
     *
     */
    for (n = 0; n < devList.numDevices; n++) {
	devPtr = &(devList.devices[n]);
        if (strcmp (devPtr->name, devname) == 0)
	    break;
        else devPtr = NULL;
    }
    
    return devPtr;
}

/*
 * To change the video board timing, we need to create a special
 * path with a ``VL_DEVICE'' node on it.
 */
static void
vlSetVideoSync(VLDevice *devPtr)
{
    VLNode devNode;
    VLControlValue val;
    int want601, want525;
    int timing;

    devNode = vlGetNode(video.svr, VL_DEVICE, 0, VL_ANY);

    video.path = vlCreatePath(video.svr, devPtr->dev, devNode, devNode);

    if (video.path == -1)
	vlPerror("Bad path");

    vlSetupPaths(video.svr, (VLPathList)&video.path, 1, VL_SHARE, VL_READ_ONLY);

    vlGetControl(video.svr, video.path, devNode, VL_SYNC, &val);

#ifdef	COSMO
    if (strcmp(devPtr->name, "impact") != 0 && 
        val.intVal == VL_EV1_SYNC_SLAVE) {

	val.intVal = VL_SYNC_INTERNAL;
	if (vlSetControl(video.svr, video.path, devNode, VL_SYNC, &val))
	    vlPerror("Warning: unable to set timing mode");
    }
#endif	/* COSMO */

    /*
     * Try to figure out non-square vs. square from the width
     */
    if (image.width == 720 || image.width == 704/2
	/*rounded down*/ || image.width== 
	((720/2)+16)/16*16) /*rounded up*/
	want601 = 1;
    else
	want601 = 0;

    /*
     * Try to figure out PAL/NTSC from height
     */
    if (image.height <= 496/2)
	want525 = 1;
    else
    if (image.height <= 576/2)
	want525 = 0;
    else
    if (image.height <= 496)
	want525 = 1;
    else
	want525 = 0;

    if (want601) {
	if (want525)
	    timing = VL_TIMING_525_CCIR601;
	else
	    timing = VL_TIMING_625_CCIR601;
    } else {
	if (want525)
	    timing = VL_TIMING_525_SQ_PIX;
	else
	    timing = VL_TIMING_625_SQ_PIX;
    }

    vlGetControl(video.svr, video.path, devNode, VL_TIMING, &val);
    if (val.intVal != timing) {
	val.intVal = timing;
	if (vlSetControl(video.svr, video.path, devNode, VL_TIMING, &val))
	    vlPerror("Warning: unable to set video timing");
	vlGetControl(video.svr, video.path, devNode, VL_TIMING, &val);
	if (val.intVal != timing) {
	    if (want601)
		fprintf(stderr, "dmplay: CCIR 601 timing is not available");
	    else
		fprintf(stderr, "dmplay: Square pixel timing is not available");
	    exit(1);
	}
    }
    vlDestroyPath(video.svr, video.path);
}

/*
 * setup the timing path
 * (IndyVideo/Galileo,digital port 2 -> Cosmo)
 */
static void
vlStartTimingPath(VLDevice *devPtr)
{
    VLNode timingSrc;		/* Timing path */
    VLNode timingDrn;

    timingSrc = vlGetNode(video.svr, VL_SRC, VL_SCREEN, VL_ANY);
    timingDrn = vlGetNode(video.svr, VL_DRN, VL_VIDEO, 2);

    video.timingPath = vlCreatePath(video.svr,devPtr->dev,timingSrc,timingDrn); 

    if (video.timingPath == -1) {
	vlPerror("vlCreatePath failed for timing path");
	exit(1);
    }

    if (vlSetupPaths(video.svr, (VLPathList)&video.timingPath, 1, 
						VL_SHARE, VL_SHARE) < 0) {
	vlPerror ("Unable to start video timing");
	exit (1);
    }

    if (vlBeginTransfer(video.svr, video.timingPath, 0, NULL)) {
	vlPerror ("Unable to vlBeginTransfer video timing");
	exit (1);
    }

    video.timingActive = 1;
}

/*
 * Cosmo -> IndyVideo/Galileo --> screen, video
 */
static void
vlCreateMainPath(VLDevice *devPtr)
{
    VLNode src;			/* from Cosmo */
    VLNode dataTiming;		/* timing portion of data path */
    VLControlValue val;

    src              = vlGetNode(video.svr, VL_SRC, VL_VIDEO, 1);
    video.drn        = vlGetNode(video.svr, VL_DRN, VL_SCREEN, VL_ANY);
    video.voutdrn    = vlGetNode(video.svr, VL_DRN, VL_VIDEO, 0);
    dataTiming       = vlGetNode(video.svr, VL_DRN, VL_VIDEO, 2);
    
    video.path = vlCreatePath(video.svr, devPtr->dev, src, video.drn); 
    if (video.path == -1) {
	vlPerror("vlCreatePath failed for data path");
	vlDestroyPath(video.svr, video.timingPath);
	exit(1);
    }

    if (vlAddNode(video.svr, video.path, video.voutdrn)) {
	vlPerror("voutdrn Node Error");
    }

    if (vlAddNode(video.svr, video.path, dataTiming)) {
	vlPerror("dataTiming Node Error");
    }

    if (vlSetupPaths(video.svr, (VLPathList)&video.path, 1,
						 VL_SHARE, VL_SHARE) < 0) {
	vlPerror ("Unable to setup data path");
	vlDestroyPath(video.svr, video.timingPath);
	exit(1);
    }
#if 1
    {
    int externalHeight=image.height;
    int externalWidth=image.width;
    if (image.interlacing != DM_IMAGE_NONINTERLACED) {
        externalHeight /= 2;
    }
    externalHeight = (externalHeight+7)/8*8;
    if ( image.width == 640/2 || image.width == 704/2 || image.width == 768/2 
	||  image.width ==(((720/2)+16)/16*16))
            externalWidth  *= 2;
    if ( abs(image.height-240) < 8 || abs(image.height-288) < 8)
            externalHeight *= 2;

    val.xyVal.y  = externalHeight*2;
    val.xyVal.x  = externalWidth;

    vlSetControl(video.svr, video.path, video.drn, VL_SIZE, &val);
    }
#endif

    /*
     * Set up zoom factor
     */
    if (options.zoom != 1) {
	/*
	 * IndyVideo/Galileo likes to have simple zoom arguments, so
	 * normalize what we have to 1/n or n/1.
	 */
	int n, d;
	n = options.zoom * 256;
	d = 256;
	while (d > 1 && n > 1)
	    if (n >= (d << 1)) {
		n >>= 1;
		d >>= 1;
	    } else
	    if (d >= (n << 1)) {
		n >>= 1;
		d >>= 1;
	    } else
		break;
        val.fractVal.numerator = n;
        val.fractVal.denominator = d;
        vlSetControl(video.svr, video.path, video.drn, VL_ZOOM, &val);
        vlGetControl(video.svr, video.path, video.drn, VL_ZOOM, &val);
	options.zoom = (float)val.fractVal.numerator/val.fractVal.denominator;
    }

    /*
     * Cosmo captured frames contain three extra lines at the top
     * and bottom of an image which are typically clipped out by the
     * bezel of a TV set. These lines are often distorted or contain
     * garbage, so we clip them out here.
     *
     * To clip the video, we need to first shrink it, then change
     * the origin.
     */
    vlGetControl(video.svr, video.path, video.drn, VL_SIZE, &val);
    val.xyVal.y  = val.xyVal.y - (image.cliptop+image.clipbottom);
    val.xyVal.x  = val.xyVal.x - (image.clipleft+image.clipright);

    /*
     * If we are decimating, then IndyVideo/Galileo will shrink our
     * video output accordingly, but if we zoom up, we need to do it
     * ourselves.
     */
    if (options.zoom > 1) {
	    val.xyVal.y  *= options.zoom;
	    val.xyVal.x  *= options.zoom;
    }

    /*
     * Remember the size we set the screen to so we can use it later
     * when we create the window.
     */
    video.height = val.xyVal.y;
    video.width  = val.xyVal.x;

    vlSetControl(video.svr, video.path, video.drn, VL_SIZE, &val);

    vlGetControl(video.svr, video.path, video.drn, VL_OFFSET, &val);
    val.xyVal.y -= image.cliptop  * options.zoom;
    val.xyVal.x -= image.clipleft * options.zoom;
    vlSetControl(video.svr, video.path, video.drn, VL_OFFSET, &val);
}

static VLDevice *devPtr = NULL;

void
vlSetupVideoImpact(void)
{
    VLNode src;			/* from Cosmo */
    VLControlValue val;

    /*
     * If not connected, connect to vl server and find our video device.
     */
    if (!devPtr)
    	devPtr = vlConnect("impact");

    /* Set the timing on the video so the screen node will be the right size 
     * and the video out drain will have the right timing for the video 
     * standard implied by the movie image size.
     */
    vlSetVideoSync(devPtr);

    if (devPtr == NULL) {
	fprintf(stderr, "impact not found\n");
	stopAllThreads();
    }

    src              = vlGetNode(video.svr, VL_SRC, VL_CODEC, codec.codecPort);
    video.drn        = vlGetNode(video.svr, VL_DRN, VL_SCREEN, VL_ANY);
    video.voutdrn    = vlGetNode(video.svr, VL_DRN, VL_VIDEO, VL_ANY);
    
    video.path = vlCreatePath(video.svr, devPtr->dev, src, video.drn); 

    if (video.path != -1) {
    	/* Setup to video out - try to add screen out node */
        (void) vlAddNode(video.svr, video.path, video.voutdrn);
    } else {
    	/* Failed to setup to video out, try going only to screen out */
	video.drn = -1;
        video.path = vlCreatePath(video.svr, devPtr->dev, src, video.voutdrn); 
        if (video.path == -1) {
            vlPerror("vlCreatePath failed for data path");
            stopAllThreads();
	}
    }

    if (vlSetupPaths(video.svr, (VLPathList)&video.path, 1,
						 VL_SHARE, VL_SHARE) < 0) {
	vlPerror ("Unable to setup data path");
	stopAllThreads();
    }

    val.xyVal.y  = image.externalHeight*2;
    val.xyVal.x  = image.externalWidth;

    if (video.drn != -1)
   	 vlSetControl(video.svr, video.path, video.drn, VL_SIZE, &val);
    /*
     * Set up zoom factor
     */
    if (options.zoom != 1) {
	/*
	 * IndyVideo/Galileo1.5 likes to have simple zoom arguments, so
	 * normalize what we have to 1/n or n/1.
	 */
	int n, d;
	n = options.zoom * 256;
	d = 256;
	while (d > 1 && n > 1)
	    if (n >= (d << 1)) {
		n >>= 1;
		d >>= 1;
	    } else
	    if (d >= (n << 1)) {
		n >>= 1;
		d >>= 1;
	    } else
		break;
        val.fractVal.numerator = n;
        val.fractVal.denominator = d;
        vlSetControl(video.svr, video.path, video.drn, VL_ZOOM, &val);
        vlGetControl(video.svr, video.path, video.drn, VL_ZOOM, &val);
	options.zoom = (float)val.fractVal.numerator/val.fractVal.denominator;
    }

   
    /* Tell the impact codec node the size of a field. */
    val.xyVal.y  = image.externalHeight;
    val.xyVal.x  = image.externalWidth;
    vlSetControl(video.svr, video.path, src, VL_SIZE, &val);

    /*
     * Cosmo captured frames contain three extra lines at the top
     * and bottom of an image which are typically clipped out by the
     * bezel of a TV set. These lines are often distorted or contain
     * garbage, so we clip them out here.
     *
     * To clip the video, we need to first shrink it, then change
     * the origin.
     */
    vlGetControl(video.svr, video.path, video.drn, VL_SIZE, &val);
    val.xyVal.y  = val.xyVal.y - (image.cliptop+image.clipbottom);
    val.xyVal.x  = val.xyVal.x - (image.clipleft+image.clipright);

    /*
     * If we are decimating, then IndyVideo/Galileo will shrink our
     * video output accordingly, but if we zoom up, we need to do it
     * ourselves.
     */
    if (options.zoom > 1) {
	    val.xyVal.y  *= options.zoom;
	    val.xyVal.x  *= options.zoom;
    }

    /*
     * Remember the size we set the screen to so we can use it later
     * when we create the window.
     */
    video.height = val.xyVal.y;
    video.width  = val.xyVal.x;

    vlSetControl(video.svr, video.path, video.drn, VL_SIZE, &val);

    vlGetControl(video.svr, video.path, video.drn, VL_OFFSET, &val);
    val.xyVal.y -= image.cliptop  * options.zoom;
    val.xyVal.x -= image.clipleft * options.zoom;
    vlSetControl(video.svr, video.path, video.drn, VL_OFFSET, &val);
}

void
vlStartVideoImpact(void)
{
    VLControlValue val;

    val.intVal = video.windowId;
    if (video.drn != -1) {
	/* Associate the video screen node with the window */
        vlSetControl(video.svr, video.path, video.drn, VL_WINDOW, &val);
    }
    
    if (vlBeginTransfer(video.svr, video.path, 0, NULL)) {
    	vlPerror("screenPath couldn't transfer");
		stopAllThreads ();
    }
}

void
vlFreezeVideoImpact(void)
{
    VLControlValue val;

    val.boolVal = TRUE;
    vlSetControl(video.svr, video.path, video.drn, VL_FREEZE, &val);
    vlSetControl(video.svr, video.path, video.voutdrn, VL_FREEZE, &val);
}

void
vlUnFreezeVideoImpact(void)
{
    VLControlValue val;

    val.boolVal = FALSE;
    vlSetControl(video.svr, video.path, video.drn, VL_FREEZE, &val);
    vlSetControl(video.svr, video.path, video.voutdrn, VL_FREEZE, &val);
}


void
vlTakeDownVideoImpact(void)
{
    if (vlDestroyPath(video.svr, video.path) == -1) {
        vlPerror("couldn't destroy the  path");
	stopAllThreads ();
    }
}
