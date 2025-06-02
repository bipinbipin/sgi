/*
 * surfgrid.c - simple test of polygon offset 1.1
 *
 * usage:
 *	surfgrid [-f]
 *
 * options:
 *	-f	run on full screen
 *
 * keys:
 *	o	polygon offset off 
 *      p       polygon offset on by pushing polygons away from lines
 *      l       polygon offset on by pulling lines away from polygons
 *      F       increase polygon offset factor
 *      f       decrease polygon offset factor
 *      B       increase polygon offset units
 *      b       decrease polygon offset units
 *	g	toggle grid drawing
 *	s	toggle smooth/flat shading
 *	n	toggle whether to use GL evaluators or GLU nurbs
 *	u	decr number of segments in U direction
 *	U	incr number of segments in U direction
 *	v	decr number of segments in V direction
 *	V	incr number of segments in V direction
 *      y       print factor & units
 *      h,?     this help message
 *	Esc	quit
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glut.h>


GLenum err;
#define CHECK_ERROR \
	err = glGetError(); \
	if (err) { \
	    printf("Error %d at line %d\n", err, __LINE__); \
	}

#define W 600
#define H 600


static GLfloat controlpts[] = {
	 4.0, 0.0, 0.0, 4.0, 2.0, 0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0,
         3.0, 0.0, 1.0, 2.0, 4.0, 0.0, 1.0, 2.0, 8.0, 0.0, 0.0, 4.0,
         8.0, 0.0, 0.0, 4.0, 4.0, 0.0,-1.0, 2.0, 3.0, 0.0,-1.0, 2.0,
         3.0, 0.0,-1.0, 2.0, 2.0, 0.0,-1.0, 2.0, 4.0, 0.0, 0.0, 4.0,
         2.0,-2.0, 0.0, 2.0, 1.0,-1.0, 0.5, 1.0, 1.5,-1.5, 0.5, 1.0,
         1.5,-1.5, 0.5, 1.0, 2.0,-2.0, 0.5, 1.0, 4.0,-4.0, 0.0, 2.0,
         4.0,-4.0, 0.0, 2.0, 2.0,-2.0,-0.5, 1.0, 1.5,-1.5,-0.5, 1.0,
         1.5,-1.5,-0.5, 1.0, 1.0,-1.0,-0.5, 1.0, 2.0,-2.0, 0.0, 2.0,
         0.0,-2.0, 0.0, 2.0, 0.0,-1.0, 0.5, 1.0, 0.0,-1.5, 0.5, 1.0,
         0.0,-1.5, 0.5, 1.0, 0.0,-2.0, 0.5, 1.0, 0.0,-4.0, 0.0, 2.0,
         0.0,-4.0, 0.0, 2.0, 0.0,-2.0,-0.5, 1.0, 0.0,-1.5,-0.5, 1.0,
         0.0,-1.5,-0.5, 1.0, 0.0,-1.0,-0.5, 1.0, 0.0,-2.0, 0.0, 2.0,
         0.0,-2.0, 0.0, 2.0, 0.0,-1.0, 0.5, 1.0, 0.0,-1.5, 0.5, 1.0,
         0.0,-1.5, 0.5, 1.0, 0.0,-2.0, 0.5, 1.0, 0.0,-4.0, 0.0, 2.0,
         0.0,-4.0, 0.0, 2.0, 0.0,-2.0,-0.5, 1.0, 0.0,-1.5,-0.5, 1.0,
         0.0,-1.5,-0.5, 1.0, 0.0,-1.0,-0.5, 1.0, 0.0,-2.0, 0.0, 2.0,
        -2.0,-2.0, 0.0, 2.0,-1.0,-1.0, 0.5, 1.0,-1.5,-1.5, 0.5, 1.0,
        -1.5,-1.5, 0.5, 1.0,-2.0,-2.0, 0.5, 1.0,-4.0,-4.0, 0.0, 2.0,
        -4.0,-4.0, 0.0, 2.0,-2.0,-2.0,-0.5, 1.0,-1.5,-1.5,-0.5, 1.0,
        -1.5,-1.5,-0.5, 1.0,-1.0,-1.0,-0.5, 1.0,-2.0,-2.0, 0.0, 2.0,
        -4.0, 0.0, 0.0, 4.0,-2.0, 0.0, 1.0, 2.0,-3.0, 0.0, 1.0, 2.0,
        -3.0, 0.0, 1.0, 2.0,-4.0, 0.0, 1.0, 2.0,-8.0, 0.0, 0.0, 4.0,
        -8.0, 0.0, 0.0, 4.0,-4.0, 0.0,-1.0, 2.0,-3.0, 0.0,-1.0, 2.0,
        -3.0, 0.0,-1.0, 2.0,-2.0, 0.0,-1.0, 2.0,-4.0, 0.0, 0.0, 4.0,
        -4.0, 0.0, 0.0, 4.0,-2.0, 0.0, 1.0, 2.0,-3.0, 0.0, 1.0, 2.0,
        -3.0, 0.0, 1.0, 2.0,-4.0, 0.0, 1.0, 2.0,-8.0, 0.0, 0.0, 4.0,
        -8.0, 0.0, 0.0, 4.0,-4.0, 0.0,-1.0, 2.0,-3.0, 0.0,-1.0, 2.0,
        -3.0, 0.0,-1.0, 2.0,-2.0, 0.0,-1.0, 2.0,-4.0, 0.0, 0.0, 4.0,
        -2.0, 2.0, 0.0, 2.0,-1.0, 1.0, 0.5, 1.0,-1.5, 1.5, 0.5, 1.0,
        -1.5, 1.5, 0.5, 1.0,-2.0, 2.0, 0.5, 1.0,-4.0, 4.0, 0.0, 2.0,
        -4.0, 4.0, 0.0, 2.0,-2.0, 2.0,-0.5, 1.0,-1.5, 1.5,-0.5, 1.0,
        -1.5, 1.5,-0.5, 1.0,-1.0, 1.0,-0.5, 1.0,-2.0, 2.0, 0.0, 2.0,
         0.0, 2.0, 0.0, 2.0, 0.0, 1.0, 0.5, 1.0, 0.0, 1.5, 0.5, 1.0,
         0.0, 1.5, 0.5, 1.0, 0.0, 2.0, 0.5, 1.0, 0.0, 4.0, 0.0, 2.0,
         0.0, 4.0, 0.0, 2.0, 0.0, 2.0,-0.5, 1.0, 0.0, 1.5,-0.5, 1.0,
         0.0, 1.5,-0.5, 1.0, 0.0, 1.0,-0.5, 1.0, 0.0, 2.0, 0.0, 2.0,
         0.0, 2.0, 0.0, 2.0, 0.0, 1.0, 0.5, 1.0, 0.0, 1.5, 0.5, 1.0,
         0.0, 1.5, 0.5, 1.0, 0.0, 2.0, 0.5, 1.0, 0.0, 4.0, 0.0, 2.0,
         0.0, 4.0, 0.0, 2.0, 0.0, 2.0,-0.5, 1.0, 0.0, 1.5,-0.5, 1.0,
         0.0, 1.5,-0.5, 1.0, 0.0, 1.0,-0.5, 1.0, 0.0, 2.0, 0.0, 2.0,
         2.0, 2.0, 0.0, 2.0, 1.0, 1.0, 0.5, 1.0, 1.5, 1.5, 0.5, 1.0,
         1.5, 1.5, 0.5, 1.0, 2.0, 2.0, 0.5, 1.0, 4.0, 4.0, 0.0, 2.0,
         4.0, 4.0, 0.0, 2.0, 2.0, 2.0,-0.5, 1.0, 1.5, 1.5,-0.5, 1.0,
         1.5, 1.5,-0.5, 1.0, 1.0, 1.0,-0.5, 1.0, 2.0, 2.0, 0.0, 2.0,
         4.0, 0.0, 0.0, 4.0, 2.0, 0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0,
         3.0, 0.0, 1.0, 2.0, 4.0, 0.0, 1.0, 2.0, 8.0, 0.0, 0.0, 4.0,
         8.0, 0.0, 0.0, 4.0, 4.0, 0.0,-1.0, 2.0, 3.0, 0.0,-1.0, 2.0,
         3.0, 0.0,-1.0, 2.0, 2.0, 0.0,-1.0, 2.0, 4.0, 0.0, 0.0, 4.0,
};


static GLfloat nurbctlpts[] = {
         4.0, 0.0, 0.0, 4.0, 2.0, 0.0, 1.0, 2.0, 4.0, 0.0, 1.0, 2.0,
         8.0, 0.0, 0.0, 4.0, 4.0, 0.0,-1.0, 2.0, 2.0, 0.0,-1.0, 2.0,
         4.0, 0.0, 0.0, 4.0, 2.0,-2.0, 0.0, 2.0, 1.0,-1.0, 0.5, 1.0,
         2.0,-2.0, 0.5, 1.0, 4.0,-4.0, 0.0, 2.0, 2.0,-2.0,-0.5, 1.0,
         1.0,-1.0,-0.5, 1.0, 2.0,-2.0, 0.0, 2.0,-2.0,-2.0, 0.0, 2.0,
        -1.0,-1.0, 0.5, 1.0,-2.0,-2.0, 0.5, 1.0,-4.0,-4.0, 0.0, 2.0,
        -2.0,-2.0,-0.5, 1.0,-1.0,-1.0,-0.5, 1.0,-2.0,-2.0, 0.0, 2.0,
        -4.0, 0.0, 0.0, 4.0,-2.0, 0.0, 1.0, 2.0,-4.0, 0.0, 1.0, 2.0,
        -8.0, 0.0, 0.0, 4.0,-4.0, 0.0,-1.0, 2.0,-2.0, 0.0,-1.0, 2.0,
        -4.0, 0.0, 0.0, 4.0,-2.0, 2.0, 0.0, 2.0,-1.0, 1.0, 0.5, 1.0,
        -2.0, 2.0, 0.5, 1.0,-4.0, 4.0, 0.0, 2.0,-2.0, 2.0,-0.5, 1.0,
        -1.0, 1.0,-0.5, 1.0,-2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 0.0, 2.0,
         1.0, 1.0, 0.5, 1.0, 2.0, 2.0, 0.5, 1.0, 4.0, 4.0, 0.0, 2.0,
         2.0, 2.0,-0.5, 1.0, 1.0, 1.0,-0.5, 1.0, 2.0, 2.0, 0.0, 2.0,
         4.0, 0.0, 0.0, 4.0, 2.0, 0.0, 1.0, 2.0, 4.0, 0.0, 1.0, 2.0,
         8.0, 0.0, 0.0, 4.0, 4.0, 0.0,-1.0, 2.0, 2.0, 0.0,-1.0, 2.0,
         4.0, 0.0, 0.0, 4.0,
};


/*
 * Misc vector op routines.
 */

