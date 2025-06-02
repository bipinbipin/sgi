/*==================A Color Transforming Capture Application==========
 *
 *
 * File:          colorcapture.c
 *
 * Description:   colorcapture captures a stream of video to memory
 *		  and displays it with user-definable contrast, brightness,
 *		  saturation, and/or hue changes.
 *
 * Functions:     IRIS Video Library functions used
 *
 *                vlOpenVideo()
 *                vlGetNode()
 *                vlCreatePath()
 *                vlSetupPaths()
 *                vlSetControl()
 *		  vlCreateBuffer()
 *                vlRegisterBuffer()
 *		  vlGetActiveRegion()
 *		  vlGetNextValid()
 *		  vlPutFree()
 *                vlBeginTransfer()
 *                vlEndTransfer()
 *                vlDeregisterBuffer()
 *                vlDestroyPath()
 *		  vlDestroyBuffer()
 *                vlCloseVideo()
 *
 * Functions:     Color Space Converter functions used
 *
 *                dmColorCreate
 *                dmColorDestroy
 *                dmColorConvert
 *                dmColorSetSrcParams
 *                dmColorSetDstParams
 *		  dmColorSetContrast
 *		  dmColorSetBrightness
 *		  dmColorSetSaturation
 *		  dmColorSetHue
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <gl/gl.h>
#include <dmedia/vl.h>
#include <dmedia/dm_color.h>		/* Color converter header file */
#include <dmedia/dm_image.h>		/* Image header file           */
#include <sys/poll.h>
#include <signal.h>

#define SIZE_FULL	1
#define SIZE_HALF	2
#define SIZE_QUARTER	3

/* Defaults */
int   iSize       = SIZE_HALF;           /* Half size image      */
float fContrast   = 1.0f,                /* Normal contrast      */
      fBrightness = 0.0f,                /* No brightness offset */
      fSaturation = 1.0f,                /* Normal saturation    */
      fHue        = 0.0f;                /* No hue rotation      */
int   bVFlip      =   0,                 /* No vertical   flip   */
      bHFlip      =   0;                 /* No horizontal flip   */

int   interrupted =   0;

/* Report errors */
void
error_exit(void)
{
  exit(1);
}


void
usage (void)
{
  printf ("Usage: colorcapture -h [-c contrast] [-b brightness] [-s saturation] [-r hue] [-quarter|-half|-full] -vflip -hflip\n");
}


int
initargs(int argc, char **argv)
{
    --argc;
    ++argv;

    while (argc) {
	if (!strcmp("-h", *argv)) {
	    usage();
	    exit(0);
	} else if (!strcmp("-vflip",   *argv)) {
	    bVFlip = 1;
	} else if (!strcmp("-hflip",   *argv)) {
	    bHFlip = 1;
	} else if (!strcmp("-full",    *argv)) {
	    iSize = SIZE_FULL;
	} else if (!strcmp("-half",    *argv)) {
	    iSize = SIZE_HALF;
	} else if (!strcmp("-quarter", *argv)) {
	    iSize = SIZE_QUARTER;
	} else if (!strcmp("-c", *argv)) {
	    --argc;
	    ++argv;
	    fContrast = atof (*argv);
	} else if (!strcmp("-b", *argv)) {
	    --argc;
	    ++argv;
	    fBrightness = atof (*argv);
	} else if (!strcmp("-s", *argv)) {
	    --argc;
	    ++argv;
	    fSaturation = atof (*argv);
	} else if (!strcmp("-r", *argv)) {
	    --argc;
	    ++argv;
	    fHue = atof (*argv);
	} else {
	    usage ();
	    return 1;
	}

	argc--;
	argv++;
    }
  return 0;
}



/*ARGSUSED*/
void 
ctlcinterrupt(int signal_no)
{
	interrupted=1;
}


