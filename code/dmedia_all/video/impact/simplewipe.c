/*
 * File: 	simplewipe.c
 *
 * Usage:	simplewipe.c 	[-f] [-d] [-n devicenum] [-t title] 
 *				[-v videosource] [-I] [-h]
 *			  	 
 *
 * Description:	Simplewipe is an example of spatially blending video with
 *		graphics over time. It does not do any special setup, but 
 *		it does show the output in a graphics window and on video out.
 *		With your mouse positioned in the graphics window containing
 *		your blended video, the "w" key executes a rectangular zoom
 *		wipe. This application only runs on video hardware that has
 *		a video output port. This program is device specific and
 *		will only run on the Galileo or IndyVideo board.
 *
 * Functions:	SGI Video Library functions used (see other modules)
 *
 *		vlOpenVideo()
 *		vlGetDeviceList()
 *		vlGetNode()
 *		vlCreatePath()
 *		vlAddNode()
 *		vlGetDevice()
 *		vlSetupPaths()
 *		vlSelectEvents()
 *		vlRegisterHandler()
 *		vlAddCallback()
 *		vlSetControl()
 *		vlGetControl()
 *		vlMainLoop()
 *		vlBeginTransfer()
 *		vlEndTransfer()
 *		vlDestroyPath()
 *		vlCloseVideo()
 *		vlGetErrno()
 *		vlPerror()
 *		vlStrError()
 *
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/MwmUtil.h>

#include <dmedia/vl.h>
#include <dmedia/vl_mgv.h>

#include <gl/gl.h>
#include <gl/device.h>

/*
 *  Function prototypes
 */

void	VideoTracking(Window, int,int);
void	processXEvent(uint, void *);
void	processVLEvent(VLServer, VLEvent *, void *);
void	docleanup(void);
void    usage(void);
void	dowipe(void);
void  	doswitchloop(int min, int max, int speed, int axis);

VLServer	vlSvr = NULL;
VLPath		vlPath = -1;
VLDevList	devlist;
VLNode		src_vid;
VLNode		src_scr;
VLNode		drn_scr;
VLNode		drn_vid;
VLNode		blend_node;	/* 'cause blending controls are here */

Display		*dpy;
Window		drnwin;
int		vin = VL_ANY;
char		*_progname;
char		*deviceName;
int		devicenum = -1;
VLDev		deviceId;
int		debug = 0;

Atom WM_DELETE_WINDOW;
Atom WM_PROTOCOLS;

int lastw = 0, lasth = 0;

#define Debug	if (debug) printf
#define fDebug	if (debug) fprintf

#define ESC_KEY 0x1b

#define USAGE \
"%s: [-f] [-t <window title>] [-d] [-I] [-v <videonode> ] [-h]\n\
\t-f\tdisable forking\n\
\t-t\tset window title\n\
\t-d\tdebug mode\n\
\t-I\tprint node and path IDs for command line interface users\n\
\t-v\tvideo source node number\n\
\t-h\tthis help message\n"

void 
usage(void)
{
    fprintf(stderr, USAGE, _progname);
}

/* Constrain the window's aspect ratio */
void
constrainWindowResize(Display *dpy, Window win, int max_w, int max_h)
{
    XSizeHints xsh;

    xsh.min_aspect.x = xsh.max_aspect.x = 80;
    xsh.min_aspect.y = xsh.max_aspect.y = 60;
    xsh.min_width  = max_w;
    xsh.min_height = max_h;
    xsh.max_width  = max_w;
    xsh.max_height = max_h;
    xsh.flags = PAspect|PMinSize|PMaxSize;

    XSetWMNormalHints(dpy, win, &xsh);
    XResizeWindow(dpy, win, max_w, max_h);
}

void
doswitchloop(min,max,speed,axis)
int min;  /* start point of loop */
int max;  /* end point of loop   */
int speed;/* increment for loop   */
int axis; /* axis allows for setting VL_MGV_WIPE_POSN_PERP */
{
    VLControlValue vlctlval;
    int i;

    vlctlval.intVal = VL_MGV_KEYERMODE_SPATIAL;    /* set the keyer mode */
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_KEYER_MODE, &vlctlval);
    vlctlval.fractVal.denominator = 1000; /* set the fract denominator */
    for (i = min; i <= max; i+= speed) /* loop and set wipe positions */
    {
	vlctlval.fractVal.numerator = i; /* position will be loop variable i */
	vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_POSN,&vlctlval);
	if (axis == 2 ) /* if axis 2, set position of the perpendicular also */
	vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_POSN_PERP,&vlctlval);
    }    
}

