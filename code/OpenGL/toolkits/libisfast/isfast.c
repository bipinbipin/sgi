/*****************************************************************************
 * isfast - routines for subjectively assessing performance of selected
 *	    OpenGL drawing operations
 *
 * Overview:
 *
 *	The main entry points (ImmediateModeIsFast, etc.) each run two
 *	tests:  a baseline test, and a second test that uses the
 *	attributes or rendering mode of interest.  They compare the
 *	performance of the two tests, and (usually) conclude the mode
 *	of interest ``is fast'' if it's at least half as fast as the
 *	baseline test.
 *
 *	To run a test, the main entry points call RunTest().  RunTest()
 *	uses the performance database library (libpdb) to check the
 *	database for the desired test result.  If the result is already
 *	in the database, then that previous result is just returned.
 *	Otherwise, RunTest() calls Measure() to measure the performance
 *	of the desired operation, and then records the result in the
 *	database for future use.
 *
 *	Measure() opens a window for OpenGL drawing and initializes the
 *	OpenGL rendering context with some useful defaults.  It then
 *	invokes the test routine supplied by RunTest(), which saves the
 *	measured performance in a global variable.  Finally, it closes
 *	the window and returns to RunTest().
 *
 *	The individual test routines (BaseTriangleTest, for example)
 *	that are invoked by Measure() just set any additional OpenGL
 *	state that they need, clear the drawing buffers, and call the
 *	libpdb routine pdbMeasureRate() to measure the performance of
 *	a drawing routine.  Most of the time, they store the primitives
 *	in a display list, and the drawing routine just calls that
 *	display list.  (This yields more meaningful results than
 *	immediate-mode drawing when the test machine is being accessed
 *	across a network.)  So the usual division of labor is that the
 *	test routine performs one-time setup of state information, and
 *	the drawing routine performs all the repeated rendering.
 *
 *	One subtlety:  libpdb needs to calibrate its measuring routines.
 *	This calibration can take a significant percentage of the time
 *	required to run isfast, so each of the test routines takes care
 *	to ensure that calibration is performed only once.
 *
 * History:
 *
 *	1.0	9/93	akin	Written.  See accompanying README for
 *				rationale and examples.
 *	2.0	5/95	akin	Heavily modified.  Eliminated dependence
 *				on toolkits, made calibration operation
 *				less frequent (thus speeding up testing),
 *				used new pdb which includes support for
 *				versioning, and restructured tests to make
 *				better use of display lists (so that running
 *				remotely yields more useful results).
 *	2.1	7/8/95	akin	Added line antialiasing.
 *****************************************************************************/


#include <stdio.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include "pdb.h"
#include "isfast.h"


static double	AALineRate = 0.0;
static double	BaseLineRate = 0.0;
static double	BaseTriangleRate = 0.0;
static double	ImmediateModeRate = 0.0;
static double	NoDepthBufferRate = 0.0;
static double	StencillingRate = 0.0;
static double	TextureMappingRate = 0.0;
static int	Calibrate = 1;
static char*	Version = NULL;
static char*	ServerName = NULL;

static const char* cIsFast = "IsFast";
static const char* cBaseLine = "BaseLine";
static const char* cBaseTriangle = "BaseTriangle";


static void	AALineTest		(void);
static void	BaseLineTest		(void);
static void	BaseTriangleTest	(void);
static void	DrawBaseLines		(void);
static void	DrawBaseTriangles	(void);
static void	DrawDisplayList		(void);
static void	ImmediateModeTest	(void);
static int	Measure			(const char*	windowType,
					 const char*	windowTitle,
					 void		(*testFunc)(void));
static void	NoDepthBufferTest	(void);
static int	RunTest			(void		(*testFunction)(void),
					 const char*	testName,
					 double*	rate,
					 char*		windowType);
static void	StencillingTest		(void);
static void	TextureMappingTest	(void);


