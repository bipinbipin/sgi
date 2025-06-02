/* sonic.c -  exemplifying adding audio to an interactive graphics application
 * updated 4/97 to port to Audio Library 2.0, Audio File Library 2.0, Motif and OpenGL.
 * Compile with  -lGLU -lGLw -lGL -lXm -lXt -lX11 -laudio -laudiofile -lm
 */

#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/lock.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <malloc.h>
#include <math.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Protocols.h>
#include <X11/keysym.h>
#include <GL/GLwMDrawA.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <audio.h>
#include <audiofile.h>

/* To get a feel for how little CPU audio is using, change USEGFX to 0 */
#define USEGFX 1 

/* AUDIO DEFINES */
#define BUFFERSIZE 4000            /* Size of output sample buffer. */
#define SONICRATE 44100.0	   /* Sample rate we want to think in */
#define PI 3.141592653             /* Our old friend from trig.     */
#define SOUNDDIR "/usr/share/data/sounds/"
#define BALLFILENAME "prosonus/instr/metal_triad_power.E1.aiff"
#define WALLFILENAME "prosonus/musictags/slinky_slap.aiff"

/* OTHER DEFINES */
#define GEOM_TO_AUDIO 0.15  /* constant mapping gfx distance to audio distance */
#define OVERALL_VOLUME 2.0  /* constant effecting apparent loudness of audio */
#define TIMEOUT 33          /* 33ms ~= 30Hz graphics frame rate. */
#define SPHERERADIUS 1.0    
#define BOXSIZE 20.0
#define NO_HIT 0                   
#define JUST_HIT 1
#define BEEN_HIT 2

/* GRAPHICS PROTOTYPES */
static void graphics_loop(int argc, char *argv[]);
static void scene_init(Widget, XtPointer, XtPointer);
static void do_resize(Widget, XtPointer, XtPointer);
static void draw_scene(Widget, XtPointer, XtPointer);
static void input(Widget, XtPointer, XtPointer);
static void change_view(Widget, XtPointer, XEvent *, Boolean *);
static void shutdown(Widget, XtPointer, XtPointer);
static void meanwhile(XtPointer, XtIntervalId *);
static GLuint make_box(void);

/* AUDIO PROTOTYPES */
static void audio_init(void);
static int init_sound(float **, char*, double);
static void audio_loop(void *);
static void process_oneshot_audio( float *, float, float, int);
static void process_continuous_audio( float *, float, float, int);
static void audio_exit(int);

/* AUDIO RELATED GLOBALS */
float *ballbuf, *wallbuf;       /* Sample buffers for ball and wall sounds */
int  ballframes,wallframes;    /* Number of samples in each of these buffers */
ALconfig audioconfig;           /* Audio Library port configuration */
ALport audioport;               /* Audio Library audio port */
AFfilehandle affh;              /* Audio File Library file handle */

/* OTHER GLOBALS */
GLuint sphere,box;              /* GL object identifiers for ball and box */
float rx, ry;                   /* Angles of rotation for observer */
float sphx,sphy,sphz;           /* Coordinates of sphere */
float movx,movy,movz;           /* Vector for sphere motion */
float hitx, hity, hitz;         /* Coordinates of ball hitting wall */
int moveball;			/* Indicates whether ball is in motion or not */
int hit;                        /* Status of ball hitting wall */
int gfxdone;                    /* Indicates completion of graphics process */
int audiodone;                  /* Indicates completion of audio process */

static Dimension xsize,ysize;
static XtAppContext app_context;

