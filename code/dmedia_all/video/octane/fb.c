/*
 * Program: Frame Buffer Test Example
 *
 * This program is an example blending with the fb node
 * The input video is passed through the Frame Buffer to
 * use the delay.  Then mixed with the blender to show 
 * the effect.
 */
/*
 * System Include Files
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

/*
 * Include files
 */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/MwmUtil.h>
/*
 * Video Include files
 */
#include <dmedia/vl.h>
#include <dmedia/vl_mgv.h>
#include <dmedia/vl_impact.h>
/* 
 * GL include files
 */
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
/*
 * Globals for this program ( static to control the name space )
 */
static VLServer		vlSvr = NULL;
static VLPath		vlPath = -1;
static VLDevList	devlist;
static VLNode		src_vid1;
static VLNode		drn_scr;
static VLNode		drn_vid;
static VLNode		blend_node;
static VLNode		fb_node;

static Display		*dpy;
static Window		drnwin;
static char		*_progname;
static int		devicenum = -1;
static VLDev		deviceId;
static int		debug = 0;
static VLBuffer		buf;
static VLInfoPtr	info;
static int		lastw,lasth;
/*
 */
Atom WM_DELETE_WINDOW;
Atom WM_PROTOCOLS;

/*
 * Macros
 */
#define Debug	if (debug) printf

/*
 * Defeines
 */
#define ESC_KEY 0x1b

/*
 * Usage String:
 */
#define USAGE \
"%s: [-f] [-t] [-d] [-I] [-h]\n\
\t-f\tdisable forking\n\
\t-t\tset window title\n\
\t-d\tdebug mode\n\
\t-I\tprint node and path IDs for command line interface users\n\
\t-h\tthis help message\n"

/*
 * Command line help message
 */
void 
usage(void)
{
    fprintf(stderr, USAGE, _progname);
}

/*
 * Constrain the window's aspect ratio.
 */
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


/*
 * The Simple program that used the Frame Buffer
 */
