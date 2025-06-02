//
// motifmovie.c
//
//      Example showing how to play a movie in a motif widget
//
//      Also shows use of 
//
//          mvOpenPort() for MV_PORT_AUDIO, MV_PORT_GFX, and MV_PORT_VIDEO
//          mvBindMovieToPorts()
//          mvUnbindMovieFromPorts()
//
//          mvSetMovieSelectEvents
//          mvGetMovieEventFD
//
//          mvPlayAt()
//          mvGetCurrentDirection()
//
//          mvSetVideoDisplay()
//          mvGetVideoDisplay()
//
//      and MV_EVENT_WARNING, and mvGetPlayErrorStr().
// 
// 

#include <X11/Xlib.h>
#define XK_MISCELLANY
#include <X11/keysymdef.h>
#include <Xm/Xm.h>
#include <Xm/Frame.h>
#include <GL/GLwMDrawA.h>

#include <stdio.h>
#include <math.h>
#include <dmedia/moviefile.h>
#include <dmedia/movieplay.h>

static XtAppContext app_ctxt;
static GLXContext   ctxt = 0;
static XVisualInfo *vInfo = 0;
static int          doVideo = 0;
static int          doVideoAndGfx = 0;
static int          doubleBuffer = 0;
static int          bound = 0;

// 
// movie_event
//
static void movie_event(XtPointer closure, int *source, XtInputId *id)
{
    MVid      movie = (MVid)closure;
    MVevent   event;

    mvNextMovieEvent(movie, &event);
  
    switch (event.type)
    {
        case MV_EVENT_FRAME:
	    fprintf(stderr, "Playback frame %d.\n", event.mvframe.frame);
            break;

        case MV_EVENT_STOP:
	    fprintf(stderr, "Playback stopped.\n");
            break;

        case MV_EVENT_ERROR:
	    fprintf(stderr, "Error during playback of frame %d: %s.\n",
                     event.mverror.frame, mvGetPlayErrorStr(event.mverror.errcode, 
                                                             event.mverror.pid));
            break;

        case MV_EVENT_WARNING:
            switch(event.mvwarning.errcode)
            {
                MVtime   newTime;

                case MV_FRAME_SKIPPED:
                    if (mvGetCurrentDirection(movie) == MV_FORWARD)
                        newTime = event.mvwarning.movieTime + event.mvwarning.duration;
                    else
                        newTime = event.mvwarning.movieTime - event.mvwarning.duration;
                    fprintf(stderr, "Skipped from %lld to %lld\n", event.mvwarning.movieTime,
                                                               newTime);
                    break;
                case MV_FRAME_REPEATED:
                    fprintf(stderr, "Repeated time %lld for %lld\n", event.mvwarning.movieTime,
                                                                event.mvwarning.duration);
                    break;
                default:
	            fprintf(stderr, "Warning during playback: %s.\n",
                         mvGetErrorStr(event.mvwarning.errcode));
            }
	    break;

    } 
}

