
/*
** vtos.c - video to screen
*/

#ident "$Revision: 1.2 $"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <bstring.h>
#include <sys/time.h>
#include <limits.h>
#include <dmedia/dmedia.h>
#include <dmedia/vl.h>
#include <dmedia/dm_buffer.h>
#include <dmedia/dm_params.h>
#include <gl/image.h>

#include <X11/X.h>   /* X */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <Xm/MwmUtil.h>

#include <GL/glx.h>  /* OpenGL */
#include <GL/gl.h>

int ntoi(char *str, int *val);
void usage(void);
void CreateWindowAndContext(Window *win, GLXContext *ctx, int *visualAttr, 
    char name[]);

char *prognm;

int Debug_f = 0;
int Nframes_f = 1;
int Viddev_f = VL_ANY;
int Vidnode_f = VL_ANY;
int mask = VLTransferCompleteMask | VLTransferFailedMask;
Display *dpy;
Window window;
GLXContext 	   ctx;
int 		init_x = 0;
int 		init_y = 0;
XVisualInfo *vi;

/* smallest single buffered RGBA visual */
int visualAttr[] = {GLX_RGBA, GLX_RED_SIZE, 8, 
		    GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
		    None};

int xsize, ysize;

#define MAX(a,b)	(((a)>(b))?(a):(b))

