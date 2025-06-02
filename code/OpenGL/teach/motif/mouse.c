/*
 * mouse.c - double buffered RGBA motif program using mouse motion events
 */
/* compile: cc -o mouse mouse.c -lGLw -lGLU -lGL -lXm -lXt -lX11 */
#include <stdlib.h>
#include <stdio.h>
#include <Xm/Frame.h>
#include <Xm/RowColumn.h>
#include <X11/GLw/GLwMDrawA.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <string.h>
#include <GL/gl.h>
#include <math.h>
#include <time.h>
#include <getopt.h>
#include <"/home/tulip13/bipin/code/OpenGL/toolkits/libglut/glut.h">

static GLuint asphere;

#define POINTMASK (unsigned long)511
#define G (0.002)	/* eyept to red sphere gravity */
#define LG (0.3)
#define CUBESIDE (120.)
#define CUBESCALE (23.)
#define CUBEOFFX (-4.)
#define CUBEOFFY (0.)
#define CUBEOFFZ (57.)
#define FALSE 0
#define TRUE 1

/* globals */
float sigma = 10., r = 28., b = 8./3., dt = 0.003;
unsigned long rp = 0, bp = 0, gp = 0, yp = 0, mp = 0;
long xmax, ymax, zmax, zmin;
float rv[POINTMASK+1][3],			/* red points */
	bv[POINTMASK+1][3],		/* blue points */
	gv[POINTMASK+1][3],		/* green points */
	yv[POINTMASK+1][3],		/* yellow points */
	mv[POINTMASK+1][3];		/* magenta points */

int lpf;				/* number of new lines per frame */

float eyex[3],	/* eye location */
	 eyev[3],	/* eye velocity */
	 eyel[3];	/* lookat point location */
GLint fovy = 600;
float dx, dy, dz;
GLUquadricObj *quadObj;

float cubeoffx = CUBEOFFX;
float cubeoffy = CUBEOFFY;
float cubeoffz = CUBEOFFZ;
float farplane = 80.;

int animate = 1;

/* option flags */
GLboolean hexflag,		/* hexagons? */
	sflag, 			
	fflag,			
	wflag,
	gflag,
	debug;

/* option values */
short hexbright;	/* brightness for hexagon color */
int speed,		/* speed (number of new line segs per redraw) */
    frame;		/* frame rate (actually noise value for TIMER0) */
float a = 0,
    da;			/* hexagon rotational velocity (.1 degree/redraw) */
float gravity;

/* function declarations */
void init_3d(void);
void init_graphics(void);
void draw_hexcube(void);
void draw_hexplane(void);
void draw_hexagon(void);
void move_eye(void);
void redraw(void);
void next_line(float v[][3], unsigned long *p);
void parse_args(int argc, char **argv);
void print_usage(char*);
void print_info(void);
void sphdraw(float args[4]);
void setPerspective(int angle, float aspect, float zNear, float zFar);


static void Reshape(int width, int height)
{

      glViewport(0,0,width,height);
      glClear(GL_COLOR_BUFFER_BIT);
      xmax = width;
      ymax = height;
}

static void Key(unsigned char key, int x, int y)
{

    switch (key) {
      case 'g':
	animate = 1;
	glutPostRedisplay();
	break;
      case 's':
	 animate = 0;
	 glutPostRedisplay();
	 break;
      case 27:
	gluDeleteQuadric(quadObj);
	exit(0);
    }
}

static void Draw(void)
{
    int i, j;

    if (animate) {
	i = speed;
	while (i--) {
	    next_line(rv, &rp);
	    next_line(bv, &bp);
	    next_line(gv, &gp);
	    next_line(yv, &yp);
	    next_line(mv, &mp);
	}
	glPushMatrix();
	move_eye();
	redraw();
	glPopMatrix();
    }
}



static void draw_scene(void);
static void initialize(void);
static void update_view(int mstate, int ox, int oy, int nx, int ny);

static int attribs[] = { GLX_RGBA, GLX_RED_SIZE, 1, GLX_DOUBLEBUFFER, None};

static String fallbackResources[] = {
    "*sgiMode: True",
    "*useSchemes: all",
    "*glxwidget.width: 300", "*glxwidget.height: 300",
    "*frame.shadowType: SHADOW_IN",
    NULL};