/* GRAPHICS ROUTINES */
void graphics_loop(int argc, char *argv[])
{

static String fallback_resources[] = {
    "*frame*shadowType: SHADOW_IN",
    "*sonic*width: 500",
    "*sonic*height: 500",
    "*glwidget*allocateBackground: TRUE",
    "*glwidget*background:red",
    NULL
};

static int attribs[] = { GLX_RGBA, GLX_RED_SIZE, 1, GLX_DOUBLEBUFFER, None};

    GLXContext glx_context;
    XVisualInfo *visinfo;
    Display *dpy;
    Atom atoms[1];
    Arg args[20];
    int n;
    Widget glw, toplevel, form;

    /* Create the toplevel widget */
    toplevel = XtAppInitialize(&app_context, "sonic", (XrmOptionDescList) NULL,
                               (Cardinal)0, (int *)&argc, (String*)argv,
                               fallback_resources, (ArgList)NULL, 0);

    /* Create a form to contain everything */
    n = 0;
    form = XmCreateForm(toplevel, "form", args, n);
    XtManageChild(form);

    /* Specify the visual directly */
    dpy = XtDisplay(toplevel);
    if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs))) {
        printf("Error: no suitable RGB visual\n");
	shutdown(NULL,NULL,NULL);
    }

    /* Create the OpenGL widget */
    n = 0;
    XtSetArg(args[n], GLwNvisualInfo, visinfo); n++;
    XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM); n++;
    XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM); n++;
    glw = GLwCreateMDrawingArea(form, "glwidget", args, n);
    XtManageChild (glw);

    /* Add callbacks and event handlers */
    XtAddEventHandler(glw, PointerMotionMask, False, change_view, 0);
    XtAddCallback(glw, GLwNexposeCallback, draw_scene, 0);
    XtAddCallback(glw, GLwNresizeCallback, do_resize, 0);
    XtAddCallback(glw, GLwNinputCallback, input, 0);

    /* The timeout meanwhile() keeps things moving when there's no input */
    XtAppAddTimeOut(app_context, TIMEOUT , meanwhile,(XtPointer)glw);

    /* Register to receive notice when window manager "Quit" is selected */
    atoms[0] = XmInternAtom(XtDisplay(toplevel),"WM_DELETE_WINDOW",TRUE);
    XmAddWMProtocols(toplevel,atoms,1);
    XmAddWMProtocolCallback(toplevel,atoms[0],shutdown,NULL);

    XtRealizeWidget(toplevel); 	/* Instatiate all Motif widgets/windows */

    glx_context = glXCreateContext(XtDisplay(glw),visinfo,0,GL_FALSE);
    GLwDrawingAreaMakeCurrent(glw, glx_context);
    scene_init(glw,NULL,NULL);

    XtAppMainLoop(app_context); /* Motif main loop */
}


/* meanwhile() -- handles motion when no input is happening */
void meanwhile(XtPointer data, XtIntervalId *id)
{
    Widget glw = (Widget) data;

    if (moveball) {

	sphx += movx; sphy += movy; sphz += movz;
                        /* Move sphere in current direction */

	if (sphx > (BOXSIZE - SPHERERADIUS)) {    /* Check for x bounce */
	    sphx = 2*(BOXSIZE - SPHERERADIUS) - sphx;
	    movx = -movx; hit = JUST_HIT;
	}
	else if (sphx < (SPHERERADIUS - BOXSIZE)) {
	    sphx = 2*(SPHERERADIUS - BOXSIZE) - sphx;
	    movx = -movx; hit = JUST_HIT;
	}

	if (sphy > (BOXSIZE - SPHERERADIUS)) {    /* Check for y bounce */
	    sphy = 2*(BOXSIZE - SPHERERADIUS) - sphy;
	    movy = -movy; hit = JUST_HIT;
	}
	else if (sphy < (SPHERERADIUS - BOXSIZE)) {
	    sphy = 2*(SPHERERADIUS - BOXSIZE) - sphy;
	    movy = -movy; hit = JUST_HIT;
	}

	if (sphz > (BOXSIZE - SPHERERADIUS)) {    /* Check for z bounce */
	    sphz = 2*(BOXSIZE - SPHERERADIUS) - sphz;
	    movz = -movz; hit = JUST_HIT;
	}
	else if (sphz < (SPHERERADIUS - BOXSIZE)) {
	    sphz = 2*(SPHERERADIUS - BOXSIZE) - sphz;
	    movz = -movz; hit = JUST_HIT;
	}

	if (hit == JUST_HIT) {                     /* Record where bounced */
	     hitx = sphx; hity=sphy; hitz=sphz;
    	}

    }
    draw_scene(glw, NULL, NULL);

    /* Re-register this function as timeout to keep things moving. */
    XtAppAddTimeOut(app_context, TIMEOUT, meanwhile,(XtPointer)glw);
}