float x_axis[] = { 1.0, 0.0, 0.0 };
float y_axis[] = { 0.0, 1.0, 0.0 };
float z_axis[] = { 0.0, 0.0, 1.0 };
float nx_axis[] = { -1.0, 0.0, 0.0 };
float ny_axis[] = { 0.0, -1.0, 0.0 };
float nz_axis[] = { 0.0, 0.0, -1.0 };

void norm(float v[3])
{
    float r;

    r = sqrt( v[0]*v[0] + v[1]*v[1] + v[2]*v[2] );

    v[0] /= r;
    v[1] /= r;
    v[2] /= r;
}

float dot(float a[3], float b[3])
{
    return (a[0]*b[0] + a[1]*b[1] + a[2]*b[2]);
}

void cross(float v1[3], float v2[3], float result[3])
{
    result[0] = v1[1]*v2[2] - v1[2]*v2[1];
    result[1] = v1[2]*v2[0] - v1[0]*v2[2];
    result[2] = v1[0]*v2[1] - v1[1]*v2[0];
}

float length(float v[3])
{
    float r = sqrt( v[0]*v[0] + v[1]*v[1] + v[2]*v[2] );
    return r;
}


static long winwidth = W, winheight = H;
GLUnurbsObj *nobj;
GLuint surflist, gridlist;