static const GLfloat Vertex[] = {
	 0.9,  0.0,
	 0.85, 0.1,
	 0.8,  0.0,
	 0.75, 0.1,
	 0.7,  0.0,
	 0.65, 0.1,
	 0.6,  0.0,
	 0.55, 0.1,
	 0.5,  0.0,
	 0.45, 0.1,
	 0.4,  0.0,
	 0.35, 0.1,
	 0.3,  0.0,
	 0.25, 0.1,
	 0.2,  0.0,
	 0.15, 0.1,
	 0.1,  0.0,
	 0.05, 0.1,
	 0.0,  0.0,
	-0.05, 0.1,
	-0.1,  0.0,
	-0.15, 0.1,
	-0.2,  0.0,
	-0.25, 0.1,
	-0.3,  0.0,
	-0.35, 0.1,
	-0.4,  0.0,
	-0.45, 0.1,
	-0.5,  0.0,
	-0.55, 0.1,
	-0.6,  0.0,
	-0.65, 0.1,
	-0.7,  0.0,
	-0.75, 0.1,
	-0.8,  0.0,
	-0.85, 0.1,
	-0.9,  0.0
	};


/*****************************************************************************
 * AALineTest - measure drawing rate for antialiased lines
 *****************************************************************************/

static void
AALineTest() {
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	glShadeModel(GL_FLAT);

	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_FASTEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glNewList(1, GL_COMPILE);
		DrawBaseLines();
	glEndList();

	pdbMeasureRate(glFinish, DrawDisplayList, glFinish, Calibrate,
		&AALineRate);
	Calibrate = 0;

	glDeleteLists(1, 1);
	}


/*****************************************************************************
 * BaseLineTest - measure drawing rate for baseline lines
 *****************************************************************************/

static void
BaseLineTest() {
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	glShadeModel(GL_FLAT);

	glNewList(1, GL_COMPILE);
		DrawBaseLines();
	glEndList();

	pdbMeasureRate(glFinish, DrawDisplayList, glFinish, Calibrate,
		&BaseLineRate);
	Calibrate = 0;

	glDeleteLists(1, 1);
	}


/*****************************************************************************
 * BaseTriangleTest - measure drawing rate for baseline triangles
 *****************************************************************************/

static void
BaseTriangleTest() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);		/* force some drawing to occur */

	glNewList(1, GL_COMPILE);
		DrawBaseTriangles();
	glEndList();

	pdbMeasureRate(glFinish, DrawDisplayList, glFinish, Calibrate,
		&BaseTriangleRate);
	Calibrate = 0;

	glDeleteLists(1, 1);
	}


/*****************************************************************************
 * DepthBufferingIsFast - see if depth-buffered triangles are at least half as
 *			  fast as triangles without depth-buffering
 *****************************************************************************/

int
DepthBufferingIsFast(void) {
	if (!RunTest(BaseTriangleTest, cBaseTriangle, &BaseTriangleRate,
	    "max rgb, max z"))
		return 0;

	if (!RunTest(NoDepthBufferTest, "NoDepthBuffer", &NoDepthBufferRate,
	    "max rgb"))
		return 0;

	return BaseTriangleRate > 0.5 * NoDepthBufferRate;
	}


/*****************************************************************************
 * DrawBaseLines - draw a simple line strip
 *
 * The caller must enable the appropriate options, e.g. lighting, depth
 * buffering, texturing.
 *****************************************************************************/

static void
DrawBaseLines(void) {
	int i;

	glNormal3f(0.0, 0.0, 1.0);
	glBegin(GL_LINE_STRIP);
		for (i = 0; i < sizeof(Vertex) / sizeof(Vertex[0]); i += 2)
			glVertex2fv(Vertex + i);
	glEnd();
	}


/*****************************************************************************
 * DrawBaseTriangles - draw a simple triangle strip
 *
 * The caller must enable the appropriate options, e.g. lighting, depth
 * buffering, texturing.
 *****************************************************************************/

static void
DrawBaseTriangles(void) {
	int i;

	glNormal3f(0.0, 0.0, 1.0);
	glBegin(GL_TRIANGLE_STRIP);
		for (i = 0; i < sizeof(Vertex) / sizeof(Vertex[0]); i += 2)
			glVertex2fv(Vertex + i);
	glEnd();
	}


/*****************************************************************************
 * DrawDisplayList - draw display list created by DisplayListTest
 *****************************************************************************/

static void
DrawDisplayList(void) {
	glCallList(1);
	}


