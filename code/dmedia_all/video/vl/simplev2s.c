/*
 * File:          simplev2s.c
 *
 * Usage:         simplev2s 
 *
 * Description:   Simplev2s demonstrates live video to screen.
 *		  This application only runs on video hardware 
 *     	 	  that has a video output port. It will not run 
 *  		  on a VINO video board. 
 *
 * Functions:     SGI Video Library functions used
 *
 *                vlOpenVideo()
 *                vlGetNode()
 *                vlCreatePath()
 *                vlSetupPaths()
 *                vlSetControl()
 *                vlBeginTransfer()
 *                vlEndTransfer()
 *                vlDestroyPath()
 *                vlCloseVideo()
 */
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <Xm/MwmUtil.h>
#include <X11/Xutil.h>
#include <dmedia/vl.h>

main(int argc, char **argv)
{
    VLServer svr;
    VLPath path;
    Display *dpy;
    Window vwin;
    VLNode src, drn;
    VLControlValue val;
    char *progname, *ptr;
    int x, y, c;
    uint w, h, bw, d;
    Window dummyWin;
    XSizeHints size_hints;
    XClassHint class_hints;

    /* get basename of argv */
    if ((ptr = strrchr(*argv, '/')) != NULL) progname = ++ptr;
    else progname = *argv;

    /* Open an X display */
    if (!(dpy = XOpenDisplay("")))
	exit(1);
	
    /* Connect to the video daemon */
    if (!(svr = vlOpenVideo("")))
	exit(1);
	
    /* Create a window to show the video */
    vwin = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
			       10, 10, 640, 480, 0,
			       BlackPixel(dpy, DefaultScreen(dpy)),
			       BlackPixel(dpy, DefaultScreen(dpy)));

    /* Ignore window manager placement set the window to 10, 10 */
    size_hints.flags = USPosition;
    size_hints.x = 10;
    size_hints.y = 10;

    /* set class properties for 4Dwm desktop */
    class_hints.res_name = progname;
    class_hints.res_class = progname;
	
    XSetClassHint(dpy, vwin, &class_hints);
    XSetWMNormalHints(dpy, vwin, &size_hints);
    XMapWindow(dpy, vwin);
    XFlush(dpy);
 
    /* Create a source node on a video device */
    src = vlGetNode(svr, VL_SRC, VL_VIDEO, VL_ANY);
    
    /* Create a drain node on the screen */
    drn = vlGetNode(svr, VL_DRN, VL_SCREEN, VL_ANY);
    
    /* Create a path on the first device that supports it */
    if((path = vlCreatePath(svr, VL_ANY, src, drn)) < 0)
    	exit(1);
    
     /* Set up the hardware for and define the usage of the path */
    vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE);
    
    /* Set the X window to be the drain */
    val.intVal = vwin;
    vlSetControl(svr, path, drn, VL_WINDOW, &val); 
    
    /* Get X and VL into the same coordinate system */   
    XTranslateCoordinates(dpy, vwin, DefaultRootWindow(dpy),
                          0, 0,&x, &y, &dummyWin);
			  
    /* Set the live video to the same location and size as the X window */   
    val.xyVal.x = x;
    val.xyVal.y = y;
    vlSetControl(svr, path, drn, VL_ORIGIN, &val); 
    
    XGetGeometry(dpy, vwin, &dummyWin, &x, &y, &w, &h, &bw, &d);
    val.xyVal.x = w;
    val.xyVal.y = h;
    vlSetControl(svr, path, drn, VL_SIZE, &val);
    
    /* Begin the data transfer */
    vlBeginTransfer(svr, path, 0, NULL);        

    /* Wait until the user presses a key */
    printf("Press return to exit.\n");
    c = getc(stdin);   
    
    /* End the data transfer */
    vlEndTransfer(svr, path); 
    
    /* Clean up and exit */   
    vlDestroyPath(svr, path);
    vlCloseVideo(svr);
}