int useglunurbs = 0;
int smooth = 1;
GLboolean tracking = GL_FALSE;
int showgrid = 1;
int showsurf = 1;
int fullscreen = 0;
float modelmatrix[16];
int usegments=4;
int vsegments=4;

static enum {
   POLYGON_OFFSET_OFF, 
   POLYGON_OFFSET_ON_PUSH_POLYGONS,
   POLYGON_OFFSET_ON_PULL_LINES 
} polygonOffsetMode = POLYGON_OFFSET_ON_PUSH_POLYGONS;
static float factor;
static float units;

int spindx, spindy;
int startx, starty;
int curx, cury;
int prevx, prevy; /* to get good deltas using glut */

void redraw(void);
void createlists(void);

float torusnurbpts[];
float torusbezierpts[];


void move(int x, int y)
{
    int dx, dy;
    int aspindx, aspindy;
    prevx = curx;
    prevy = cury;
    curx = x;
    cury = y;
    if (curx != startx || cury != starty) {
	redraw();
	startx = curx;
	starty = cury;
    }
}


void button(int button, int state, int x, int y)
{
    if(button != GLUT_LEFT_BUTTON)
	return;
    switch(state) {
    case GLUT_DOWN:
	prevx = curx = startx = x;
	prevy = cury = starty = y;
	spindx = 0;
	spindy = 0;
	tracking = GL_TRUE;
	break;
    case GLUT_UP:
	/*
	 * If user released the button while moving the mouse, keep
	 * spinning.
	 */
	if (x != prevx || y != prevy) {
	    spindx = x - prevx;
	    spindy = y - prevy;
	}
	tracking = GL_FALSE;
	break;
    }
}


/* Maintain a square window when resizing */
void reshape(int width, int height)
{
    int size;
    size = (width < height ? width : height);
    glViewport((width-size)/2, (height-size)/2, size, size);
    glutReshapeWindow(size, size);
    redraw();
}


void gridmaterials(void)
{
    static float front_mat_diffuse[] = { 1.0, 1.0, 0.4, 1.0 };
    static float front_mat_ambient[] = { 0.1, 0.1, 0.1, 1.0 };
    static float back_mat_diffuse[] = { 1.0, 0.0, 0.0, 1.0 };
    static float back_mat_ambient[] = { 0.1, 0.1, 0.1, 1.0 };

    glMaterialfv(GL_FRONT, GL_DIFFUSE, front_mat_diffuse);
    glMaterialfv(GL_FRONT, GL_AMBIENT, front_mat_ambient);
    glMaterialfv(GL_BACK, GL_DIFFUSE, back_mat_diffuse);
    glMaterialfv(GL_BACK, GL_AMBIENT, back_mat_ambient);
}