/* input() -- handle input from GL widget */
void input(Widget glw, XtPointer client_data, XtPointer calld)
{
    char buffer[1];
    KeySym keysym;
    GLwDrawingAreaCallbackStruct *call_data = 
		(GLwDrawingAreaCallbackStruct *)calld;

    switch(call_data->event->type) {
        case KeyRelease:
            /* It is necessary to convert the keycode to a keysym before
             * it is possible to check if it is an escape */
            if (XLookupString((XKeyEvent *) call_data->event,buffer,1,
			&keysym,NULL) == 1) {

		switch(keysym) {
		    case ((KeySym)XK_Escape):     /* Escape key exits app */
			shutdown(glw,NULL,NULL);
			break;
		    default:
			break;
		}
	    }
            break;

        case ButtonPress:     /* Stop/start ball motion on Left Mouse button */
            if(((XButtonEvent *)call_data->event)->button == 1) {
                moveball = !moveball; 
		hit = JUST_HIT;
                hitx = sphx; hity=sphy; hitz=sphz;
            }
            break;

	default:
	    break;
    }
}


/* move viewpoint w/ mouse motion */
void change_view(Widget w, XtPointer client_data, XEvent *ev, Boolean *cont)
{
    XMotionEvent *xmev = (XMotionEvent *)ev;

    ry=300.0*(2.0*(float)(xmev->x)/(float)xsize-1.0);
    rx=300.0*(2.0*(float)(xmev->y)/(float)ysize-1.0);
}


/* Iniitialize scene and GL set up */
void scene_init(Widget w, XtPointer client_data, XtPointer call_data)
{
    const GLfloat ambient[3] = {0.100, 0.100, 0.100};
    const GLfloat mat_diffuse[3] = {0.000, 0.369, 0.165};
    const GLfloat mat_specular[3] = {0.200, 0.500, 0.100};
    const GLfloat lt_position[4] = {0.000, 0.000, 1.000, 0.000};
    const GLfloat lt_diffuse[3] = {0.600, 0.600, 0.600};
    GLUquadric *sphereq;

    XtVaGetValues(w, XmNwidth, &xsize, XmNheight, &ysize, NULL);

    /* Set up projection matrix and model-view stack */
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluPerspective(60.0, xsize/(float)ysize, .25, BOXSIZE*1.718);

    /* Initialize GL display list for sphere using GL utility library */
    sphereq = gluNewQuadric();
    gluQuadricOrientation(sphereq,GLU_OUTSIDE);
    sphere = glGenLists(1);
    glNewList(sphere,GL_COMPILE);
        gluSphere(sphereq, SPHERERADIUS, 25, 25);
    glEndList();
    gluDeleteQuadric(sphereq);

    box = make_box();         /* Initialize GL display list for walls (box) */

    glMaterialf(GL_FRONT, GL_SHININESS, 10.0);
    glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);

    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 1);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    glLightfv(GL_LIGHT0, GL_POSITION, lt_position);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lt_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, ambient);

    glCullFace(GL_BACK);
    glEnable(GL_LIGHT0);
    glDisable(GL_DEPTH_TEST);
}


