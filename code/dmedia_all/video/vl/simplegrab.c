/*
 * File:          simplegrab.c
 *
 * Usage:         simplegrab 
 *
 * Description:   simplegrab grabs a video frame to memory and the screen 
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
 *                vlPerror()
 */
#include <stdlib.h>
#include <stdio.h>
#include <gl/gl.h>
#include <dmedia/vl.h>

char *_progName;

/* Report errors */
void
error_exit(void)
{
    vlPerror(_progName);
    exit(1);
}

void
main(int argc, char **argv)
{
    VLServer svr;
    VLPath path;
    VLNode src, drn;
    VLControlValue val;
    VLBuffer buffer;
    VLInfoPtr info;
    char *dataPtr;
    int c;
    int xsize;
    int ysize;
    long win;
    
    _progName = argv[0];
    
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
    
    /* Get the video size */
    vlGetControl(svr, path, drn, VL_SIZE, &val);
    xsize = val.xyVal.x;
    ysize = val.xyVal.y;
    
    /* Set up and open a GL window to display the data */
    prefsize(xsize,ysize);
    win = winopen("Simplegrab Window");
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

    /* Wait for a frame */
    do {
    	info = vlGetNextValid(svr, buffer);
    } while (!info);

    /* Get a pointer to the frame */
    dataPtr = vlGetActiveRegion(svr, buffer, info);
	    
    /* Write the data to the screen */
    lrectwrite(0,0, xsize-1, ysize-1, (ulong *)dataPtr);

    /* Finished with frame, unlock the buffer */
    vlPutFree(svr, buffer);

    /* End the data transfer */
    vlEndTransfer(svr, path);
     
    /* Wait until the user presses a key */
    printf("Press <Enter> to exit: ");
    c = getc(stdin);
	    
    /* Cleanup before exiting */
    vlDeregisterBuffer(svr, path, drn, buffer);
    vlDestroyBuffer(svr, buffer);
    vlDestroyPath(svr, path);
    vlCloseVideo(svr);
}
