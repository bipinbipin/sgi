/*==================A Color Posterizing Application==========
 *
 *
 * File:          colorposterize.c
 *
 * Description:   colorposterize captures a stream of video to memory
 *		  and displays it with user-definable color levels.
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
int   iSize       = SIZE_HALF;           /* Half size image         */
int   iLevels     = 256;                 /* Levels of posterization */

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
  printf ("Usage: colorposterize [-h] [-l levels] [-quarter|-half|-full]\n");
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
	} else if (!strcmp("-full",    *argv)) {
	    iSize = SIZE_FULL;
	} else if (!strcmp("-half",    *argv)) {
	    iSize = SIZE_HALF;
	} else if (!strcmp("-quarter", *argv)) {
	    iSize = SIZE_QUARTER;
	} else if (!strcmp("-l", *argv)) {
	    --argc;
	    ++argv;
	    iLevels = atoi (*argv);
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
/*
*  convert1 converts the full range down to the number of required
*  levels; i.e. from 0-255 to 0-9 (assuming levels = 10).  convert2
*  performs the reverse operation.
*/
    DMcolorconverter converter1,
                     converter2;
    DMparams        *srcParams,               /* Source      image parameters */
                    *dstParams,               /* Destination image parameters */
                    *compParams;              /* Component parameters         */

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
    winopen("ColorPosterize Window");
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

    /* Create the color converters' contexts */
    if ((dmColorCreate (&converter1)  == DM_FAILURE) ||
        (dmColorCreate (&converter2)  == DM_FAILURE))
      error_exit();

    /* Create source and destination parameters for the images */
    if ((dmParamsCreate (&srcParams)  == DM_FAILURE) ||
        (dmParamsCreate (&dstParams)  == DM_FAILURE) ||
        (dmParamsCreate (&compParams) == DM_FAILURE))
      error_exit();

    /* Set source image parameters for XBGR packing, 8 bit components,
     * height, width, and orientation.
     */
    dmParamsSetEnum   (srcParams, DM_IMAGE_PACKING,     DM_IMAGE_PACKING_XBGR);
    dmParamsSetEnum   (srcParams, DM_IMAGE_DATATYPE,    DM_IMAGE_DATATYPE_CHAR);
    dmParamsSetEnum   (srcParams, DM_IMAGE_ORIENTATION, DM_IMAGE_TOP_TO_BOTTOM);
    dmParamsSetEnum   (srcParams, DM_IMAGE_MIRROR,      DM_IMAGE_LEFT_TO_RIGHT);
    dmParamsSetInt    (srcParams, DM_IMAGE_HEIGHT,      ysize);
    dmParamsSetInt    (srcParams, DM_IMAGE_WIDTH,       xsize);

    /* Set destination image parameters to the same
     */
    dmParamsSetEnum   (dstParams, DM_IMAGE_PACKING,     DM_IMAGE_PACKING_XBGR);
    dmParamsSetEnum   (dstParams, DM_IMAGE_DATATYPE,    DM_IMAGE_DATATYPE_CHAR);
    dmParamsSetEnum   (dstParams, DM_IMAGE_ORIENTATION, DM_IMAGE_TOP_TO_BOTTOM);
    dmParamsSetEnum   (dstParams, DM_IMAGE_MIRROR,      DM_IMAGE_LEFT_TO_RIGHT);
    dmParamsSetInt    (dstParams, DM_IMAGE_HEIGHT,      ysize);
    dmParamsSetInt    (dstParams, DM_IMAGE_WIDTH,       xsize);

    dmParamsSetFloat  (compParams, DM_IMAGE_MIN,           0);
    dmParamsSetFloat  (compParams, DM_IMAGE_MAX,   iLevels-1);
    dmParamsSetFloat  (compParams, DM_IMAGE_BIAS,          0);
    dmParamsSetFloat  (compParams, DM_IMAGE_SCALE, iLevels-1);
    dmParamsSetParams (dstParams,  DM_IMAGE_COMPONENT_ALL,
                                   compParams);


    /* Apply the source and destination image parameters to the 
     * color converters.  Note that the DMparams are switched so
     * that the process is reversed.
     */
    dmColorSetSrcParams (converter1, srcParams);
    dmColorSetDstParams (converter1, dstParams);
    dmColorSetSrcParams (converter2, dstParams);
    dmColorSetDstParams (converter2, srcParams);

    /* The source, destination, and components parameters are
     * no longer required.
     */
    dmParamsDestroy (srcParams);
    dmParamsDestroy (dstParams);
    dmParamsDestroy (compParams);

    dmColorSetContrast (converter1, 1.0);     /* Workaround for bug in 6.2 */
    dmColorSetContrast (converter2, 1.0);     /* Workaround for bug in 6.2 */

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

	/* Apply the color conversions */
	dmColorConvert (converter1, dataPtr, dataPtr);
	dmColorConvert (converter2, dataPtr, dataPtr);
		
	/* Write the data to the screen */
	lrectwrite(0,0, xsize-1, ysize-1, (ulong *)dataPtr);
    
	/* Finished with frame, unlock the buffer */
	vlPutFree(svr, buffer);
    }

    /* Destroy the color transform */
    dmColorDestroy (converter1);
    dmColorDestroy (converter2);
    
    /* End the data transfer */
    vlEndTransfer(svr, path);
     
    /* Cleanup before exiting */
    vlDeregisterBuffer(svr, path, drn, buffer);
    vlDestroyBuffer(svr, buffer);
    vlDestroyPath(svr, path);
    vlCloseVideo(svr);
    return 0;
}