/*****************************************************************************
 * ImmediateModeIsFast - see if immediate-mode triangles are at least half as
 *			 fast as display-listed triangles
 *****************************************************************************/

int
ImmediateModeIsFast(void) {
	if (!RunTest(BaseTriangleTest, cBaseTriangle, &BaseTriangleRate,
	    "max rgb, max z"))
		return 0;

	if (!RunTest(ImmediateModeTest, "ImmediateMode", &ImmediateModeRate,
	    "max rgb, max z"))
		return 0;
	
	return ImmediateModeRate > 0.5 * BaseTriangleRate;
	}


/*****************************************************************************
 * ImmediateModeTest - measure drawing rate for immediate-mode triangle strip
 *****************************************************************************/

static void
ImmediateModeTest() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);		/* force some drawing to occur */

	pdbMeasureRate(glFinish, DrawBaseTriangles, glFinish, Calibrate,
		&ImmediateModeRate);
	Calibrate = 0;
	}


/*****************************************************************************
 * LineAAIsFast - see if antialiased lines are at least half as fast as
 *		  ordinary aliased lines
 *****************************************************************************/

int
LineAAIsFast(void) {
	if (!RunTest(BaseLineTest, cBaseLine, &BaseLineRate, "max rgb"))
		return 0;

	if (!RunTest(AALineTest, "AALine", &AALineRate, "max rgb"))
		return 0;

	return AALineRate > 0.5 * BaseLineRate;
	}


/*****************************************************************************
 * Measure - create a window, set default OpenGL state, and run a test
 *****************************************************************************/

static int
Measure (
    const char*	windowType,
    const char*	windowTitle,
    void	(*testFunc)(void)
    ) {
	static const GLfloat diffuse[] = {0.5, 0.5, 0.5, 1.0};
	static const GLfloat specular[] = {0.5, 0.5, 0.5, 1.0};
	static const GLfloat direction[] = {1.0, 1.0, 1.0, 0.0};
	static const GLfloat matAmbient[] = {0.1, 0.1, 0.1, 1.0};
	static const GLfloat matSpecular[] = {0.5, 0.5, 0.5, 1.0};

	if (!IsFastOpenWindow(windowType, windowTitle))
		return 0;

	glClearDepth(1.0);
	glClearStencil(0);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
	glLightfv(GL_LIGHT0, GL_POSITION, direction);
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);

	glMaterialfv(GL_FRONT, GL_AMBIENT, matAmbient);
	glMaterialfv(GL_FRONT, GL_SPECULAR, matSpecular);
	glMateriali(GL_FRONT, GL_SHININESS, 128);
	glEnable(GL_COLOR_MATERIAL);
	glShadeModel(GL_SMOOTH);

	glMatrixMode(GL_PROJECTION);
	gluPerspective(45.0, 1.0, 2.4, 4.6);
	glMatrixMode(GL_MODELVIEW);

	gluLookAt(0.0,0.0,3.5, 0.0,0.0,0.0, 0.0,1.0,0.0);

	(*testFunc)();

	IsFastCloseWindow();
	return 1;
	}


/*****************************************************************************
 * NoDepthBufferTest - draw triangles without depth buffering
 *****************************************************************************/

static void
NoDepthBufferTest() {
	glClear(GL_COLOR_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);

	glNewList(1, GL_COMPILE);
		DrawBaseTriangles();
	glEndList();

	pdbMeasureRate(glFinish, DrawDisplayList, glFinish, Calibrate,
		&NoDepthBufferRate);
	Calibrate = 0;

	glDeleteLists(1, 1);
	}


/*****************************************************************************
 * RunTest - if performance data for a given test is in the database, return
 *	     it; otherwise create a window, measure the test's drawing rate,
 *	     and save the result in the database
 *****************************************************************************/

static int
RunTest (
    void	(*testFunction)(void),
    const char*	testName,
    double*	rate,
    char*	windowType
    ) {
	if (pdbReadRate(ServerName, cIsFast, testName, Version, rate)
	    == PDB_NO_ERROR)
		return 1;

	if (!Measure(windowType, testName, testFunction))
		/* testFunction sets ``rate'' implicitly */
		return 0;
	
	pdbWriteRate(ServerName, cIsFast, testName, Version, *rate);

	return 1;
	}