GLuint make_box(void)  /* Returns a GL display list that defines walls (box) */
{
    GLuint obj;
    GLubyte c = 96;

    obj = glGenLists(1);         /* GL display list to contain the box  */
    glNewList(obj,GL_COMPILE);

    glBegin(GL_TRIANGLE_FAN);
	glColor3ub(   0,   0,   0); glVertex3f(-1.0,-1.0,-1.0);
	glColor3ub(   0,   0,   c); glVertex3f(-1.0,-1.0, 1.0);
	glColor3ub(   0,   c,   c); glVertex3f(-1.0, 1.0, 1.0);
	glColor3ub(   0,   c,   0); glVertex3f(-1.0, 1.0,-1.0);
	glColor3ub(   c,   c,   0); glVertex3f( 1.0, 1.0,-1.0);
	glColor3ub(   c,   0,   0); glVertex3f( 1.0,-1.0,-1.0);
	glColor3ub(   c,   0,   c); glVertex3f( 1.0,-1.0, 1.0);
	glColor3ub(   0,   0,   c); glVertex3f(-1.0,-1.0, 1.0);
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
	glColor3ub(   c,   c,   c); glVertex3f( 1.0, 1.0, 1.0);
	glColor3ub(   c,   c,   0); glVertex3f( 1.0, 1.0,-1.0);
	glColor3ub(   c,   0,   0); glVertex3f( 1.0,-1.0,-1.0);
	glColor3ub(   c,   0,   c); glVertex3f( 1.0,-1.0, 1.0);
	glColor3ub(   0,   0,   c); glVertex3f(-1.0,-1.0, 1.0);
	glColor3ub(   0,   c,   c); glVertex3f(-1.0, 1.0, 1.0);
	glColor3ub(   0,   c,   0); glVertex3f(-1.0, 1.0,-1.0);
	glColor3ub(   c,   c,   0); glVertex3f( 1.0, 1.0,-1.0);
    glEnd();

    glPushMatrix();            /* Lines for cube outline */
    glScalef(0.99,0.99,0.99);  /* Shrink lines just a tad, so they're visible */
    glColor3ub(160,160,160);

    glBegin(GL_LINE_STRIP);
	glVertex3f(-1.0,-1.0,-1.0);
	glVertex3f( 1.0,-1.0,-1.0);
	glVertex3f( 1.0, 1.0,-1.0);
	glVertex3f(-1.0, 1.0,-1.0);
	glVertex3f(-1.0,-1.0,-1.0);
	glVertex3f(-1.0,-1.0, 1.0);
	glVertex3f( 1.0,-1.0, 1.0);
	glVertex3f( 1.0, 1.0, 1.0);
	glVertex3f(-1.0, 1.0, 1.0);
	glVertex3f(-1.0,-1.0, 1.0);
    glEnd();

    glBegin(GL_LINES);
	glVertex3f( 1.0,-1.0,-1.0);
	glVertex3f( 1.0,-1.0, 1.0);
	glVertex3f( 1.0, 1.0,-1.0);
	glVertex3f( 1.0, 1.0, 1.0);
	glVertex3f(-1.0, 1.0,-1.0);
	glVertex3f(-1.0, 1.0, 1.0);
    glEnd();

    glPopMatrix();
    glEndList();

    return(obj);
}


/* drawscene() -- Draw current state of ball and walls */
void draw_scene(Widget w, XtPointer client_data, XtPointer call_data)
{
    glPushMatrix();
    glRotatef(ry, 0.0, 1.0, 0.0);       /* Viewpoint orientation */
    glRotatef(rx, 1.0, 0.0, 0.0);       
    /* (We'll see rx and ry again in the audio routines) */

    glDisable(GL_LIGHTING);	/* No lighting when drawing box background */
    glDisable(GL_CULL_FACE);	/* No backface removal when drawing box */
    glPushMatrix();
    glScalef(BOXSIZE,BOXSIZE,BOXSIZE);
    glCallList(box);
    glPopMatrix();

    glEnable(GL_LIGHTING);	/* Use lighting when drawing sphere */
    glEnable(GL_CULL_FACE);	/* Use backface removal when drawing sphere */
    glPushMatrix();
    glTranslatef(sphx,sphy,sphz);
    glScalef(SPHERERADIUS,SPHERERADIUS,SPHERERADIUS);
    glCallList(sphere);
    glPopMatrix();

    glPopMatrix();
    glXSwapBuffers(XtDisplay(w),XtWindow(w));
}


/* do_resize() called from GL widget's resizeCallback */
void do_resize(Widget w, XtPointer client_data, XtPointer calld)
{
    GLwDrawingAreaCallbackStruct *call_data = 
		(GLwDrawingAreaCallbackStruct *)calld;

    glViewport(0, 0, (GLint)call_data->width-1, (GLint)call_data->height-1);
}


/* shutdown() called when exiting from ESC key or Window Manager Menu "Quit" */
void shutdown(Widget w, XtPointer client_data, XtPointer call_data)
{
    /* GL cleanup */
    gfxdone = 1;
    glDeleteLists(box,1); 
    glDeleteLists(sphere,1);

    /* Wait for audio to shut down properly */
    while (!audiodone) sginap(1);
    exit(0);
}