//
// Bind the movie for playback
//
//     For MV_PORT_VIDEO on Indy, Indigo, and Octane the X Window
//     (and it's entire ancestry) must be mapped before mvBindMovieToPorts
//     is called.  So we should make sure the window gets exposed
//     before calling this routine.
//
void bindMovie(Widget widget, MVid movie)
{
    int            fd;

    // Make sure window is mapped (necessary for galileo video hw)
    if (doVideo)
    {
        MVport            ports[2];
        MVvideoPortData   vdata;
        MVaudioPortData   adata;
        int               numPorts = 0, dum;

        // Let port choose config and be rude with the sample rate
        adata.config = 0;
        adata.ownsRate = DM_TRUE;
        adata.sampleRate = 0;

        // Display video on the gfx screen and let port setup vl stuff
        if (doVideoAndGfx)
        {
            vdata.dpy = XtDisplay(widget);
            vdata.win = XtWindow(widget);
            vdata.ctxt = ctxt;
            vdata.vInfo = vInfo;
        }
        else
        {
            vdata.dpy = 0;
            vdata.win = 0;
            vdata.ctxt = 0;
            vdata.vInfo = 0;
        }
        vdata.svr = 0;
        vdata.path = 0;
        vdata.src = 0;
        vdata.drn = 0;
        vdata.flickerFilter = 0;

        ports[numPorts] = mvOpenPort(MV_PORT_AUDIO, &adata);
        if (ports[numPorts] == 0)
        {
            fprintf(stderr, "Can't open audio port: %s\n", mvGetErrorStr(mvGetErrno()));
        }
        else 
            numPorts++;

        ports[numPorts] = mvOpenPort(MV_PORT_VIDEO, &vdata);
        if (ports[numPorts] == 0)
        {
            fprintf(stderr, "Can't open video port: %s\n", mvGetErrorStr(mvGetErrno()));
        }
        else 
            numPorts++;

        if (mvBindMovieToPorts(movie, ports, numPorts, &dum) == DM_FAILURE)
        {
            fprintf(stderr, "Can't bind movie: %s\n", mvGetErrorStr(mvGetErrno()));
        }
    }
    else
    {
        // This will bind a gfx window (and audio port if there's audio in 
        // the movie).  This will also pickup a hardware accelerator if
        // it's available
        mvSetMovieHwAcceleration(movie, DM_TRUE);
        if (mvBindOpenGLWindow(movie, XtDisplay(widget), XtWindow(widget), ctxt)
            == DM_FAILURE)
        {
            fprintf(stderr, "Can't bind movie: %s\n", mvGetErrorStr(mvGetErrno()));
            exit(-1);
        }
    }

    // Tell movie what kinds of events we're interested in 
    mvSetMovieSelectEvents(movie, MV_EVENT_MASK_FRAME | MV_EVENT_MASK_STOP |
                                  MV_EVENT_MASK_ERROR | MV_EVENT_MASK_WARNING);

    // Add movie's event fd to app 
    if (mvGetMovieEventFD(movie, &fd) != DM_SUCCESS)
    {
         fprintf(stderr, "Can't get movie fd: %s\n", mvGetErrorStr(mvGetErrno()));
    }
    else
        XtAppAddInput(app_ctxt, fd, (XtPointer)XtInputReadMask, movie_event, (XtPointer)movie);
}

//
// resize
//
static void resize(Widget widget, XtPointer client, XtPointer call)
{
    MVid                          movie = (MVid)client;
    Dimension                     width, height;

    XtVaGetValues(widget, XmNheight, &height, XmNwidth, &width, 0);
    mvSetViewSize(movie, (int)width, (int)height, 0);
}