static struct {			/* global UI variables - keep them together */
    XtAppContext appctx;
    Widget glxwidget;
    Boolean direct;
    GLfloat trans[3];		/* current translation */
    GLfloat rot[2];		/* current rotation */
} state;

static void
input(Widget w, XtPointer client_data, XtPointer call) {
   char buf[31];
   KeySym keysym;
   XEvent *event = ((GLwDrawingAreaCallbackStruct *) call)->event;
   static int mstate, omx, omy, mx, my;

   switch(event->type) {
   case KeyRelease:
      XLookupString(&event->xkey, buf, sizeof buf, &keysym, NULL);
      switch(keysym) {
      case XK_Escape:
	 exit(EXIT_SUCCESS);
	 break;
      default: break;
      }
      break;
    case ButtonPress:
	if (event->xbutton.button == Button2) {
	    mstate |= 2;
	    mx = event->xbutton.x;
	    my = event->xbutton.y;
	} else if (event->xbutton.button == Button1) {
	    mstate |= 1;
	    mx = event->xbutton.x;
	    my = event->xbutton.y;
	}
	break;
    case ButtonRelease:
	if (event->xbutton.button == Button2)
	    mstate &= ~2;
	else if (event->xbutton.button == Button1)
	    mstate &= ~1;
	break;
    case MotionNotify:
	if (mstate) {
	    omx = mx;
	    omy = my;
	    mx = event->xbutton.x;
	    my = event->xbutton.y;
	    update_view(mstate, omx,mx,omy,my);
	}
	break;  
   }
}

static void
resize(Widget w, XtPointer client_data, XtPointer call) {
   GLwDrawingAreaCallbackStruct *call_data;
   call_data = (GLwDrawingAreaCallbackStruct *) call;

   glViewport(0, 0, call_data->width, call_data->height);
}

static void
expose(Widget w, XtPointer client_data, XtPointer call) {
    GLwDrawingAreaCallbackStruct *call_data;
    call_data = (GLwDrawingAreaCallbackStruct *) call;

    draw_scene();
}

main(int argc, char *argv[]) {
    Display        *dpy;
    XVisualInfo    *visinfo;
    GLXContext      glxcontext;
    Widget          toplevel, frame;

    toplevel = XtOpenApplication(&state.appctx, "mouse", NULL, 0, &argc, argv,
		fallbackResources, applicationShellWidgetClass, NULL, 0);
    dpy = XtDisplay(toplevel);

    frame = XmCreateFrame(toplevel, "frame", NULL, 0);
    XtManageChild(frame);

    /* specify visual directly */
    if (!(visinfo = glXChooseVisual(dpy, DefaultScreen(dpy), attribs)))
	XtAppError(state.appctx, "no suitable RGB visual");

    state.glxwidget = XtVaCreateManagedWidget("glxwidget", glwMDrawingAreaWidgetClass,
	frame, GLwNvisualInfo, visinfo, NULL);
    XtAddCallback(state.glxwidget, GLwNexposeCallback, expose, NULL);
    XtAddCallback(state.glxwidget, GLwNresizeCallback, resize, NULL);
    XtAddCallback(state.glxwidget, GLwNinputCallback, input, NULL);

    XtRealizeWidget(toplevel);

    glxcontext = glXCreateContext(dpy, visinfo, 0, GL_TRUE);
    state.direct = glXIsDirect(dpy, glxcontext);
    GLwDrawingAreaMakeCurrent(state.glxwidget, glxcontext);

    initialize();

    XtAppMainLoop(state.appctx);
}

static void
initialize(void) {
    glShadeModel(GL_FLAT);
    gluPerspective(40., 3.0/2.0, 0.001, 100000.0);
    glTranslatef(0.0, 0.0, -3.0);
    glClearColor(0.2,0.2,0.2,0.);
}

static void
side(void) { /* make a square translated 0.5 in the z direction */
    glPushMatrix();
    glTranslatef(0.0,0.0,0.5);
    glRectf(-0.5,-0.5,0.5,0.5);
    glPopMatrix();
}