/* ------------------------------------------------------------------------- */
/* AUDIO ROUTINES */
void audio_init(void) /* Configure the audio port */
{
    static char soundfilename[256];
  
    strcpy(soundfilename,SOUNDDIR);
    strcat(soundfilename,BALLFILENAME);
    ballframes=init_sound( &ballbuf, soundfilename, SONICRATE); /* Get the ball waveform*/

    strcpy(soundfilename,SOUNDDIR);
    strcat(soundfilename,WALLFILENAME);
    wallframes=init_sound( &wallbuf, soundfilename, SONICRATE); /* Get the wall waveform*/

    audioconfig = alNewConfig();             /* Get a new audio configuration */    
    alSetSampFmt( audioconfig, AL_SAMPFMT_FLOAT); /* Sample format uses floats*/
    alSetFloatMax(audioconfig, 1.0);      /* Floats will vary from -1.0 to 1.0*/

    alSetQueueSize(audioconfig,2*BUFFERSIZE); 
            /* Set the sample queue size to be twice that of our sound buffer */

    alSetChannels(audioconfig,AL_STEREO);             /* Use stereo */

    audioport = alOpenPort("sine","w",audioconfig); /* Open audio for writing */
    if (audioport == NULL) {
        printf("Error: Couldn't open audio port\n"); 
	audio_exit(-1);
    }
}


int init_sound(float **floatbuf, char *filename, double userate)  /* Read audio file */
{
/*  This routine reads the specified audio file, lets the audio file library
 *  converts the samples to floating point and return the length of the 
 *  sample buffer.  
 */

    int numframes;     /* Number of sample frames in audio file */
    int vframes;       /* Number of sample frames in buffer */
    double filerate;   /* sample rate of file */

    /* We'd need an AFfilesetup with more info if the file has just raw data */
    if((affh = afOpenFile(filename,"r",NULL)) == NULL) {  /*Open audio file*/
        printf("Error: Couldn't open audio file %s\n",filename); 
	audio_exit(-3);
    }

    if((filerate = afGetRate(affh, AF_DEFAULT_TRACK)) <= 0.0) { /* Find sample rate */
	printf("Error: %s sample rate %f <= 0.0\n",filename,filerate);
	audio_exit(-4);
    }

    if((numframes = afGetFrameCount(affh, AF_DEFAULT_TRACK)) <= 0) { /* Find buffer size */
	printf("Error: Can't get %s framecount\n",filename);
	audio_exit(-5);
    }

    vframes = (int)(userate/filerate * (double)numframes);
    if((double)vframes != (userate/filerate * (double)numframes))
	vframes++; /* for rounding */

    /* Tell the audiofile library how we want the samples */
    afSetVirtualRate(affh,AF_DEFAULT_TRACK,userate);
    afSetVirtualSampleFormat(affh,AF_DEFAULT_TRACK,AF_SAMPFMT_FLOAT,32);
    afSetVirtualChannels(affh,AF_DEFAULT_TRACK,1);

    *floatbuf = (float *)malloc(vframes*sizeof(float)); 
                /* Create a buffer of floats - floats are much nicer than */
                /* integers for the amplitude calculations later on.      */

    /* Read 16 bit samples into temp buffer before float conversion */
    if(afReadFrames(affh,AF_DEFAULT_TRACK,*floatbuf,numframes) <= 0) {
        printf("Error reading audio file %s\n",filename); 
	audio_exit(-6);
    }
 
    afCloseFile(affh);
    affh = AF_NULL_FILEHANDLE;

    return(vframes);    /* Return number of frames read in */
}