void
dowipe(void)
{
    VLControlValue vlctlval;
    int i, min, max, speed, axis;

    vlctlval.fractVal.denominator = 1000;   /* set the fract denominator */

    vlctlval.intVal = VL_MGV_WIPETYPE_CORNER;   /* set a wipe type */
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_TYPE, &vlctlval);
    vlctlval.intVal = VL_MGV_WIPEANGLE_NE;	    /* set a wipe angle */	    
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_ANGLE, &vlctlval);
    vlctlval.fractVal.numerator = 500;	    /* set a wipe center */
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_CENT, &vlctlval);
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_CENT_PERP, &vlctlval);
    vlctlval.intVal = 0;		    /* set a repeat, invert  factor */
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_REPT, &vlctlval);
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_REPT_PERP, &vlctlval);
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_INVERT, &vlctlval);
    vlctlval.intVal = 1;		    /* set symmetry on */
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_SYMMETRY, &vlctlval);
    vlctlval.intVal = 7;		    /* set wipe_fuzz : 7 = sharp */
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_FUZZ, &vlctlval);
    /* set up params for wipe loop: */
    min = 0;   max = 500;    speed = 4; axis = 2;
    doswitchloop(min,max,speed,axis); /* call the loop function */
}

main(int argc, char **argv)
{
    char *winname = NULL;
    int opterr = 0;
    int c,i;
    int dev;
    int ret;
    int nofork = 0;
    int x, y;
    int print_ids = 0;
    Atom WM_PROT_SET[2];
    Atom _SGI_VIDEO;
    VLControlValue val;
    
    _progname = argv[0];

    /*
     * Parse command line options
     * -f   disable forking
     * -n   device number
     * -d   set debugging
     * -t   set title
     * -v   use video input n
     * -I   print node and path IDs
     * -h   display help message
     */
    while ((c = getopt(argc, argv, "t:dv:Ifh")) != EOF) 
    {
	switch(c) 
	{
	    case 'f':
		nofork = 1;
	    break;
	    
	    case 'd':
		debug++;
	    break;
	    
	    case 't':
		winname = optarg;
	    break;
	    
	    case 'I':
		print_ids = 1;
	    break;

	    case 'v':
		vin = atoi(optarg);
		Debug("video input = %d\n", vin);
	    break;
	    
	    case 'h':
		usage();
		exit(0);
	    break;
	}
    }

    if (opterr) 
    {
    	usage();
	exit(1);
    }

    /* Run in background if no-forking option not set */
    if (!nofork) 
    {
	ret = fork();
	switch (ret) 
	{
	    case 0:
	    break;
	    
	    case -1:
		fprintf(stderr, "%s: can't fork\n");
		exit(1);
	    break;
	    
	    default:
		exit(0);
	    break;
	}
    }

    /* Connect to the daemon */
    if (!(vlSvr = vlOpenVideo(""))) 
    {
	printf("%s: opening video: %s\n",_progname,vlStrError(vlGetErrno()));
	exit(1);
    }

    /* Get the list of devices the daemon supports */
    if (vlGetDeviceList(vlSvr, &devlist) < 0) 
    {
	printf("%s: getting device list: %s\n",_progname,vlStrError(vlGetErrno()));
	exit(1);
    }

   /*
    * Find Impact Device Number ( only one in a system )
    */

    for (i=0; i<devlist.numDevices; i++)
    {
            if (strcmp(devlist.devices[i].name,"impact") == 0)
                devicenum = i;
    }

    /*
     * No Impact Device found.
     */
    if ( -1 == devicenum )
    {
      printf("This program requires 'impact' hardware.\a\n");
      exit(1);
    }

    /*
     * Establish a path between the screen source and video
     * drain. Then add a video source node and a screen drain
     * node.
     */

    /*
     * Setup drain nodes on the screen and video.
     */
    drn_scr = vlGetNode(vlSvr, VL_DRN, VL_SCREEN, VL_ANY);
    drn_vid = vlGetNode(vlSvr, VL_DRN, VL_VIDEO, VL_ANY);
    
    /*
     * Setup source nodes on the screen and video.
     */
    src_scr = vlGetNode(vlSvr, VL_SRC, VL_SCREEN, VL_ANY);
    src_vid = vlGetNode(vlSvr, VL_SRC, VL_VIDEO, vin);

    blend_node = vlGetNode(vlSvr, VL_INTERNAL, VL_BLENDER, VL_ANY);

    vlPath = -1;

    /*
     * Get the device info
     */
	deviceName = devlist.devices[devicenum].name;
	deviceId   = devlist.devices[devicenum].dev;
        /* Try to create the screen to video path */
	vlPath = vlCreatePath(vlSvr, deviceId, src_scr, drn_vid);
	/* Add the video source and screen drain nodes */
	ret = vlAddNode(vlSvr, vlPath, src_vid);
	if (!ret)
	    ret = vlAddNode(vlSvr, vlPath, drn_scr);
	if (ret)
	{  /* Device doesn't support this path, these nodes */
	    vlDestroyPath(vlSvr, vlPath);
	    vlPath = -1;
	}

    if (vlAddNode(vlSvr, vlPath, blend_node))
    {
	vlPerror("Add Device Node");
	vlDestroyPath(vlSvr, vlPath);
	vlPath = -1;
    }
    if (vlPath == -1)
    { /* Couldn't create the path, quit */
	vlPerror("Path Setup");
	docleanup();
    }

     /* Print the node and path IDs for cmd line users */
    if (print_ids)
    {
	printf("SIMPLEWIPE NODE IDs:\n");
	printf("screen source = %d\n", src_scr);
	printf("video source = %d\n", src_vid);
	printf("screen drain = %d\n", drn_scr);
	printf("video drain = %d\n", drn_vid);
	printf("blend = %d\n", blend_node);
	printf("PATH ID = %d\n", vlPath);
    }
    		
    /* Set up the hardware for and define the usage of the path */
    if (vlSetupPaths(vlSvr, (VLPathList)&vlPath, 1, VL_SHARE, VL_SHARE) < 0)
    {
	vlPerror(_progname);
	exit(1);
    }

    /*
     * Set Source window first, so one gets full resolution
     */ 
    vlGetControl(vlSvr, vlPath, src_scr, VL_SIZE, &val);
    vlSetControl(vlSvr, vlPath, src_scr, VL_SIZE, &val);

    vlGetControl(vlSvr, vlPath, drn_scr, VL_SIZE, &val);
    lastw = val.xyVal.x; lasth = val.xyVal.y;

    /* Set the keyer mode, keyer source and blend controls */
    val.intVal = VL_MGV_KEYERMODE_LUMA;
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_KEYER_MODE, &val);
    val.intVal = src_scr;
    vlSetControl(vlSvr, vlPath, blend_node, VL_BLEND_A, &val);
    val.intVal = src_vid;
    vlSetControl(vlSvr, vlPath, blend_node, VL_BLEND_B, &val);


    /* Display the drain window */
    if (!(dpy = XOpenDisplay(""))) 
    {
	printf("%s: can't open display\n", _progname);
	exit(1);
    }

    drnwin = XCreateWindow(dpy, DefaultRootWindow(dpy),
			   0, lasth, lastw, lasth, 0,
			   CopyFromParent, CopyFromParent, CopyFromParent,
			   (ulong) 0, NULL);


    _SGI_VIDEO = XInternAtom(dpy, "_SGI_VIDEO", False);
    WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    WM_PROTOCOLS = XInternAtom(dpy, "WM_PROTOCOLS", False);
    WM_PROT_SET[0] = WM_DELETE_WINDOW;
    WM_PROT_SET[1] = _SGI_VIDEO;
    XSetWMProtocols(dpy,drnwin, WM_PROT_SET, 2);

    if (!winname) {
      XStoreName(dpy, drnwin, "Simplewipe Drain");
    } else {
      XStoreName(dpy, drnwin, winname);
    }

    
    /* Adjust the drain window's aspect ratio */
    constrainWindowResize(dpy, drnwin, lastw, lasth);

    /* Set the video to appear in window drnwin */
     val.intVal = drnwin;
    vlSetControl(vlSvr, vlPath, drn_scr, VL_WINDOW, &val);

    /* Get the location of the screen drain */
    vlGetControl(vlSvr, vlPath, drn_scr, VL_ORIGIN, &val);
    /* Move its window to that location */
    XMoveWindow(dpy, drnwin, val.xyVal.x, val.xyVal.y);

    XMapWindow(dpy, drnwin);
    XSelectInput(dpy, drnwin, KeyPressMask|VisibilityChangeMask
			    |ExposureMask|StructureNotifyMask);
    XSync(dpy, 0);

    /* Specify a file descriptor and pending check function for VL events */			  
    vlRegisterHandler(vlSvr, ConnectionNumber(dpy), processXEvent,
		      (VLPendingFunc)XPending, dpy);
		      
    /* Set up event handler routine as callback for all events */
    vlAddCallback(vlSvr, vlPath, VLAllEventsMask, processVLEvent, NULL);

    /* Set the VL event mask so we only get control changed events */
    vlSelectEvents(vlSvr, vlPath, VLControlChangedMask);

    /* Start the data transfer immediately (i.e. don't wait for trigger) */
    vlBeginTransfer(vlSvr, vlPath, 0, NULL);

    /*
     * Inform the user how to use the program.
     */
    printf("\nWith the mouse positioned over the video window,\n");
    printf("press the 'w' key to execute the wipe.\n");
    
    /* Handle event dispatching */
    vlMainLoop();
}