static void
cube(void) { /* make a cube out of 4 squares */
    glPushMatrix();
    side();
    glRotatef(90.,1.,0.,0.);
    side();
    glRotatef(90.,1.,0.,0.);
    side();
    glRotatef(90.,1.,0.,0.);
    side();
    glPopMatrix();
}

static void
draw_scene() {

    glClear(GL_COLOR_BUFFER_BIT);
    glPushMatrix();
    glTranslatef(state.trans[0], state.trans[1], state.trans[2]);
    glRotatef(state.rot[0], 1.0, 0.0, 0.0);
    glRotatef(state.rot[1], 0.0, 1.0, 0.0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glColor3f(.1, .1, .8);
    cube();
    glScalef(0.3,0.3,0.3);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColor3f(.8, .8, .1);

    init_3d();
    init_graphics();

    /* draw the first POINTMASK points in each color */
    while(rp < POINTMASK) {
	next_line(rv, &rp);
	next_line(bv, &bp);
	next_line(gv, &gp);
	next_line(yv, &yp);
	next_line(mv, &mp);
    }

    eyex[0] = eyex[1] = eyex[2] = 0.;
    eyel[0] = rv[rp][0];
    eyel[1] = rv[rp][1];
    eyel[2] = rv[rp][2];
	
    glPushMatrix();
    move_eye();
    redraw();
    glPopMatrix();

    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Key);
    glutIdleFunc(Draw);
    glutDisplayFunc(Draw);
    glutMainLoop();


    glPopMatrix();
    GLwDrawingAreaSwapBuffers(state.glxwidget);
    if (!state.direct) glFinish();	/* hack to improve net interactivity */
}

static void
update_view(int mstate, int ox, int nx, int oy, int ny) {
    int dx = ox - nx;
    int dy = ny - oy;

    switch(mstate) {
    case 1:	/* pan */
	state.trans[0] -= dx/100.;
	state.trans[1] -= dy/100.;
	break;
    case 2:	/* rotate */
	state.rot[0] += (dy * 180.0)/500.;
	state.rot[1] -= (dx * 180.0)/500.;
#define clamp(x) x = x > 360. ? x-360. : x < -360. ? x += 360. : x 
	clamp(state.rot[0]);
	clamp(state.rot[1]);
	break;
    case 3:	/* zoom */
	state.trans[2] -= (dx+dy)/100.;
	break;
    }
    draw_scene();
}

/* compute the next point on the path according to Lorenz' equations. */
void next_line(float v[][3], unsigned long *p)
{

    dx = sigma * (v[*p][1] - v[*p][0]) * dt;
    dy = (r*v[*p][0] - v[*p][1] + v[*p][0]*v[*p][2]) * dt;
    dz = (v[*p][0] *v[*p][1] + b*v[*p][2]) * dt;	
	
    v[(*p + 1) & POINTMASK][0] = v[*p][0] + dx;
    v[(*p + 1) & POINTMASK][1] = v[*p][1] + dy;
    v[(*p + 1) & POINTMASK][2] = v[*p][2] - dz;
    *p = (*p + 1) & POINTMASK;
}

void drawLines(unsigned long index, float array[POINTMASK][3])
{
    unsigned long p;
    int i;

#define LINE_STEP 4

    p = (index+1)&POINTMASK;
    i = LINE_STEP-(p % LINE_STEP);
    if (i == LINE_STEP) i=0;
    glBegin(GL_LINE_STRIP);
	/* draw points in order from oldest to newest */
	while(p != index) {
	    if (i == 0) {
		glVertex3fv(array[p]);
		i = LINE_STEP;
	    } 
	    i--;
	    p = (p+1) & POINTMASK;
	}
	glVertex3fv(array[index]);
    glEnd();
}

void redraw(void)
{
    unsigned long p;
	
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(hexflag)
	draw_hexcube();

    glColor3f(1.0, 0.0, 0.0);
    drawLines(rp, rv);
    sphdraw(rv[rp]);

    glColor3f(0.0, 0.0, 1.0);
    drawLines(bp, bv);
    sphdraw(bv[bp]);
	
    glColor3f(0.0, 1.0, 0.0);
    drawLines(gp, gv);
    sphdraw(gv[gp]);

    glColor3f(1.0, 0.0, 1.0);
    drawLines(yp, yv);
    sphdraw(yv[yp]);

    glColor3f(0.0, 1.0, 1.0);
    drawLines(mp, mv);
    sphdraw(mv[mp]);

    glutSwapBuffers();
}