void audio_loop(void *arg)		/* Audio process main loop */
{
    float distance,amplitude,balance;   /* What we need to find out */
    float soundbuf[BUFFERSIZE];         /* Sample buffer for computations */
    float relx,rely,relz;      /* Coordinates of ball relative to orientation */
    float radx,rady;           /* Angle describing orientation in radians */

    if(schedctl(NDPRI,0,NDPHIMAX) == -1) /* Effective user id needs to be root*/
      printf("Warning: Couldn't run at high, non-degrading priority.\n");
                      /* Make process run at a high, non-degrading priority. */
		      /* Prevents clicks from losing CPU to other processes. */

    if(plock(PROCLOCK)!=0)               /* Effective user id needs to be root*/
      printf("Warning: Couldn't lock process into memory.\n");
                                     /* Lock this process into memory. Helps */
			             /* prevent clicks due to page swapping. */

    audio_init();      /* Initialize audio */
    while(!gfxdone) {    /* Keep going until gfx process says otherwise */

        /* Put sphere's coordinates through viewing transformation. */
        /* Rot x (rx) * Rot y (rx) just like the graphics are doing */

        radx = PI*rx/180.0;
        rady = PI*ry/180.0;

        relx =  sphx*cos(rady) + sphy*sin(radx)*sin(rady) + 
                sphz*cos(radx)*sin(rady);
        rely =  sphy*cos(radx) - sphz*sin(radx);
        relz = -sphx*sin(rady) + sphy*sin(radx)*cos(rady) + 
                sphz*cos(radx)*cos(rady);

        distance = fsqrt( relx*relx + rely*rely + relz*relz );

        balance = acos( relx/distance) / PI;
            /* Compute the audio orientation of the sphere by taking the     */
            /* angle of the projected vector from the observer to the object */
            /* the object onto the xy plane of the observer.  Left-right     */
            /* balance is easiest to achieve with a 2 speaker system such as */
            /* headphones, so we'll need to use the distance in the          */
            /* x direction (relative to the observer's orientation) to find  */
            /* left-right balance.  With only 2 speakers top-bottom balance  */
            /* is very difficult to simulate, so we'll ignore it here.       */

        distance *= GEOM_TO_AUDIO; /* scale graphics world to audio world */
        amplitude = OVERALL_VOLUME * 1.0 / (distance*distance + 1.0);  
            /* Use inverse square proportion for amplitude */


        process_continuous_audio(soundbuf,amplitude,balance,BUFFERSIZE);

        if(hit != NO_HIT) {  /* Don't bother with walls if we don't need to */

            /* Pass the coordinates of the hit through the same */
            /* transformations as above.                        */
            relx =  hitx*cos(rady) + hity*sin(radx)*sin(rady) + 
                    hitz*cos(radx)*sin(rady);
            rely =  hity*cos(radx) - hitz*sin(radx);
            relz = -hitx*sin(rady) + hity*sin(radx)*cos(rady) + 
                    hitz*cos(radx)*cos(rady);

            distance = fsqrt( relx*relx + rely*rely + relz*relz );
            balance = acos( relx/distance) / PI;
            distance *= GEOM_TO_AUDIO; /* scale graphics world to audio world */
            amplitude = OVERALL_VOLUME * 1.0 / (distance*distance + 1.0);  

            process_oneshot_audio(soundbuf,amplitude,balance,BUFFERSIZE);
        }

        while(alGetFillable(audioport) < (BUFFERSIZE/2)) {
            sginap(1); /* Relinquish the CPU and wait til enough room to write*/
		       /* the entire sound buffer to the audio queue exists. */
        }

        alWriteFrames(audioport,soundbuf,(BUFFERSIZE/2)); 
                        /* Output samples to audio port */
    }
    
    while(alGetFilled(audioport) > 0);
        /* Wait till all the sound has been played before closing the port.  */

    audio_exit(0);
}


/* Process one-shot audio for bouncing off wall sound - time critical */
void process_oneshot_audio(float *soundbuf, float amplitude,    
                           float balance, int buflen)
{
    static int wallphase;            /* Index into wall's sound buffer */
    float rbalance = 1.0 - balance;  /* Speeds up calculations */
    float ramp,lamp;                 /* speeds up calculations */
    int i;


    if(hit == JUST_HIT)   /* If this is the first time around, the index     */
    {                     /* into the sound buffer needs to be reinitialized */
        hit = BEEN_HIT;   /* ialized, otherwise we may need to pick up       */
        wallphase = 0;    /* processing where we left off, since the entire  */
                          /* duration of sound may be longer than the time   */
    }                     /* slice we need to process.                       */

    amplitude *= 5.0;             /* The wall is usually pretty far away */
                                  /* boost up the volume for collisions  */
    ramp = amplitude * rbalance; /* initialize for a single multiply w/in loop*/
    lamp = amplitude * balance;  /* initialize for a single multiply w/in loop*/

    /*  This loop mixes the relevant portion of the wall sound with our 
     *  current soundbuffer while performing multiplications for amplitude.
     *  Some general performance tips we're taking advantage of:
     *	1) Our multipliers lamp and ramp are precomputed outside the loop.
     */
    for(i=0; (i<buflen) && (wallphase<wallframes); wallphase++) {

        /* Since we know that this will be processed _after_ there is valid */
        /* audio data in the sample buffer, we'll perform the operation of  */
        /* mixing the new audio data with the old audio data as the samples */
        /* are copied. In this case the mixing operation is a simple add. A */
        /* weighted average may be needed if the sum is likely to clip.     */

        soundbuf[i++] += lamp * wallbuf[wallphase];     /* Left  */
        soundbuf[i++] += ramp * wallbuf[wallphase];   /* Right */
    } /* Note that a stereo soundbuffer is interleaved LRLRLR... */

    if(wallphase == wallframes)      /* If the sound buffer is exhausted, */
        hit = NO_HIT;                /* we don't need to come back.       */
}