main(int argc, char **argv)
{
    char ch;
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
    
    /*
     * Remember our command line name for error messages
     */
    _progname = argv[0];

    /*
     * Parse command line options
     * -f   disable forking
     * -d   set debugging
     * -t   set title
     * -v   use video input n
     * -I   print node and path IDs
     * -h   display help message
     */
    while ((c = getopt(argc, argv, "t:dIhf")) != EOF) 
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
	    
	    case 'h':
		usage();
		exit(0);
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

    /*
     * Connect to the daemon
     */
    if (!(vlSvr = vlOpenVideo(""))) 
    {
	printf("%s: opening video: %s\n",_progname,vlStrError(vlGetErrno()));
	exit(1);
    }

    /*
     * Get the list of devices the daemon supports
     */
    if (vlGetDeviceList(vlSvr, &devlist) < 0) 
    {
	printf("%s: getting device list: %s\n",
		_progname,vlStrError(vlGetErrno()));
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
     * Setup drain nodes on the screen and video.
     */
    drn_vid = vlGetNode(vlSvr, VL_DRN, VL_VIDEO, VL_ANY);
    
    /*
     * Setup drain nodes on the screen and video.
     */
    drn_scr = vlGetNode(vlSvr, VL_DRN, VL_SCREEN, VL_ANY);
    
    /*
     * Setup source nodes on the memory and video.
     */

    src_vid1 = vlGetNode(vlSvr, VL_SRC, VL_VIDEO, VL_ANY);

    /*
     * Create a Blend node.  This also allows is to use the keyer.
     */

    blend_node = vlGetNode(vlSvr, VL_INTERNAL, VL_BLENDER, VL_ANY);

    /*
     * Create a Blend node.  This also allows is to use the keyer.
     */

    fb_node = vlGetNode(vlSvr, VL_INTERNAL, VL_FB, VL_ANY);

    /*
     * Mark the Path as OK
     */
    ret = 0;

    /*
     * Establish a path between the screen source and video
     * drain. Then add a video blend node and a screen drain
     * node. All of the nodes are on this path.
     */

    vlPath = vlCreatePath(vlSvr, VL_ANY, VL_ANY, VL_ANY);

    if(vlPath == -1)
    {
	/*
         * Device doesn't support this path, these nodes.
	 */
	    vlDestroyPath(vlSvr, vlPath);
	    Debug("Can not Create Path\n");
            ret = -1;
    } 
    /*
     * Add the video source and screen drain nodes.
     */

	if (!ret) ret = vlAddNode(vlSvr, vlPath, drn_scr);
	if(ret)
	{
	    /*
             * Device doesn't support this path, these nodes.
	     */
	    vlDestroyPath(vlSvr, vlPath);
	    vlPath = -1;
	    Debug("Can not add drn_scr Node\n");
	} 

	if (!ret) ret = vlAddNode(vlSvr, vlPath, src_vid1);
	if(ret)
	{
	    /*
             * Device doesn't support this path, these nodes.
	     */
	    vlDestroyPath(vlSvr, vlPath);
	    vlPath = -1;
	    Debug("Can not add src_vid1 Node\n");
	} 

	if (!ret) ret = vlAddNode(vlSvr, vlPath, drn_vid);
	if(ret)
	{
	    /*
             * Device doesn't support this path, these nodes.
	     */
	    vlDestroyPath(vlSvr, vlPath);
	    vlPath = -1;
	    Debug("Can not add drn_vid Node\n");
	} 

	if (!ret) ret = vlAddNode(vlSvr, vlPath, blend_node);
	if(ret)
	{
	    /*
             * Device doesn't support this path, these nodes.
	     */
	    vlDestroyPath(vlSvr, vlPath);
	    vlPath = -1;
	    Debug("Can not add blend_node Node\n");
	} 

	if (!ret) ret = vlAddNode(vlSvr, vlPath, fb_node);
	if(ret)
	{
	    /*
             * Device doesn't support this path, these nodes.
	     */
	    vlDestroyPath(vlSvr, vlPath);
	    vlPath = -1;
	    Debug("Can not add fb_node Node\n");
	} 

    /*
     * Couldn't create the path, quit.
     */
    if (vlPath == -1)
    {
	vlPerror("Path Setup");
	docleanup();
    }

    /*
     * Print the node and path IDs for cmd line users.
     */
    if (print_ids)
    {
	printf("FB NODE IDs:\n");
	printf("Video Source 1= %d\n", src_vid1);
	printf("Screen Drain  = %d\n", drn_scr);
	printf("Video Drain   = %d\n", drn_vid);
	printf("Blender       = %d\n", blend_node);
	printf("Frame Buffer  = %d\n", fb_node);
	printf("Path     ID   = %d\n", vlPath);
    }
    		
    /*
     * Set up the hardware for and define the usage of the path
     */
    if (vlSetupPaths(vlSvr, (VLPathList)&vlPath, 1, VL_SHARE, VL_SHARE) < 0)
    {
	vlPerror(_progname);
	exit(1);
    }

    /*
     * Set Source window first, so one gets full resolution.
     */ 

    vlGetControl(vlSvr, vlPath, drn_scr, VL_SIZE, &val);
    lastw = val.xyVal.x; lasth = val.xyVal.y;

    /*
     * Display the drain window
     */
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
      XStoreName(dpy, drnwin, "Frame Buffer Delay Blending");
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

    /*
     * Start the data transfer immediately (i.e. don't wait for trigger)
     */
    vlBeginTransfer(vlSvr, vlPath, 0, NULL);

    /*
     * SETUP the keyer mode, keyer source and blend controls.
     */
    val.intVal = VL_MGV_KEYERMODE_SPATIAL;
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_KEYER_MODE, &val);

    if( ret = vlSetConnection(vlSvr, vlPath,
				src_vid1, VL_IMPACT_PORT_PIXEL_SRC_A,
				fb_node,  VL_IMPACT_PORT_PIXEL_DRN_A,
				FALSE ) )
    {
	vlPerror( "SetConnection for fb Node" );
	printf("*Connection failed for the FB node %d \n" , ret);
    }

    if( ret = vlSetConnection(vlSvr, vlPath,
				fb_node, VL_IMPACT_PORT_PIXEL_SRC_A,
				blend_node,  VL_IMPACT_PORT_PIXEL_DRN_A,
				FALSE ) )
    {
	vlPerror( "SetConnection for Blender Pixel A Node" );
	printf("*Connection failed for the FB node %d \n" , ret);
    }

    if( ret = vlSetConnection(vlSvr, vlPath,
				src_vid1, VL_IMPACT_PORT_PIXEL_SRC_A,
				blend_node,  VL_IMPACT_PORT_PIXEL_DRN_B,
				FALSE ) )
    {
	vlPerror( "SetConnection for Blender Pixel B Node" );
    }

    if( ret = vlSetConnection(vlSvr, vlPath,
				blend_node, VL_IMPACT_PORT_PIXEL_SRC_A,
				drn_vid,  VL_IMPACT_PORT_PIXEL_DRN_A,
				FALSE ) )
    {
	vlPerror( "SetConnection for fb Node" );
    }

    if( ret = vlSetConnection(vlSvr, vlPath,
				blend_node, VL_IMPACT_PORT_PIXEL_SRC_A,
				drn_scr,  VL_IMPACT_PORT_PIXEL_DRN_A,
				FALSE ) )
    {
	vlPerror( "SetConnection fir the Blender to the Drain Node" );
    }

   /*
    * Select blender bg alpha input = 1.0
    */
    val.intVal = 2;
    if (vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_BLEND_B_ALPHA_SELECT,
        &val)!=0) {
      printf("set blender bg alpha input error\n");
      vlPerror(_progname);
      exit(1);
    }

   /*
    * Select blender fg alpha input = keyer
    */
    val.intVal = 2;
    if (vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_BLEND_A_ALPHA_SELECT,
        &val)!=0) {
      printf("set blender fg alpha input error\n");
      vlPerror(_progname);
      exit(1);
    }


/*
 * Setup the Blend Parameters
 */
    val.intVal = 2;
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_ANGLE, &val);

    val.intVal = 7;
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_KEYER_DETAIL, &val);

    val.intVal = 3;
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_WIPE_REPT, &val);

    /*
     * Handle event dispatching
     */
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
    {
    }
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

    Debug("VL event.type = %d of Type = %d\n", ev->reason, cev->type);
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
    VLControlValue val;

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
		char keybuf[4];
				
		XLookupString(kev, keybuf, 1, &keysym, 0);
		switch (keybuf[0]) 
		{   /* Quit */
		    case 'q':
		    case 'Q':
		    case ESC_KEY:
			docleanup();
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