int
main(int argc, char **argv)
{
    VLServer svr;
    VLPath path;
    VLNode src, drn;
    VLControlValue val;
    VLBuffer buffer;
    VLInfoPtr info;
    char *dataPtr;
    int xsize;
    int ysize;
    DMcolorconverter converter;		/* color converter handle */
    DMparams *srcparams, *dstparams;	/* image parameters */
    struct pollfd poll_fd[1];
    
    if (initargs(argc, argv))
	return 1;

    foreground();
        
    /* Connect to the daemon */
    if (!(svr = vlOpenVideo(""))) 
	error_exit();

    /* Set up a drain node in memory */
    drn = vlGetNode(svr, VL_DRN, VL_MEM, VL_ANY);
    
    /* Set up a source node on any video source  */
    src = vlGetNode(svr, VL_SRC, VL_VIDEO, VL_ANY);

    /* Create a path using the first device that will support it */
    path = vlCreatePath(svr, VL_ANY, src, drn); 

    /* Set up the hardware for and define the usage of the path */
    if ((vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE)) < 0)
	error_exit();

    /* Set the packing to RGB */
    val.intVal = VL_PACKING_RGB_8;
    vlSetControl(svr, path, drn, VL_PACKING, &val);
    
    /* Let's zoom the window */
    switch (iSize)
      {
        case SIZE_FULL:
          val.fractVal.numerator   = 1;
          val.fractVal.denominator = 1;
	  break;
        case SIZE_HALF:
          val.fractVal.numerator   = 1;
          val.fractVal.denominator = 2;
	  break;
        case SIZE_QUARTER:
          val.fractVal.numerator   = 1;
          val.fractVal.denominator = 4;
	  break;
	default:
	  error_exit();
      }

    if (vlSetControl(svr, path, drn, VL_ZOOM, &val))
	error_exit();

    /* Get the video size */
    vlGetControl(svr, path, drn, VL_SIZE, &val);
    xsize = val.xyVal.x;
    ysize = val.xyVal.y;
    
    /* Set up and open a GL window to display the data */
    prefsize(xsize,ysize);
    winopen("ColorCapture Window");
    RGBmode();
    pixmode(PM_TTOB, 1);
    gconfig();
    
    /* Create and register a buffer for 1 frame */
    buffer = vlCreateBuffer(svr, path, drn, 1);
    if (buffer == NULL)
	error_exit();	
    vlRegisterBuffer(svr, path, drn, buffer);
    
    /* Begin the data transfer */
    if (vlBeginTransfer(svr, path, 0, NULL))
	error_exit();

     poll_fd[0].fd=vlBufferGetFd(buffer);
     poll_fd[0].events=POLLIN;

    /* Create the color converter context */
    dmColorCreate (&converter);

    /* Create source and destination parameters for the images */
    dmParamsCreate (&srcparams);
    dmParamsCreate (&dstparams);

    /* Set source image parameters for XBGR packing, 8 bit components,
     * height, width, and orientation.
     */
    dmParamsSetEnum (srcparams, DM_IMAGE_PACKING,     DM_IMAGE_PACKING_XBGR);
    dmParamsSetEnum (srcparams, DM_IMAGE_DATATYPE,    DM_IMAGE_DATATYPE_CHAR);
    dmParamsSetEnum (srcparams, DM_IMAGE_ORIENTATION, DM_IMAGE_TOP_TO_BOTTOM);
    dmParamsSetEnum (srcparams, DM_IMAGE_MIRROR,      DM_IMAGE_LEFT_TO_RIGHT);
    dmParamsSetInt  (srcparams, DM_IMAGE_HEIGHT,      ysize);
    dmParamsSetInt  (srcparams, DM_IMAGE_WIDTH,       xsize);

    /* Set destination image parameters to the same
     */
    dmParamsSetEnum (dstparams, DM_IMAGE_PACKING,     DM_IMAGE_PACKING_XBGR);
    dmParamsSetEnum (dstparams, DM_IMAGE_DATATYPE,    DM_IMAGE_DATATYPE_CHAR);
    dmParamsSetEnum (dstparams, DM_IMAGE_ORIENTATION, DM_IMAGE_TOP_TO_BOTTOM);
    dmParamsSetEnum (dstparams, DM_IMAGE_MIRROR,      DM_IMAGE_LEFT_TO_RIGHT);
    dmParamsSetInt  (dstparams, DM_IMAGE_HEIGHT,      ysize);
    dmParamsSetInt  (dstparams, DM_IMAGE_WIDTH,       xsize);

    /* User requested vertical flip?   */
    if (bVFlip)                                          
      dmParamsSetEnum (dstparams, DM_IMAGE_ORIENTATION,
                                  DM_IMAGE_BOTTOM_TO_TOP);

    /* User requested horizontal flip?   */
    if (bHFlip)                                          
      dmParamsSetEnum (dstparams, DM_IMAGE_MIRROR,
                                  DM_IMAGE_RIGHT_TO_LEFT);
    
    /* Apply the source and destination image parameters to the 
     * color converter.
     */
    dmColorSetSrcParams (converter, srcparams);
    dmColorSetDstParams (converter, dstparams);

    /* The source and destination image parameters are
     * no longer required.
     */
    dmParamsDestroy (srcparams);
    dmParamsDestroy (dstparams);

    /* Set the contrast, brightness, saturation, and hue.
     */
    dmColorSetContrast   (converter, fContrast);
    dmColorSetBrightness (converter, fBrightness);
    dmColorSetSaturation (converter, fSaturation);
    dmColorSetHue        (converter, fHue);

    printf("Type <control-c> to exit.\n");
    signal(SIGINT, ctlcinterrupt);
    
    while(!interrupted) {
	do {
	    info = vlGetNextValid(svr, buffer);
	    if (!info){
		poll(poll_fd,1,-1);
	    }
	    if (interrupted) break;
	} while (!info);
        if (interrupted) break;
    
	/* Get a pointer to the frame */
	dataPtr = vlGetActiveRegion(svr, buffer, info);

	/* Apply the color conversion */
	dmColorConvert (converter, dataPtr, dataPtr);
		
	/* Write the data to the screen */
	lrectwrite(0,0, xsize-1, ysize-1, (ulong *)dataPtr);
    
	/* Finished with frame, unlock the buffer */
	vlPutFree(svr, buffer);
    }

    /* Destroy the color transform */
    dmColorDestroy (converter);
    
    /* End the data transfer */
    vlEndTransfer(svr, path);
     
    /* Cleanup before exiting */
    vlDeregisterBuffer(svr, path, drn, buffer);
    vlDestroyBuffer(svr, buffer);
    vlDestroyPath(svr, path);
    vlCloseVideo(svr);
    return 0;
}