/*****************************************************************************
 * StencillingIsFast - see if stencilled triangles are at least half as fast
 *		       as non-stencilled triangles
 *****************************************************************************/

int
StencillingIsFast(void) {
	if (!RunTest(BaseTriangleTest, cBaseTriangle, &BaseTriangleRate,
	    "max rgb, max z"))
		return 0;

	if (!RunTest(StencillingTest, "Stencilling", &StencillingRate,
	    "max rgb, max z, max s"))
		return 0;

	return StencillingRate > 0.5 * BaseTriangleRate;
	}


/*****************************************************************************
 * StencillingTest - draw triangles with nontrivial stencil operations
 *****************************************************************************/

static void
StencillingTest() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
	    | GL_STENCIL_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glNewList(1, GL_COMPILE);
		DrawBaseTriangles();
	glEndList();

	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_EQUAL, 1, 1);
	glStencilOp(GL_INVERT, GL_INVERT, GL_INVERT);

	pdbMeasureRate(glFinish, DrawDisplayList, glFinish, Calibrate,
		&StencillingRate);
	Calibrate = 0;

	glDeleteLists(1, 1);
	}


/*****************************************************************************
 * TextureMappingIsFast - see if texture-mapped triangles are at least half as
 *			  fast as ordinary shaded triangles
 *****************************************************************************/

int
TextureMappingIsFast(void) {
	if (!RunTest(BaseTriangleTest, cBaseTriangle, &BaseTriangleRate,
	    "max rgb, max z"))
		return 0;

	if (!RunTest(TextureMappingTest, "TextureMapping", &TextureMappingRate,
	    "max rgb, max z"))
		return 0;

	return TextureMappingRate > 0.5 * BaseTriangleRate;
	}


/*****************************************************************************
 * TextureMappingTest - draw baseline triangles with texture mapping
 *****************************************************************************/