/*
 * VideoTracking - if the user changes the size of the window, 
 * update the video to reflect the new size and position.
 */
void
VideoTracking(Window win, int x, int y)
{
    Window dummyWin;
    VLControlValue val;
 
    /* Get X's idea of origin */
    XTranslateCoordinates(dpy, win, DefaultRootWindow(dpy),
                          0, 0,
                          &x, &y,
                          &dummyWin);

    /* Try to make vl match X */
    val.xyVal.x = x;
    val.xyVal.y = y;
    if (win == drnwin)
	vlSetControl(vlSvr, vlPath, drn_scr, VL_ORIGIN, &val);
    else
	vlSetControl(vlSvr, vlPath, src_scr, VL_ORIGIN, &val);
}

/* 
 * VLEvent processing for video to screen.
 * We only deal with control changed events.
 */
void
processVLEvent(VLServer vlSvr, VLEvent *ev, void *dummy)
{
    VLControlChangedEvent *cev = (VLControlChangedEvent *) ev;
    VLControlValue val;

    Debug("VL event.type = %d\n", ev->reason);
    switch (ev->reason)
    {   /* Ignore all but a change in the drain's location */
	case VLControlChanged:
	    if ((cev->type == VL_ORIGIN)&&(cev->node == drn_scr))
	    {
	        /* Drain moved, move window accordingly */
		vlGetControl(vlSvr, vlPath, drn_scr, VL_ORIGIN, &val);
		XMoveWindow(dpy, drnwin, val.xyVal.x, val.xyVal.y);
	    }
	break;
    }
}