void surfacematerials(void)
{
    static float front_mat_diffuse[] = { 0.2, 0.7, 0.4, 1.0 };
    static float front_mat_ambient[] = { 0.1, 0.1, 0.1, 1.0 };
    static float back_mat_diffuse[] = { 1.0, 1.0, 0.2, 1.0 };
    static float back_mat_ambient[] = { 0.1, 0.1, 0.1, 1.0 };

    glMaterialfv(GL_FRONT, GL_DIFFUSE, front_mat_diffuse);
    glMaterialfv(GL_FRONT, GL_AMBIENT, front_mat_ambient);
    glMaterialfv(GL_BACK, GL_DIFFUSE, back_mat_diffuse);
    glMaterialfv(GL_BACK, GL_AMBIENT, back_mat_ambient);
}

void init(void)
{
    int i;
    static float ambient[] = { 0.0, 0.0, 0.0, 1.0 };
    static float diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
    static float position[] = { 90.0, 90.0, -150.0, 0.0 };
    static float lmodel_ambient[] = { 1.0, 1.0, 1.0, 1.0 };
    static float lmodel_twoside[] = { GL_TRUE };

    CHECK_ERROR

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective( 40.0, 1.0, 2.0, 12.0 );
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glGetFloatv(GL_MODELVIEW_MATRIX, modelmatrix);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_AUTO_NORMAL);

    if (polygonOffsetMode == POLYGON_OFFSET_ON_PUSH_POLYGONS) {
       glEnable(GL_POLYGON_OFFSET_FILL);
       glDisable(GL_POLYGON_OFFSET_LINE);

       factor = 1.0;
       units = 1.0;

       printf("Polygon offset on by pushing polygons away from viewer.\n");
       printf("Factor: %.2f Units: %.1f\n",factor,units);
    }
    else if (polygonOffsetMode == POLYGON_OFFSET_ON_PULL_LINES) {
       glDisable(GL_POLYGON_OFFSET_FILL);
       glEnable(GL_POLYGON_OFFSET_LINE);

       factor = -1.0;
       units = -1.0;

       printf("Polygon offset on by pulling lines towards viewer.\n");
       printf("Factor: %.2f Units: %.1f\n",factor,units);
    }
    else {
       assert(polygonOffsetMode == POLYGON_OFFSET_OFF);

       glDisable(GL_POLYGON_OFFSET_FILL);
       glDisable(GL_POLYGON_OFFSET_LINE);

       /* leave factor & units untouched */

       printf("Polygon offset off.\n");
    }
    glPolygonOffset(factor,units);

    glFrontFace(GL_CCW);

    glEnable( GL_MAP2_VERTEX_4 );
    glClearColor(0.25, 0.25, 0.5, 0.0);

    nobj = gluNewNurbsRenderer();
    gluNurbsProperty(nobj, GLU_SAMPLING_METHOD, GLU_DOMAIN_DISTANCE );

    surflist = glGenLists(1);
    gridlist = glGenLists(1);
    createlists();

    CHECK_ERROR
}

void drawmesh(void)
{
    int i, j;
    float *p;

    int up2p = 4;
    int uorder = 3, vorder = 3;
    int nu = 4, nv = 4;
    int vp2p = up2p * uorder * nu;

    for (j=0; j < nv; j++) {
	for (i=0; i < nu; i++) {
	    p = torusbezierpts + (j * vp2p * vorder) + (i * up2p * uorder);

	    glMap2f( GL_MAP2_VERTEX_4, 0.0, 1.0, up2p, 3, 0.0, 1.0, vp2p, 3,
		     (void*)p );

	    if (showsurf) {
		surfacematerials();

		glEnable(GL_LIGHTING);
		glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
		glEvalMesh2( GL_FILL, 0, usegments, 0, vsegments );
	    }
	    if (showgrid) {
                gridmaterials();

		glDisable(GL_LIGHTING);
		glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
		glEvalMesh2( GL_FILL, 0, usegments, 0, vsegments );
            }
	}
    }

    CHECK_ERROR
}