//
// input
//
//    handle keyboard input
//
static void input(Widget widget, XtPointer client, XtPointer call)
{
    GLwDrawingAreaCallbackStruct *cbs = (GLwDrawingAreaCallbackStruct *)call;
    MVid                          movie = (MVid)client;
    MVtime                        t;
    MVtimescale                   scale = mvGetMovieTimeScale(movie);
    DMmedium                      medium = DM_IMAGE;
    MVsyncInfo                    info;
    stamp_t                       now;
    float                         speed;

    if (cbs->event->type == KeyRelease)
    {
	XKeyEvent	*keyEvent = (XKeyEvent *)(cbs->event);
	char		buf;
	KeySym		key;
	XComposeStatus 	compose;

        buf = 0;
	XLookupString(keyEvent, &buf, 1, &key, &compose);

	switch(key)
	{
	    case XK_Escape:
	        exit(0);
        }

        switch(buf)
        {
            case 'q':
                exit(0);

            case 'p':
                mvPlay(movie);
                break;

            case 't':
                dmGetUST((unsigned long long *)&now);

                // Start 3 seconds from now at the beginning of the movie
                info.ust = now + 3 * 1000000000LL;
                info.movieTime = 0;
                info.timeScale = mvGetMovieTimeScale(movie);

                mvPlayAt(movie, &info);
                break;

            case 's':
                mvStop(movie);
                break;

            case 'r':
                mvSetCurrentTime(movie, 0, 1);
                break;

	    case '+':	    /* one IMAGE frame ahead */
                mvGetMovieBoundary(movie, mvGetCurrentTime(movie, scale), 
                    scale, MV_FORWARD, MV_FRAME, 1, &medium, &t);
                mvSetCurrentTime(movie, t, scale);
	        fprintf(stderr, "Playback time %lld.\n", 
                             mvGetCurrentTime(movie, scale));
    
	        break;

	    case '-':	    /* one IMAGE frame back */
                mvGetMovieBoundary(movie, mvGetCurrentTime(movie, scale), 
                    scale, MV_BACKWARD, MV_FRAME, 1, &medium, &t);
                mvSetCurrentTime(movie, t,  scale);
	        fprintf(stderr, "Playback time %lld.\n", 
                             mvGetCurrentTime(movie , scale));
	        break;

	    case ')':	    
                speed = mvGetPlaySpeed(movie);
                if (speed > 0)
                    speed += 1.0;
                else
                    speed -= 1.0;
                if (speed == 0)
                    speed = 1.0;
                mvSetPlaySpeed(movie, speed);
	        fprintf(stderr, "Playback speed %f.\n", speed);
	        break;
    
	    case '(':	    
                speed = mvGetPlaySpeed(movie);
                if (speed > 0)
                    speed -= 1.0;
                else
                    speed += 1.0;

                if (speed == 0)
                    speed = -1.0;
                mvSetPlaySpeed(movie, speed);
	        fprintf(stderr, "Playback speed %f.\n", speed);
	        break;

	    case 'm':	    /* toggle audio muting */
	        if (mvGetEnableAudio(movie)) {
	            fprintf(stderr, " Audio OFF\n");
	            mvSetEnableAudio(movie, DM_FALSE);
	        } else {
	            fprintf(stderr, " Audio ON\n");
	            mvSetEnableAudio(movie, DM_TRUE);
	        }
	        break;

	    case 'v':	    /* toggle video muting */
	        if (mvGetEnableVideo(movie)) {
	            fprintf(stderr, " Video OFF\n");
	            mvSetEnableVideo(movie, DM_FALSE);
	        } else {
	            fprintf(stderr, " Video ON\n");
	            mvSetEnableVideo(movie, DM_TRUE);
	        }
	        break;

	    case 'l':	    /* trip through loop states */
	        fprintf(stderr, "toggle loop state to ");
	        switch (mvGetPlayLoopMode(movie)) {
	            case MV_LOOP_NONE:
	                mvSetPlayLoopMode(movie, MV_LOOP_CONTINUOUSLY);
	                fprintf(stderr, "CONTINUOUS\n");
	                break;
	            case MV_LOOP_CONTINUOUSLY:
	                mvSetPlayLoopMode(movie, MV_LOOP_SWINGING);
      
	                fprintf(stderr, "SWINGING\n");
	                break;
    	            case MV_LOOP_SWINGING:
	                mvSetPlayLoopMode(movie, MV_LOOP_NONE);
	                fprintf(stderr,  "NONE\n");
	                break;
	         }
	         break;

	    case 'b':       /* backward playback */
	        fprintf(stderr, "backward play\n");
	        mvSetPlaySpeed(movie, fabs(mvGetPlaySpeed(movie))*-1.0);
	        mvPlay(movie);
	        break;

	    case 'a':	    /* video display mode */
	        fprintf(stderr, "toggle video display mode to ");
	        switch (mvGetVideoDisplay(movie)) {
	            case MV_VIDEO_DISPLAY_UPPER_LEFT:
	                mvSetVideoDisplay(movie, MV_VIDEO_DISPLAY_CENTER);
	                fprintf(stderr, "CENTER\n");
	                break;
	            case MV_VIDEO_DISPLAY_CENTER:
	                mvSetVideoDisplay(movie, MV_VIDEO_DISPLAY_SCALE_TO_FIT);
      
	                fprintf(stderr, "SCALE\n");
	                break;
    	            case MV_VIDEO_DISPLAY_SCALE_TO_FIT:
	                mvSetVideoDisplay(movie, MV_VIDEO_DISPLAY_UPPER_LEFT);
	                fprintf(stderr, "UPPERLEFT\n");
	                break;
	         }
	         break;

	    case 'c':	    /* change bg color */
                {
                    static int bgColor = 0;
                    unsigned short r, g, b;

                    bgColor++;
                    bgColor %= 3;
    
                    switch (bgColor)
                    {
                        case 0:
                            r = g = b = 170; break;
                        case 1:
                            r = 255; g = b = 0; break;
                        case 2:
                            r = 0; g = b = 100; break;
                    }
    
	            fprintf(stderr, "changin color to %d %d %d\n", r, g, b); 
	            mvSetViewBackground(movie, r, g, b);
                }
                break;
	}
    }
}