/* Process audio sample loop for moving ball - time critical */
void process_continuous_audio(float *soundbuf, float amplitude, 
                              float balance, int buflen)
{
    static int ballphase = 0;        /* Index into wall's sound buffer */
    int i;                           /* Counter */
    float rbalance = 1.0 - balance;   /* Speeds up calculations */
    float ramp,lamp;                 /* speeds up calculations */

    ramp = amplitude * rbalance; /* initialize for a single multiply w/in loop*/
    lamp = amplitude * balance;  /* initialize for a single multiply w/in loop*/

    /*  This loop copies the continuously looping ball sound into our current
     *  soundbuffer while performing multiplications for amplitude.
     *  Some general performance tips we're taking advantage of:
     *	1) Our multipliers lamp and ramp are precomputed outside the loop.
     *	2) The modulo operation required to continuously loop the phase
     *	   of the ball waveform is done outside the innermost loop.
     */
    for(i=0; i<buflen; ) {
        for(ballphase %= ballframes; (i<buflen) && (ballphase<ballframes);
		ballphase++) {

        /* Since we know that this will be processed _before_ there is valid */
        /* audio data in the sample buffer, we'll can just copy the data into*/
        /* the buffer, if data were already in the sample buffer, mixing     */
        /* would be necessary.   Continuous looping is acheived by the modulo*/
        /* function above. */

            soundbuf[i++] = lamp * ballbuf[ballphase];      /* Left  */
            soundbuf[i++] = ramp * ballbuf[ballphase];      /* Right */
        } /* Note that a stereo soundbuffer is interleaved LRLRLR... */
    } 
}


void audio_exit(int exitval)        /* Gracefully exit the audio process */
{
    if (wallbuf != NULL)            free(wallbuf);
    if (ballbuf != NULL)            free(ballbuf);
    if (audioconfig != NULL)        alFreeConfig(audioconfig);
    if (audioport != NULL)          alClosePort(audioport);
    if (affh != AF_NULL_FILEHANDLE) afCloseFile(affh);

    audiodone = TRUE;
    exit(exitval);
}


void main(int argc, char *argv[])
{
    int audio_pid;          /* Process id of child audio process */

    /* Initialize globals */
    gfxdone = audiodone = FALSE;
    moveball = TRUE;
    rx = ry = 0.0;
    sphx = 1.0; sphy = 2.0; sphz = 3.0;
    movx = 0.2; movy = 0.4;  movz = 0.2;
    hit = NO_HIT;

    if(USEGFX) { 	
	/* The sproc() command spawns a child process. The PR_SALL argument
	 * indicates that we want just about all our data shared between
	 * the parent graphics process and the child audio process.
	 * Sonic is constructed in such a way that the audio process
	 * only reads what the graphic process writes to it via some key
	 * global variables (and visa versa). This simplifies the task of 
	 * message passing considerably.  If your app requires a more 
	 * sophisticated approach, consider the semaphored scheme in 
	 * motifexample.c (using uspsema() and usvsema()) as one approach.
	 */
        audio_pid = sproc(audio_loop,PR_SALL);     /* Fork audio process */
        if (audio_pid == -1) {
            fprintf(stderr,"Error: Couldn't create audio process.\n");
            exit(-1);
        }
        graphics_loop(argc,argv);  /* Parent process goes on to do graphics */
    } else {
	audio_loop(NULL);
    }
}