int
main(int ac, char *av[])
{
    int	c, errflg = 0;
    VLServer svr;
    VLNode drn;
    VLNode src;
    VLPath path;
    VLControlValue val;
    DMparams * plist;
    VLEvent event;
    int xfersize;
    DMbufferpool pool;
    DMbuffer dmbuffer;
    fd_set fdset;
    int	maxfd;
    int pathfd;
    int xfd;
    int i;
    int nready;
    int done = 0;
    void *addr;

    prognm = av[0];

    while( (c = getopt(ac,av,"Dc:v:n:")) != -1 ) {
	switch( c ) {
	case 'D':
	    Debug_f++;
	    break;
	case 'c':
	    if( ntoi(optarg, &Nframes_f) )
		errflg++;
	    break;
	case 'v':
	    if( ntoi(optarg, &Viddev_f) )
		errflg++;
	    break;
	case 'n':
	    if( ntoi(optarg, &Vidnode_f) )
		errflg++;
	    break;
	case '?':
	    errflg++;
	    break;
	}
    }

    if( errflg ) {
	usage();
	return 1;
    }

    if( Debug_f ) {
	printf("%s: dev %d:%d, %d frames\n", prognm, 
	    Viddev_f, Vidnode_f, Nframes_f);
    }

    if( (svr = vlOpenVideo("")) == NULL ) {
	fprintf(stderr,"%s: vlOpenVideo failed\n",prognm);
	return 1;
    }

    if( (drn = vlGetNode(svr, VL_DRN, VL_MEM, VL_ANY)) == -1 ) {
	vlPerror("get mem drain node");
	return 1;
    }

    if( (src = vlGetNode(svr, VL_SRC, VL_VIDEO, Vidnode_f)) == -1 ) {
	vlPerror("get video source node");
	return 1;
    }

    if( (path = vlCreatePath(svr, Viddev_f, src, drn)) == -1 ) {
	vlPerror("get path");
	return 1;
    }

    if( vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE) == -1 ) {
	vlPerror("setup paths");
	return 1;
    }

    /* Set packing mode to RGB */
    val.intVal = VL_PACKING_RGB_8;
    vlSetControl(svr, path, drn, VL_PACKING, &val);

    val.intVal = VL_FORMAT_RGB;
    vlSetControl(svr, path, drn, VL_FORMAT, &val);

    vlGetControl(svr, path, drn, VL_SIZE, &val);
    xsize = val.xyVal.x;
    ysize = val.xyVal.y;

    /* set up the OpenGL/X window */
    CreateWindowAndContext(&window, &ctx, visualAttr, "vtos:");
    XSelectInput(dpy, window, ButtonPressMask|KeyPressMask);
    glLoadIdentity();
    glViewport(0, 0, xsize,ysize);
    glOrtho(0, xsize, 0, ysize, -1, 1);
    glRasterPos2f(0, ysize-1);
    glDrawBuffer(GL_FRONT); /* On Vino, we only use the front buffer */
    		/* video is upside down to graphics! */
    glPixelZoom(1.0, -1.0);

    if( dmParamsCreate(&plist) == DM_FAILURE ) {
	fprintf(stderr, "%s: create params\n");
	return 1;
    }

    xfersize = vlGetTransferSize(svr, path);

    if( dmBufferSetPoolDefaults(plist, Nframes_f, xfersize, DM_FALSE, DM_FALSE)
	== DM_FAILURE ) {
	fprintf(stderr, "%s: set pool defs\n",prognm);
	return 1;
    }

    if( vlDMPoolGetParams(svr, path, drn, plist) == DM_FAILURE ) {
	fprintf(stderr, "%s: get pool reqs\n",prognm);
	return 1;
    }

    /* create the buffer pool */
    if( dmBufferCreatePool(plist, &pool) == DM_FAILURE ) {
	fprintf(stderr, "%s: create pool\n", prognm);
	return 1;
    }

    if( vlDMPoolRegister(svr, path, drn, pool) == DM_FAILURE ) {
	fprintf(stderr, "%s: register pool\n", prognm);
	return 1;
    }

    if( vlSelectEvents(svr, path, mask) == -1 ) {
	vlPerror("select events");
	return 1;
    }

    /* Begin the data transfer */
    if( vlBeginTransfer(svr, path, 0, NULL) == -1 ) {
	vlPerror("begin transfer");
	return 1;
    }

    /* spin in the get event loop until we've reached nframes */
    if( vlPathGetFD(svr, path, &pathfd) ) {
	vlPerror("getpathfd");
	return 1;
    }
    xfd = ConnectionNumber(dpy);
    maxfd = MAX(xfd, pathfd) + 1;

    while( !done ) {
	FD_ZERO(&fdset);
	FD_SET(pathfd, &fdset);

	if( (nready = select(maxfd, &fdset, 0, 0, 0)) == -1 ) {
	    perror("select");
	    return 1;
	}

	if( nready == 0 ) {
	    continue;
	}

	if( FD_ISSET(xfd, &fdset) ) {
	    XEvent event;
	    KeySym key;

	    /* ignore them for now. */
	    while( XPending(dpy) ) {
		XNextEvent(dpy, &event);
		switch( event.type ) {
		case ButtonPress:
		    break;
		case KeyPress:
		    XLookupString(&event.xkey, NULL, 0, &key, NULL);
		    switch (key) {
		    case XK_Escape:
		    case XK_q:
			done = 0;
			break;
		    default:
			break;
		    }
		    break;
		default:
		    break;
		}
	    }
	}
	if( FD_ISSET(pathfd, &fdset) ) {
	    while( vlEventRecv(svr, path, &event) == DM_SUCCESS ) {
		switch( event.reason ) {
		case VLTransferComplete:
		    vlEventToDMBuffer(&event, &dmbuffer);
		    addr = dmBufferMapData(dmbuffer);
		    glDrawPixels(xsize, ysize, GL_ABGR_EXT, GL_UNSIGNED_BYTE, 
			(char *) addr);
		    (void)dmBufferFree(dmbuffer);
				/* dump it to the screen */
		    break;
		case VLTransferFailed:
		    fprintf(stderr, "Transfer failed!\n");
		    done = 1;
		    break;
		default:
		    if( Debug_f ) {
			fprintf(stderr, "Received unexpect event %s\n", 
			    vlEventToName(event.reason));
		    }
		    break;
		}
	    }
	}
    }

    (void)vlEndTransfer(svr, path);

    (void)vlDMPoolDeregister(svr, path, src, pool);

    (void)vlDestroyPath(svr, path);

    (void)vlCloseVideo(svr);

    return 0;
}

void
usage()
{
    fprintf(stderr,"usage: [options]\n",prognm);
    fprintf(stderr,"    -c <frame count>\n");
    fprintf(stderr,"    -v <video device>\n");
    fprintf(stderr,"    -n <video node>\n");
}