void redraw(void)
{
    static int i=0;
    int dx, dy;
    float v[3], n[3], rot[3];
    float len, rlen, ang;
    static GLuint vcount;

    CHECK_ERROR

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glColor3f( 1, 1, 1);

    if (tracking) {
	dx = curx - startx;
	dy = cury - starty;
    } else {
	dx = spindx;
	dy = spindy;
    }
    if (dx || dy) {
	dy = -dy;
	v[0] = dx;
	v[1] = dy;
	v[2] = 0;

	len = length(v);
	ang = -len / 600 * 360;
	norm( v );
	cross( v, z_axis, rot );

	/*
	** This is certainly not recommended for programs that care
	** about performance or numerical stability: we concatenate
	** the rotation onto the current modelview matrix and read the
	** matrix back, thus saving ourselves from writing our own
	** matrix manipulation routines.
	*/
	glLoadIdentity();
	glRotatef(ang, rot[0], rot[1], rot[2]);
	glMultMatrixf(modelmatrix);
	glGetFloatv(GL_MODELVIEW_MATRIX, modelmatrix);
    }
    glLoadIdentity();
    glTranslatef( 0.0, 0.0, -10.0 );
    glMultMatrixf(modelmatrix);
    
    if (useglunurbs) {
	if (showsurf) glCallList(surflist);
	if (showgrid) glCallList(gridlist);
    } else {
	glMapGrid2f( usegments, 0.0, 1.0, vsegments, 0.0, 1.0 );
	drawmesh();
    }

    glutSwapBuffers();

    CHECK_ERROR
}

static void usage(void)
{
    printf("usage: surfgrid [-f]\n");
    exit(-1);
}

/*
** what to do when a menu item is selected. This function also handles
** keystroke events.
*/
void menu(int item)
{
    switch(item) {
    case 'h': case '?':
        printf("o\tpolygon offset off\n"); 
	printf("l\tpolygon offset on by pulling lines away from polygons\n");
	printf("p\tpolygon offset on by pushing polygons away from lines\n");
        printf("F\tincrease polygon offset factor\n");
        printf("f\tdecrease polygon offset factor\n");
        printf("B\tincrease polygon offset units\n");
        printf("b\tdecrease polygon offset units\n");
        printf("g\ttoggle grid drawing\n");
        printf("s\ttoggle smooth/flat shading\n");
        printf("n\ttoggle whether to use GL evaluators or GLU nurbs\n");
        printf("u\tdecr number of segments in U direction\n");
        printf("U\tincr number of segments in U direction\n");
        printf("v\tdecr number of segments in V direction\n");
        printf("V\tincr number of segments in V direction\n");
	printf("y\tprint factor & units\n");
	printf("h,?\tthis help message\n");
        printf("Esc\tquit\n");
        break;
    case 'o':
        polygonOffsetMode = POLYGON_OFFSET_OFF;
	CHECK_ERROR

	glDisable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_POLYGON_OFFSET_LINE);

	/* leave factor & units untouched */

        printf("Polygon offset off.\n");

	CHECK_ERROR
        break;
    case 'p':
        polygonOffsetMode = POLYGON_OFFSET_ON_PUSH_POLYGONS;

	CHECK_ERROR

	glEnable(GL_POLYGON_OFFSET_FILL);
	glDisable(GL_POLYGON_OFFSET_LINE);
	
	CHECK_ERROR
  
        printf("Polygon offset on by pushing polygons away from viewer.\n");
	factor = 1.0;
	units = 1.0;
	printf("Factor: %.2f Units: %.1f\n",factor,units);

	break;
    case 'l':
        polygonOffsetMode = POLYGON_OFFSET_ON_PULL_LINES;

	CHECK_ERROR

	glDisable(GL_POLYGON_OFFSET_FILL);
	glEnable(GL_POLYGON_OFFSET_LINE);

	CHECK_ERROR

        printf("Polygon offset on by pulling lines closer to viewer.\n");
	factor = -1.0;
	units = -1.0;
	printf("Factor: %.2f Units: %.1f\n",factor,units);

        break;
#define FACTOR_INCREMENT 0.25
    case 'F':
	factor += FACTOR_INCREMENT;
	if (polygonOffsetMode == POLYGON_OFFSET_OFF) {
	   printf("Polygon offset currently off.\n");
	}
	printf("Factor: %.2f Units: %.1f\n",factor,units);
	break;
    case 'f':
	factor -= FACTOR_INCREMENT;
	if (polygonOffsetMode == POLYGON_OFFSET_OFF) {
	   printf("Polygon offset currently off.\n");
	}
	printf("Factor: %.2f Units: %.1f\n",factor,units);
	break;
#define UNITS_INCREMENT 1.0
    case 'B':
	units += UNITS_INCREMENT;
	if (polygonOffsetMode == POLYGON_OFFSET_OFF) {
	   printf("Polygon offset currently off.\n");
	}
	printf("Factor: %.2f Units: %.1f\n",factor,units);
	break;
    case 'b':
	units -= UNITS_INCREMENT;
	if (polygonOffsetMode == POLYGON_OFFSET_OFF) {
	   printf("Polygon offset currently off.\n");
	}
	printf("Factor: %.2f Units: %.1f\n",factor,units);
	break;
    case 'g':
	showgrid = !showgrid;
	break;
    case 'n':
	useglunurbs = !useglunurbs;
	break;
    case 's':
	smooth = !smooth;
	if (smooth) {
	    glShadeModel( GL_SMOOTH );
	} else {
	    glShadeModel( GL_FLAT );
	}
	break;
    case 't':
	showsurf = !showsurf;
	break;
    case 'u':
	usegments = (usegments < 2 ? 1 : usegments-1);
	createlists();
	break;
    case 'U':
	usegments++;
	createlists();
	break;
    case 'v':
	vsegments = (vsegments < 2 ? 1 : vsegments-1);
	createlists();
	break;
    case 'V':
	vsegments++;
	createlists();
	break;
    case 'y':
        {
	  GLfloat getFactor[1], getUnits[1];

	  glGetFloatv(GL_POLYGON_OFFSET_FACTOR,getFactor);
	  glGetFloatv(GL_POLYGON_OFFSET_UNITS,getUnits);
	  printf("Factor: %.2f Units: %.1f\n",getFactor[0],getUnits[0]);
        }
        break;
    case '\033': /* ESC key: quit */
	exit(0);
	break;
    }
    glPolygonOffset(factor,units);

    glutPostRedisplay();
}