static void
TextureMappingTest() {
	GLubyte texture[8][8][3];
	int i;
	int j;
	int c;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	for (i = 0; i < 8; ++i)
		for (j = 0; j < 8; ++j) {
			c = ((i & 0x1) ^ (j & 0x1))? 255: 0;
			texture[i][j][0] = c;
			texture[i][j][1] = texture[i][j][2] = 0;
			}

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glEnable(GL_TEXTURE_2D);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, 8, 8, GL_RGB,
	    GL_UNSIGNED_BYTE, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	    GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
	    GL_NEAREST);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

	glTexGenf(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glTexGenf(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef(4.0, 4.0, 1.0);
	glMatrixMode(GL_MODELVIEW);

	glNewList(1, GL_COMPILE);
		DrawBaseTriangles();
	glEndList();

	pdbMeasureRate(glFinish, DrawDisplayList, glFinish, Calibrate,
		&TextureMappingRate);
	Calibrate = 0;

	glDeleteLists(1, 1);
	}



/*****************************************************************************
 * Window-system-specific code for isfast
 *
 * This code isolates window system initialization, window
 * creation/deletion, and OpenGL context creation/deletion.
 *****************************************************************************/

#ifdef X11

/* X11 Version */
#include <strings.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmu/StdCmap.h>
#include "visinfo.h"


static Display* Dpy = NULL;
static int Scr;
static Window ParentWin;
static Window Win;
static XVisualInfo Vis;
static GLXContext RC;


static Colormap FindColormap(void);
static Bool WaitForExposure(Display* d, XEvent* e, char* arg);
static int OpenWindow(const char* attributes, const char* title, int mapped);


int
IsFastXOpenDisplay(const char* displayName) {
	Dpy = XOpenDisplay(displayName);
	if (!Dpy)
		return 0;

	if (!glXQueryExtension(Dpy, NULL, NULL)) {
		XCloseDisplay(Dpy);
		Dpy = NULL;
		return 0;
		}

	Scr = DefaultScreen(Dpy);
	ParentWin = RootWindow(Dpy, Scr);

	if (displayName)
		ServerName = strdup(displayName);
	else
		ServerName = NULL;
	pdbOpen();
	Calibrate = 1;		/* request calibration on first measurement */
	if (OpenWindow("max rgb", "Checking version...", 0)) {
		Version = strdup((const char *)glGetString(GL_VERSION));
		IsFastCloseWindow();
		}
	else
		Version = strdup("VERSION?");

	return 1;
	}

void
IsFastXCloseDisplay(void) {
	if (Dpy) {
		XCloseDisplay(Dpy);
		Dpy = NULL;
		Scr = 0;
		ParentWin = None;
		Win = None;
		if (ServerName) {
			free(ServerName);
			ServerName = NULL;
			}
		if (Version) {
			free(Version);
			Version = NULL;
			}
		pdbClose();
		}
	}


int
IsFastOpenWindow(const char* attributes, const char* title) {
	return OpenWindow(attributes, title, 1);
	}


static int
OpenWindow(const char* attributes, const char* title, int mapped) {
	XVisualInfo* vInfo;
	int nVInfo;
	XSetWindowAttributes xswa;
	int x = 0, y = 0, w = 256, h = 256;
	XEvent event;

	if (!visPixelFormat(attributes))
		return 0;	/* syntax error in Visual criteria */

	vInfo = visGetGLXVisualInfo(Dpy, Scr, &nVInfo);
	if (nVInfo == 0)
		return 0;	/* no matching Visuals */
	Vis = *vInfo;
	free(vInfo);

	RC = glXCreateContext(Dpy, &Vis, 0, GL_TRUE);
	if (!RC)
		return 0;

	xswa.border_pixel = 0;
	xswa.colormap = FindColormap();
	xswa.event_mask = ExposureMask;
	Win = XCreateWindow(Dpy, ParentWin, x, y, w, h, 0, Vis.depth,
		InputOutput, Vis.visual,
		CWColormap|CWEventMask|CWBorderPixel, &xswa);
	if (Win == None)
		return 0;

	if (ParentWin == RootWindow(Dpy, Scr)) {
		Atom wmDeleteWindow;
		XSizeHints sh;

		wmDeleteWindow = XInternAtom(Dpy, "WM_DELETE_WINDOW", False);
		XSetWMProtocols(Dpy, Win, &wmDeleteWindow, 1);

		sh.x = x;
		sh.y = y;
		sh.width = w;
		sh.height = h;
		sh.flags = USPosition | USSize;
		XSetStandardProperties(Dpy, Win, title, title, None, NULL, 0,
			&sh);
		}

	if (mapped) {
		XMapWindow(Dpy, Win);
		XIfEvent(Dpy, &event, WaitForExposure, (char*) Win);
		}

	glXMakeCurrent(Dpy, Win, RC);
	glViewport(x, y, w, h);

	return 1;
	}


void
IsFastCloseWindow(void) {
	glXMakeCurrent(Dpy, None, NULL);
	glXDestroyContext(Dpy, RC);
	XDestroyWindow(Dpy, Win);
	}


static Colormap
FindColormap(void) {
        Status status;
        XStandardColormap* standardCmaps;
        int i;
        int nCmaps;
	Colormap cmap;
                
        if (Vis.class != TrueColor)
                return None;
        status = XmuLookupStandardColormap(Dpy, Scr, Vis.visualid,
                Vis.depth, XA_RGB_DEFAULT_MAP, False, True);
        if (status == 1) {
                status = XGetRGBColormaps(Dpy, RootWindow(Dpy, Scr),
                        &standardCmaps, &nCmaps, XA_RGB_DEFAULT_MAP);
                if (status == 1) {
                        for (i = 0; i < nCmaps; ++i)
                                if (standardCmaps[i].visualid == Vis.visualid) {
					cmap = standardCmaps[i].colormap;
                                        XFree(standardCmaps);
                                        return cmap;
                                        }
                        }
                }

        cmap = XCreateColormap(Dpy, ParentWin, Vis.visual, AllocNone);
	return cmap;
	}


static Bool
WaitForExposure(Display* d, XEvent* e, char* arg) {
	return (e->type == Expose)
		&& (e->xexpose.window == (Window) arg)
		&& (e->xexpose.count == 0);
	}

#endif	/* X11 */



/* This space reserved for window-system-specific code of your choice... */