/* XEvent processing for video to screen */
void
processXEvent(uint fd, void *source)
{
    int i;

    if (source == (caddr_t)dpy) 
    {
	XEvent ev;
	
	XNextEvent(dpy, &ev);
	switch (ev.type) 
	{
	    case ClientMessage:
		if (ev.xclient.message_type == WM_PROTOCOLS)
		    if (ev.xclient.data.l[0] == WM_DELETE_WINDOW)
			docleanup();
	    break;
	    
	    case Expose:		/* These really don't effect us */
	    case GraphicsExpose:
	    case VisibilityNotify:
	    break;
		
	    case ConfigureNotify:       /* Window moved or changed size */
		VideoTracking(ev.xany.window, ev.xconfigure.x,ev.xconfigure.y);
	    break;
		
	    case KeyPress: 
	    {
		XKeyEvent *kev = (XKeyEvent *)&ev;
		KeySym keysym;
		char buf[4];
				
		XLookupString(kev, buf, 1, &keysym, 0);
		switch (buf[0]) 
		{   /* Quit */
		    case 'q':
		    case 'Q':
		    case ESC_KEY:
			docleanup();
		    break;
		    case 'w':
		    case 'W':
			dowipe();
		    break;			
		    default:
		    break;
		}
	    }
	}
    }
}

/* Dispose of the vl structures */
void
docleanup(void)
{   /* End the data transfer */
    vlEndTransfer(vlSvr, vlPath);
     
    /* Destroy the path, free it's memory */
    vlDestroyPath(vlSvr, vlPath);

    /* Disonnect from the daemon */
    vlCloseVideo(vlSvr);

    exit(0);
}