void key(unsigned char key, int x, int y)
{
   menu((int)key);
}

void animate()
{
    if (!tracking && (spindx!=0 || spindy!=0))
	redraw();
}

int main(int argc, char **argv)
{
    int i, ms;

    glutInit(&argc, argv); /* initialize glut, processing arguments */

    for (i=1; i<argc; i++) {
	if (argv[i][0] == '-') {
	    switch (argv[i][1]) {
	      case 'f':
		fullscreen = 1;
		break;
	      default:
		usage();
		break;
	    }
	} else {
	    usage();
	}
    }

    glutInitWindowSize(winwidth, winheight);
    glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_DEPTH);
    glutCreateWindow("Surfgrid: a polygon offset 1.1 demo "
		     "(press right button for menu)");

    /* create a menu for the right mouse button */
    glutCreateMenu(menu);
    glutAddMenuEntry("o: polygon offset off", 'o');    
    glutAddMenuEntry("p: polygon offset on push polygons", 'p');
    glutAddMenuEntry("l: polygon offset on pull lines", 'l');    
    glutAddMenuEntry("F: increase factor", 'F');
    glutAddMenuEntry("f: decrease factor", 'f');
    glutAddMenuEntry("B: increase units", 'B');
    glutAddMenuEntry("b: decrease units", 'b');
    glutAddMenuEntry("g: toggle grid", 'g');
    glutAddMenuEntry("s: toggle smooth shading", 's');
    glutAddMenuEntry("t: toggle surface", 't');
    glutAddMenuEntry("n: toggle GL evaluators/GLU nurbs", 'n');
    glutAddMenuEntry("u: decrement u segments", 'u');
    glutAddMenuEntry("U: increment u segments", 'U');
    glutAddMenuEntry("v: decrement v segments", 'v');
    glutAddMenuEntry("V: increment v segments", 'V');
    glutAddMenuEntry("y: get factor & units", 'y');
    glutAddMenuEntry("h,?: this help message", 'h');
    glutAddMenuEntry("<esc>: exit program", '\033');
    glutAttachMenu(GLUT_RIGHT_BUTTON);

    /* set callbacks */
    glutKeyboardFunc(key);
    glutDisplayFunc(redraw);
    glutReshapeFunc(reshape);
    glutMouseFunc(button);
    glutMotionFunc(move);
    glutIdleFunc(animate);

    init();
    glutMainLoop();
}

/****************************************************************************/

float circleknots[] = { 0.0, 0.0, 0.0, 0.25, 0.50, 0.50, 0.75, 1.0, 1.0, 1.0 };

void createlists(void)
{
    CHECK_ERROR

    gluNurbsProperty(nobj, GLU_U_STEP, (usegments-1)*4 );
    gluNurbsProperty(nobj, GLU_V_STEP, (vsegments-1)*4 );

    gluNurbsProperty(nobj, GLU_DISPLAY_MODE, GLU_FILL);
    glNewList(surflist, GL_COMPILE);
    	surfacematerials();
    	gluBeginSurface(nobj);
    	gluNurbsSurface(nobj, 10, circleknots, 10, circleknots,
		    	4, 28, torusnurbpts, 3, 3, GL_MAP2_VERTEX_4);
    	gluEndSurface(nobj);
    glEndList();

    gluNurbsProperty(nobj, GLU_DISPLAY_MODE, GLU_OUTLINE_POLYGON);
    glNewList(gridlist, GL_COMPILE);
    	gridmaterials();
    	gluBeginSurface(nobj);
    	gluNurbsSurface(nobj, 10, circleknots, 10, circleknots,
		    	4, 28, torusnurbpts, 3, 3, GL_MAP2_VERTEX_4);
    	gluEndSurface(nobj);
    glEndList();

    CHECK_ERROR
}

/****************************************************************************/

/*
 * Control points of the torus in Bezier form.  Can be rendered
 * using OpenGL evaluators.
 */
