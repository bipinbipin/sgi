/*================An Event Driven Application==========
 *
 *
 * File:          eventex.c
 *
 * Usage:         eventex 
 *
 * Description:   event demonstrates VL eventloop with the IRIS GL
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
 *                vlRegisterHandler()
 *                vlAddCallback()
 *                vlSelectEvents()
 *		  vlMainLoop()
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
#include <gl/device.h>
#include <dmedia/vl.h>

/*
 *  Function Prototypes
 */
void error_exit(void);
void ProcessEvent(VLServer svr, VLEvent *ev, void *data);
void ProcessGlEvent(int fd, void *win);
void exit_capture(void);

/*
 * Global Variables
 */
char *_progName;
VLBuffer buffer;
VLServer svr;
VLPath path;
VLNode src, drn;
int xsize;
int ysize;

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
    VLControlValue val;
    int c;
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
    win = winopen("Eventex Window");
    RGBmode();
    pixmode(PM_TTOB, 1);
    gconfig();
    
    /* 
     * Allow these key presses, mouseclicks, etc to be 
     * entered in the event queue 
     */
    qdevice(ESCKEY);
    qdevice(WINSHUT);
    qdevice(WINQUIT);
    
    
    /* Create and register a buffer for 1 frame */
    buffer = vlCreateBuffer(svr, path, drn, 1);
    if (buffer == NULL)
	error_exit();	
    vlRegisterBuffer(svr, path, drn, buffer);
    
    /* Begin the data transfer */
    if (vlBeginTransfer(svr, path, 0, NULL))
	error_exit();
    
    /* 
     * Specify what path-related events we want to receive.
     * In this example we only want transfer complete events.
     */
    vlSelectEvents(svr, path, VLTransferCompleteMask);
    
    /* Set ProcessEvent() as the callback for a transfer complete event */
    vlAddCallback(svr, path, VLTransferCompleteMask, ProcessEvent, NULL);
    
    /* Set ProcessGlEvent() as the GL event handler */
    vlRegisterHandler(svr, qgetfd(), (VLEventHandler)ProcessGlEvent,
		      (VLPendingFunc) qtest, (void *)win);
		  
    /* Loop and dispatch events */
    vlMainLoop();    
}


/* Handle video library events */
void
ProcessEvent(VLServer svr, VLEvent *ev, void *data)
{
    VLInfoPtr info;
    char *dataPtr;

    switch (ev->reason)
    {
	case VLTransferComplete:
	    info = vlGetNextValid(svr, buffer);
	    if(!info)
		break;
	
	    /* Get a pointer to the frame */
	    dataPtr = vlGetActiveRegion(svr, buffer, info);
		    
	    /* Write the data to the screen */
	    lrectwrite(0,0, xsize-1, ysize-1, (ulong *)dataPtr);
	
	    /* Finished with frame, unlock the buffer */
	    vlPutFree(svr, buffer);
      	break;
    
	default:
	    printf("Got Event %d\n", ev->reason);
	break;
    }
}

/* Handle graphics library events */
void
ProcessGlEvent(int fd, void *win)
{
    static short val;

    switch (qread(&val))
    {
	/* Quit */
	case ESCKEY:
	    if (val == 1) /* Respond to keydowns only */
		exit_capture();
	break;
		
	case WINSHUT:
	case WINQUIT:
	    exit_capture();
	break;
	
	default:
	break;
    }
}

void
exit_capture()
{
    /* End the data transfer */
    vlEndTransfer(svr, path);
    
    /* Disassociate the ring buffer from the path */
    vlDeregisterBuffer(svr, path, drn, buffer);
    
    /* Destroy the path, free the memory it used */
    vlDestroyPath(svr,path);	
    
    /* Destroy the ring buffer, free the memory it used */
    vlDestroyBuffer(svr, buffer);
    
    /* Disconnect from the daemon */
    vlCloseVideo(svr);
    
    exit(0);
}