int
ntoi(char *str, int *val )
{
    char *strp;	
    long ret;

    if( *str == '\'' ) {
	str++;
	return (*str)?*str:-1;
    }

    ret = strtol(str,&strp,0);

    if( ((ret == 0) && (strp == str)) ||
	(((ret == LONG_MIN) || ret == LONG_MAX) && (errno == ERANGE)) )
	return -1;

    if( (ret > INT_MAX) || (ret < INT_MIN) ) {
	return -1;
    }
    
    *val = (int)ret;
    
    return 0;
}

int
xioerror( Display *dpy )
{
    exit(1); 
}

Bool 
waitForMapNotify(Display *dpy, XEvent *ev, XPointer arg)
{
	    return (ev->type == MapNotify && ev->xmapping.window == *((Window *) arg));
}

/* create and window and opengl context */
void    
CreateWindowAndContext(Window *win, GLXContext *ctx,int *visualAttr,char name[])
{
    XSizeHints hints;
    XSetWindowAttributes swa;
    Colormap cmap;
    int isRgba;
    XEvent event;
    XIOErrorHandler  oldXError;
    int i;

    hints.min_width = xsize/2;
    hints.max_width = 2*xsize;
    hints.base_width = hints.width = xsize;
    hints.min_height = ysize/2;
    hints.max_height = 2*ysize;
    hints.base_height = hints.height = ysize;
    hints.flags = USSize | PMinSize | PMaxSize;
    			/* If user requested initial window position of 0,0 */
    /* get a connection */
    if(dpy == NULL)
       dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        fprintf(stderr, "Can't connect to display `%s'\n",
                getenv("DISPLAY"));
        exit(EXIT_FAILURE);
    }
    oldXError = XSetIOErrorHandler (xioerror);
    
    /* get an appropriate visual */
    vi = glXChooseVisual(dpy, DefaultScreen(dpy), visualAttr);
    if (vi == NULL) {
        fprintf(stderr, "No matching visual on display `%s'\n",
                getenv("DISPLAY"));
        exit(EXIT_FAILURE);
    }
    (void) glXGetConfig(dpy, vi, GLX_RGBA, &isRgba);
    /* create a GLX context */
    if ((*ctx = glXCreateContext(dpy, vi, 0, GL_TRUE)) == NULL){
       fprintf(stderr,"Cannot create a context.\n");
       exit(EXIT_FAILURE);
    }
    /* create a colormap (it's empty for rgba visuals) */
    cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen),
                           vi->visual, isRgba ? AllocNone : AllocAll);
    if (cmap == NULL) {
        fprintf(stderr,"Cannot create a colormap.\n");
        exit(EXIT_FAILURE);
    }
    /* fill the colormap with something simple */
    if (!isRgba) {
        static char *cnames[] = {
            "black","red","green","yellow","blue","magenta","cyan","white"
        };
                        
        for (i = 0; i < sizeof(cnames)/sizeof(cnames[0]); i++) {
            XStoreNamedColor(dpy, cmap, cnames[i], i, DoRed|DoGreen|DoBlue);
        }
    }
 
    /* create a window */
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.event_mask = StructureNotifyMask |KeyPressMask | ExposureMask;

    *win = XCreateWindow(dpy, RootWindow(dpy, vi->screen),
                        init_x, init_y, xsize, ysize,
                        0, vi->depth, InputOutput, vi->visual,
                        CWBorderPixel | CWColormap | CWEventMask, &swa);
    if (*win == NULL) {   
        fprintf(stderr,"Cannot create a window.\n");
        exit(EXIT_FAILURE);
    }
    XStoreName(dpy, *win, name);  
    XSetNormalHints(dpy, *win, &hints);
 
    XMapWindow(dpy, *win);
    XIfEvent(dpy, &event, waitForMapNotify, (XPointer) win);
    /* Connect the context to the window */
    if (!glXMakeCurrent(dpy, *win, *ctx)) {
        fprintf(stderr, "Can't make window current to context\n");
        exit(EXIT_FAILURE);
    }
}