static GLfloat torusbezierpts[] = {
	 4.0, 0.0, 0.0, 4.0, 2.0, 0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0,
         3.0, 0.0, 1.0, 2.0, 4.0, 0.0, 1.0, 2.0, 8.0, 0.0, 0.0, 4.0,
         8.0, 0.0, 0.0, 4.0, 4.0, 0.0,-1.0, 2.0, 3.0, 0.0,-1.0, 2.0,
         3.0, 0.0,-1.0, 2.0, 2.0, 0.0,-1.0, 2.0, 4.0, 0.0, 0.0, 4.0,
         2.0,-2.0, 0.0, 2.0, 1.0,-1.0, 0.5, 1.0, 1.5,-1.5, 0.5, 1.0,
         1.5,-1.5, 0.5, 1.0, 2.0,-2.0, 0.5, 1.0, 4.0,-4.0, 0.0, 2.0,
         4.0,-4.0, 0.0, 2.0, 2.0,-2.0,-0.5, 1.0, 1.5,-1.5,-0.5, 1.0,
         1.5,-1.5,-0.5, 1.0, 1.0,-1.0,-0.5, 1.0, 2.0,-2.0, 0.0, 2.0,
         0.0,-2.0, 0.0, 2.0, 0.0,-1.0, 0.5, 1.0, 0.0,-1.5, 0.5, 1.0,
         0.0,-1.5, 0.5, 1.0, 0.0,-2.0, 0.5, 1.0, 0.0,-4.0, 0.0, 2.0,
         0.0,-4.0, 0.0, 2.0, 0.0,-2.0,-0.5, 1.0, 0.0,-1.5,-0.5, 1.0,
         0.0,-1.5,-0.5, 1.0, 0.0,-1.0,-0.5, 1.0, 0.0,-2.0, 0.0, 2.0,
         0.0,-2.0, 0.0, 2.0, 0.0,-1.0, 0.5, 1.0, 0.0,-1.5, 0.5, 1.0,
         0.0,-1.5, 0.5, 1.0, 0.0,-2.0, 0.5, 1.0, 0.0,-4.0, 0.0, 2.0,
         0.0,-4.0, 0.0, 2.0, 0.0,-2.0,-0.5, 1.0, 0.0,-1.5,-0.5, 1.0,
         0.0,-1.5,-0.5, 1.0, 0.0,-1.0,-0.5, 1.0, 0.0,-2.0, 0.0, 2.0,
        -2.0,-2.0, 0.0, 2.0,-1.0,-1.0, 0.5, 1.0,-1.5,-1.5, 0.5, 1.0,
        -1.5,-1.5, 0.5, 1.0,-2.0,-2.0, 0.5, 1.0,-4.0,-4.0, 0.0, 2.0,
        -4.0,-4.0, 0.0, 2.0,-2.0,-2.0,-0.5, 1.0,-1.5,-1.5,-0.5, 1.0,
        -1.5,-1.5,-0.5, 1.0,-1.0,-1.0,-0.5, 1.0,-2.0,-2.0, 0.0, 2.0,
        -4.0, 0.0, 0.0, 4.0,-2.0, 0.0, 1.0, 2.0,-3.0, 0.0, 1.0, 2.0,
        -3.0, 0.0, 1.0, 2.0,-4.0, 0.0, 1.0, 2.0,-8.0, 0.0, 0.0, 4.0,
        -8.0, 0.0, 0.0, 4.0,-4.0, 0.0,-1.0, 2.0,-3.0, 0.0,-1.0, 2.0,
        -3.0, 0.0,-1.0, 2.0,-2.0, 0.0,-1.0, 2.0,-4.0, 0.0, 0.0, 4.0,
        -4.0, 0.0, 0.0, 4.0,-2.0, 0.0, 1.0, 2.0,-3.0, 0.0, 1.0, 2.0,
        -3.0, 0.0, 1.0, 2.0,-4.0, 0.0, 1.0, 2.0,-8.0, 0.0, 0.0, 4.0,
        -8.0, 0.0, 0.0, 4.0,-4.0, 0.0,-1.0, 2.0,-3.0, 0.0,-1.0, 2.0,
        -3.0, 0.0,-1.0, 2.0,-2.0, 0.0,-1.0, 2.0,-4.0, 0.0, 0.0, 4.0,
        -2.0, 2.0, 0.0, 2.0,-1.0, 1.0, 0.5, 1.0,-1.5, 1.5, 0.5, 1.0,
        -1.5, 1.5, 0.5, 1.0,-2.0, 2.0, 0.5, 1.0,-4.0, 4.0, 0.0, 2.0,
        -4.0, 4.0, 0.0, 2.0,-2.0, 2.0,-0.5, 1.0,-1.5, 1.5,-0.5, 1.0,
        -1.5, 1.5,-0.5, 1.0,-1.0, 1.0,-0.5, 1.0,-2.0, 2.0, 0.0, 2.0,
         0.0, 2.0, 0.0, 2.0, 0.0, 1.0, 0.5, 1.0, 0.0, 1.5, 0.5, 1.0,
         0.0, 1.5, 0.5, 1.0, 0.0, 2.0, 0.5, 1.0, 0.0, 4.0, 0.0, 2.0,
         0.0, 4.0, 0.0, 2.0, 0.0, 2.0,-0.5, 1.0, 0.0, 1.5,-0.5, 1.0,
         0.0, 1.5,-0.5, 1.0, 0.0, 1.0,-0.5, 1.0, 0.0, 2.0, 0.0, 2.0,
         0.0, 2.0, 0.0, 2.0, 0.0, 1.0, 0.5, 1.0, 0.0, 1.5, 0.5, 1.0,
         0.0, 1.5, 0.5, 1.0, 0.0, 2.0, 0.5, 1.0, 0.0, 4.0, 0.0, 2.0,
         0.0, 4.0, 0.0, 2.0, 0.0, 2.0,-0.5, 1.0, 0.0, 1.5,-0.5, 1.0,
         0.0, 1.5,-0.5, 1.0, 0.0, 1.0,-0.5, 1.0, 0.0, 2.0, 0.0, 2.0,
         2.0, 2.0, 0.0, 2.0, 1.0, 1.0, 0.5, 1.0, 1.5, 1.5, 0.5, 1.0,
         1.5, 1.5, 0.5, 1.0, 2.0, 2.0, 0.5, 1.0, 4.0, 4.0, 0.0, 2.0,
         4.0, 4.0, 0.0, 2.0, 2.0, 2.0,-0.5, 1.0, 1.5, 1.5,-0.5, 1.0,
         1.5, 1.5,-0.5, 1.0, 1.0, 1.0,-0.5, 1.0, 2.0, 2.0, 0.0, 2.0,
         4.0, 0.0, 0.0, 4.0, 2.0, 0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0,
         3.0, 0.0, 1.0, 2.0, 4.0, 0.0, 1.0, 2.0, 8.0, 0.0, 0.0, 4.0,
         8.0, 0.0, 0.0, 4.0, 4.0, 0.0,-1.0, 2.0, 3.0, 0.0,-1.0, 2.0,
         3.0, 0.0,-1.0, 2.0, 2.0, 0.0,-1.0, 2.0, 4.0, 0.0, 0.0, 4.0,
};