//
// expose
//
static void expose(Widget widget, XtPointer client, XtPointer call)
{
    MVid   movie = (MVid)client;

    if (!bound)
    {
        int    width, height;

        // Clear the window
        XtVaGetValues(widget, XmNwidth, &width, XmNheight, &height, 0);
        glXMakeCurrent(XtDisplay(widget), XtWindow(widget), ctxt);
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        glXMakeCurrent(XtDisplay(widget), None, NULL);

        // Set the name
        XStoreName(XtDisplay(widget), XtWindow(widget), "motifmovie");

        // Bind the movie
        bindMovie(widget, movie); bound = 1;
    }

    mvShowCurrentTime(movie);
}

main(int argc, char *argv[])
{
    MVid   movie;
    MVrect rect;
    Widget toplevel;
    Widget frame;
    Widget gldraw;
    int    i;
    char   *file = NULL;
    int    width, height;

    if (argc < 2)
    {
        fprintf(stderr, "Usage: mmovie [-video] [-videoAndGfx] [-doubleBuffer] <movie>\n");
        exit(-1);
    }

    for (i = 1; i < argc; i++) 
    {
        if (strcmp(argv[i], "-video") == 0) {
            doVideo = 1;
        }
        else if (strcmp(argv[i], "-videoAndGfx") == 0) {
            doVideo = 1;
            doVideoAndGfx = 1;
        }
        else if (strcmp(argv[i], "-doubleBuffer") == 0) {
            doubleBuffer = 1;
        }
        else {
            if (file != NULL) {
                fprintf(stderr, "Only one movie at a time, please.\n");
                exit(-1);
            }
            file = argv[i];
            if (!(mvIsMovieFile(file))) {
                fprintf(stderr, "%s: %s is not a movie file.\n",
                        argv[0], file);
                exit(-1);
            }
        }
    }

    if (mvOpenFile(file, O_RDONLY, &movie) != DM_SUCCESS)
    {
        fprintf(stderr, "Can't open %s\n", file);
        exit(-2);
    }

    toplevel = XtVaAppInitialize(&app_ctxt, "MotifMovie", NULL, 0,
				   &argc, argv, NULL,
				   XmNallowShellResize, TRUE, 0);

    frame = XtVaCreateManagedWidget("Frame", xmFrameWidgetClass, 
                                    toplevel,
                                    NULL);

    mvGetMovieBoundingRect(movie, &rect);
    width = rect.right - rect.left;
    height = rect.top - rect.bottom;
    if (width <= 0) width = 100;
    if (height <= 0) height = 100;

    gldraw = XtVaCreateManagedWidget("Movie", glwMDrawingAreaWidgetClass, 
                                            frame, GLwNrgba, True,
                                            GLwNdoublebuffer, doubleBuffer,
                                            XmNwidth, width,
                                            XmNheight, height,
                                            NULL);

    // Create graphics context 
    XtVaGetValues(gldraw, GLwNvisualInfo, &vInfo, NULL);
    ctxt = glXCreateContext(XtDisplay(gldraw), vInfo, 0, GL_TRUE);

    XtAddCallback(gldraw, GLwNexposeCallback, expose, movie);
    XtAddCallback(gldraw, GLwNinputCallback,  input, movie);
    XtAddCallback(gldraw, GLwNresizeCallback, resize, movie);

    XtRealizeWidget(toplevel);

    XtAppMainLoop(app_ctxt);

    return 0;
}