void move_eye(void)
{
    /* first move the eye */
    eyev[0] += gravity * (rv[rp][0] - eyex[0]);
    eyev[1] += gravity * (rv[rp][1] - eyex[1]);
    eyev[2] += gravity * (rv[rp][2] - eyex[2]);

    /* adjust position using new velocity */
    eyex[0] += eyev[0] * dt;
    eyex[1] += eyev[1] * dt;
    eyex[2] += eyev[2] * dt;

    /* move the lookat point */
    /* it catches up to the red point if it's moving slowly enough */
    eyel[0] += LG * (rv[rp][0] - eyel[0]);
    eyel[1] += LG * (rv[rp][1] - eyel[1]);
    eyel[2] += LG * (rv[rp][2] - eyel[2]);

    /* change view */
    gluLookAt(eyex[0], eyex[1], eyex[2], eyel[0], eyel[1], eyel[2],
	      0, 1, 0);
}

void draw_hexcube(void)
{

    a += da;
    if(a >= 720.)		/* depends on slowest rotation factor */
	a = 0.;

    /* draw hexplanes, without changing z-values */
    glDepthMask(GL_FALSE); 
    glDisable(GL_DEPTH_TEST);

    /* x-y plane */
    glColor3f(0.2, 0.2, 0.6);
    glPushMatrix();
    glTranslatef(cubeoffx, cubeoffy, cubeoffz);
    glScalef(CUBESCALE, CUBESCALE, CUBESCALE);
    draw_hexplane();
    glPopMatrix();

    /* x-y plane, translated */
    glPushMatrix();
    glTranslatef(cubeoffx, cubeoffy, cubeoffz - 2*CUBESIDE);
    glScalef(CUBESCALE, CUBESCALE, CUBESCALE);
    draw_hexplane();
    glPopMatrix();

    glColor3f(0.6, 0.2, 0.2);
    /* x-z plane, translate low */
    glPushMatrix();
    glRotatef(90, 1.0, 0.0, 0.0);
    glTranslatef(cubeoffx, cubeoffz - CUBESIDE, -cubeoffy + CUBESIDE);
    glScalef(CUBESCALE, CUBESCALE, CUBESCALE);
    draw_hexplane();
    glPopMatrix();

    /* x-z plane, translate high */
    glPushMatrix();
    glRotatef(90, 1.0, 0.0, 0.0);
    glTranslatef(cubeoffx, cubeoffz - CUBESIDE, -cubeoffy - CUBESIDE);
    glScalef(CUBESCALE, CUBESCALE, CUBESCALE);
    draw_hexplane();
    glPopMatrix();

    glColor3f(0.2, 0.6, 0.2);
    /* y-z plane, translate low */
    glPushMatrix();
    glRotatef(90, 0.0, 1.0, 0.0);
    glTranslatef(-cubeoffz + CUBESIDE, cubeoffy, cubeoffx + CUBESIDE);
    glScalef(CUBESCALE, CUBESCALE, CUBESCALE);
    draw_hexplane();
    glPopMatrix();
	
    /* y-z plane, translate high */
    glPushMatrix();
    glRotatef (90, 0.0, 1.0, 0.0);
    glTranslatef(-cubeoffz + CUBESIDE, cubeoffy, cubeoffx - CUBESIDE);
    glScalef(CUBESCALE, CUBESCALE, CUBESCALE);
    draw_hexplane();
    glPopMatrix();

    glFlush();
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

float hex_data[8][3] =  {
    {0., 0., 0.},
    {1.155, 0., 0.},
    {0.577, 1., 0.},
    {-0.577, 1., 0.},
    {-1.155, 0., 0.},
    {-0.577, -1., 0.},
    {0.577, -1., 0.},
    {1.155, 0., 0.},
};

/* draws a hexagon 2 units across, in the x-y plane, */
/* centered at <0, 0, 0> */

void draw_hexagon(void)
{
    if(wflag) {
	glPushMatrix();
	glRotatef(a, 0.0, 0.0, 1.0);
    }

    glBegin(GL_TRIANGLE_FAN);
	glVertex3fv(hex_data[0]);
	glVertex3fv(hex_data[1]);
	glVertex3fv(hex_data[2]);
	glVertex3fv(hex_data[3]);
	glVertex3fv(hex_data[4]);
	glVertex3fv(hex_data[5]);
	glVertex3fv(hex_data[6]);
	glVertex3fv(hex_data[7]);
    glEnd();

    if(wflag)
	glPopMatrix();
}

void tmp_draw_hexplane(void)
{
    glRectf(-2.0, -2.0, 2.0, 2.0);
}

/* draw 7 hexagons */
void draw_hexplane(void)
{
    if(wflag) {
	glPushMatrix();
	glRotatef(-0.5*a, 0.0, 0.0, 1.0);
    }

    /* center , <0, 0, 0> */
    draw_hexagon();

    /* 12 o'clock, <0, 4, 0> */
    glTranslatef(0., 4., 0.);
    draw_hexagon();

    /* 10 o'clock, <-3.464, 2, 0> */
    glTranslatef(-3.464, -2., 0.);
    draw_hexagon();

    /* 8 o'clock, <-3.464, -2, 0> */
    glTranslatef(0., -4., 0.);
    draw_hexagon();

    /* 6 o'clock, <0, -4, 0> */
    glTranslatef(3.464, -2., 0.);
    draw_hexagon();

    /* 4 o'clock, <3.464, -2, 0> */
    glTranslatef(3.464, 2., 0.);
    draw_hexagon();

    /* 2 o'clock, <3.464, 2, 0> */
    glTranslatef(0., 4., 0.);
    draw_hexagon();

    if(wflag)
	glPopMatrix();
}

void sphdraw(float args[3])
{
    glPushMatrix();
    glTranslatef(args[0], args[1], args[2]);
    glCallList(asphere);
    glPopMatrix();
}

void setPerspective(int angle, float aspect, float zNear, float zFar)
{
    glPushAttrib(GL_TRANSFORM_BIT);
    glMatrixMode(GL_PROJECTION);
    gluPerspective(angle * 0.1, aspect, zNear, zFar);
    glPopAttrib();
}

/* initialize global 3-vectors */
void init_3d(void)
{
    (void)srand48((long)time((time_t*)NULL));

    /* initialize colored points */
    rv[0][0] = (float)drand48() * 10.;
    rv[0][1] = (float)drand48() * 10.;
    rv[0][2] = (float)drand48() * 10. - 10.;

    bv[0][0] = rv[0][0] + (float)drand48()*5.;
    bv[0][1] = rv[0][1] + (float)drand48()*5.;
    bv[0][0] = rv[0][2] + (float)drand48()*5.;

    gv[0][0] = rv[0][0] + (float)drand48()*5.;
    gv[0][1] = rv[0][1] + (float)drand48()*5.;
    gv[0][0] = rv[0][2] + (float)drand48()*5.;

    yv[0][0] = rv[0][0] + (float)drand48()*5.;
    yv[0][1] = rv[0][1] + (float)drand48()*5.;
    yv[0][0] = rv[0][2] + (float)drand48()*5.;

    mv[0][0] = rv[0][0] + (float)drand48()*5.;
    mv[0][1] = rv[0][1] + (float)drand48()*5.;
    mv[0][0] = rv[0][2] + (float)drand48()*5.;

    /* initialize eye velocity */
    eyev[0] = eyev[1] = eyev[2] = 0.;
}


void init_graphics(void)
{
    int width = 600;
    int height = 600;

    xmax = width;
    ymax = height;
    glDrawBuffer(GL_BACK);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClearDepth(1.0);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glViewport(0, 0, xmax, ymax);
    setPerspective(fovy, (float)xmax/(float)ymax, 0.01, farplane);
    quadObj = gluNewQuadric();
    gluQuadricNormals(quadObj, GLU_NONE);
    asphere = glGenLists(1);
    glNewList(asphere, GL_COMPILE);
    gluSphere(quadObj, 0.3, 12, 8);
    glEndList();
}

extern char *optarg;
extern int optind, opterr;

#define USAGE "usage message: this space for rent\n"
void parse_args(int argc, char **argv)
{
    int c;

    hexflag = sflag = fflag = wflag = gflag = debug = FALSE;
    opterr = 0;

    while( (c = getopt(argc, argv, "Xhixs:f:w:g:")) != -1)
	switch(c) {
	  case 'X':
	    debug = TRUE;
	    break;
	  case 'h':
	    print_usage(argv[0]);
	    exit(1);	
	  case 'i':
	    print_info();
	    exit(1);	
	  case 'x':
	    hexflag = TRUE;
	    farplane = 300.;
	    break;
	  case 's':
	    sflag = TRUE;
	    speed = atoi(optarg);
	    if(speed < 0) {
		fprintf(stderr, "Use a small positive value for speed ('s').\n");
		fprintf(stderr, "Try %s -h for help\n", argv[0]);
		exit(1);
	    }
	    break;
	  case 'f':
	    fflag = TRUE;
	    frame = atoi(optarg);
	    if(frame < 0) {
		fprintf(stderr, "Try a small positive value for \n");
		fprintf(stderr, "'f'; this is the number of vertical ");
		fprintf(stderr, "retraces per redraw\n");
		fprintf(stderr, "Try %s -h for help\n", argv[0]);
		exit(1);
	    }
	    break;
	  case 'w':
	    wflag = TRUE;
	    da = atof(optarg);
	    if(da > 10.) {
		fprintf(stderr, "That's a large rotational velocity ('w')");
		fprintf(stderr, " but you asked for it\n");
	    }
	    break;
	  case 'g':
	    gflag = TRUE;
	    gravity = atof(optarg);
	    if(gravity <= 0) {
		fprintf(stderr, "Gravity ('g') should be positive\n");
		fprintf(stderr, "Try %s -h for help\n", argv[0]);
	    }
	    break;
	  case '?':
	    fprintf(stderr, USAGE);
	}

    /* set up default values */
    if(!sflag)
	speed = 3;
    if(!fflag)
	frame = 2;	
    if(!wflag)
	da = 0.;
    if(!gflag)
	gravity = G;
}

void print_usage(char *program)
{
printf("\nUsage: %s [-h] [-i] [-x] [-s speed]", program);
printf(" [-w rot_v] [-g gravity]\n\n");
printf("-h              Print this message.\n");
printf("-i              Print information about the demo.\n");
printf("-x              Enclose the particles in a box made of hexagons.\n");
printf("-s speed        Sets the number of new line segments per redraw \n");
printf("                interval per line. Default value: 3.\n");

/*** The X port does not currently include a timer, so this feature is disabled.
printf("-f framenoise   Sets the number of vertical retraces per redraw\n");
printf("                interval. Example: -f 2 specifies one redraw per\n");
printf("                2 vertical retraces, or 30 frames per second.\n");
printf("                Default value: 2.\n");
************/

printf("-w rot_v        Spins the hexagons on their centers, and the sides\n");
printf("                of the box on their centers. Hexagons spin at the\n");
printf("                rate rot_v degrees per redraw, and box sides spin\n");
printf("                at -rot_v/2 degrees per redraw.\n");
printf("-g gravity      Sets the strength of the attraction of the eye to\n");
printf("                the red particle. Actually, it's not gravity since\n");
printf("                the attraction is proportionate to distance.\n");
printf("                Default value: 0.002. Try large values!\n");
/* input added for GLX port */
printf(" Executions control:  \n");
printf("    <spacebar>	step through single frames\n");
printf("    g		begin continuous frames\n");
printf("    s		stop continuous frames\n");

}

void print_info(void)
{
printf("\nLORENZ ATTRACTOR DEMO\n\n");
printf("This program shows some particles stuck in a Lorenz attractor (the \n");
printf("parameters used are r=28, b=8/3, sigma=10). The eye is attracted to \n");
printf("the red particle, with a force directly proportional to distance. \n");
printf("A command line argument puts the particles inside a box made of hexagons, \n");
printf("helping  to maintain the sense of 3 dimensions, but it can slow things down.\n");
printf("Other options allow you to play with the redraw rate and gravity.\n\n");

printf("Try lorenz -h for the usage message.\n");
}