/*
 * Control points of a torus in NURBS form.  Can be rendered using
 * the GLU NURBS routines.
 */
static GLfloat torusnurbpts[] = {
         4.0, 0.0, 0.0, 4.0, 2.0, 0.0, 1.0, 2.0, 4.0, 0.0, 1.0, 2.0,
         8.0, 0.0, 0.0, 4.0, 4.0, 0.0,-1.0, 2.0, 2.0, 0.0,-1.0, 2.0,
         4.0, 0.0, 0.0, 4.0, 2.0,-2.0, 0.0, 2.0, 1.0,-1.0, 0.5, 1.0,
         2.0,-2.0, 0.5, 1.0, 4.0,-4.0, 0.0, 2.0, 2.0,-2.0,-0.5, 1.0,
         1.0,-1.0,-0.5, 1.0, 2.0,-2.0, 0.0, 2.0,-2.0,-2.0, 0.0, 2.0,
        -1.0,-1.0, 0.5, 1.0,-2.0,-2.0, 0.5, 1.0,-4.0,-4.0, 0.0, 2.0,
        -2.0,-2.0,-0.5, 1.0,-1.0,-1.0,-0.5, 1.0,-2.0,-2.0, 0.0, 2.0,
        -4.0, 0.0, 0.0, 4.0,-2.0, 0.0, 1.0, 2.0,-4.0, 0.0, 1.0, 2.0,
        -8.0, 0.0, 0.0, 4.0,-4.0, 0.0,-1.0, 2.0,-2.0, 0.0,-1.0, 2.0,
        -4.0, 0.0, 0.0, 4.0,-2.0, 2.0, 0.0, 2.0,-1.0, 1.0, 0.5, 1.0,
        -2.0, 2.0, 0.5, 1.0,-4.0, 4.0, 0.0, 2.0,-2.0, 2.0,-0.5, 1.0,
        -1.0, 1.0,-0.5, 1.0,-2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 0.0, 2.0,
         1.0, 1.0, 0.5, 1.0, 2.0, 2.0, 0.5, 1.0, 4.0, 4.0, 0.0, 2.0,
         2.0, 2.0,-0.5, 1.0, 1.0, 1.0,-0.5, 1.0, 2.0, 2.0, 0.0, 2.0,
         4.0, 0.0, 0.0, 4.0, 2.0, 0.0, 1.0, 2.0, 4.0, 0.0, 1.0, 2.0,
         8.0, 0.0, 0.0, 4.0, 4.0, 0.0,-1.0, 2.0, 2.0, 0.0,-1.0, 2.0,
         4.0, 0.0, 0.0, 4.0,
};

