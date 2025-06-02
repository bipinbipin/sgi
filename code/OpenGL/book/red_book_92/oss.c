#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "aux.h"
#include <math.h>


#define WIDTH  512
#define HEIGHT 512

static long clearMask = GL_COLOR_BUFFER_BIT;

static float light1_spotdir[] = { -2.0, -2.0, 2.0 };
static float light1_spotexp[] = { 4.0 };
static float light1_spotang[] = { 10.0 };
static float light1_ambient[] = { 0.8, 0.2, 0.2, 1.0 };
static float light1_diffuse[] = { 0.2, 0.2, 0.2, 0.0 };
static float light1_position[] = { .0, .0, -1.10, 1.0 };
static float light1_specular[] = { 0.0, 0.0, 0.0, 0.0 };

static float light2_spotdir[] = { -2.0, 2.0, 2.0 };
static float light2_spotexp[] = { 4.0 };
static float light2_spotang[] = { 10.0 };
static float light2_ambient[] = { 0.2, 0.8, 0.2, 1.0 };
static float light2_diffuse[] = { 0.2, 0.2, 0.2, 0.0 };
static float light2_position[] = { .0, .0, -1.10, 1.0 };
static float light2_specular[] = { 0.0, 0.0, 0.0, 0.0 };

static float light3_spotdir[] = { 2.0, 2.0, 2.0 };
static float light3_spotexp[] = { 4.0 };
static float light3_spotang[] = { 10.0 };
static float light3_ambient[] = { 0.8, 0.2, 0.8, 1.0 };
static float light3_diffuse[] = { 0.2, 0.2, 0.2, 0.0 };
static float light3_position[] = { .0, .0, -1.10, 1.0 };
static float light3_specular[] = { 0.0, 0.0, 0.0, 0.0 };

static float light4_spotdir[] = { 2.0, -2.0, 2.0 };
static float light4_spotexp[] = { 4.0 };
static float light4_spotang[] = { 10.0 };
static float light4_ambient[] = { 0.0, 0.0, 0.9, 1.0 };
static float light4_diffuse[] = { 0.2, 0.2, 0.2, 0.0 };
static float light4_position[] = { .0, .0, -1.10, 1.0 };
static float light4_specular[] = { 0.0, 0.0, 0.0, 0.0 };

static float face_mat_ambient[] = { 0.2, 0.2, 0.2, 1.0 };
static float face_mat_shininess[] = { 16.0 };
static float face_mat_specular[] = { 1.0, 1.0, 1.0, 0.0 };
static float face_mat_diffuse[] = { 1.0, 0.5, 0.1, 0.0 };

static float lmodel_ambient[] = { 1.0, 1.0, 1.0, 1.0 };
static float lmodel_twoside[] = { GL_FALSE };
static float lmodel_local[] = { GL_TRUE };

static void draw_plate(n)
long n;
{
	long i, j;
	float p0[3], p1[3];
	float n1[3];

	n1[0] = n1[1] = 0.0; 
	n1[2] = -1.0;
	p0[2] = p1[2] = 0.0;

	glNormal3fv(n1);
	for (i = 0; i < n; i++) {
		p0[0] = -1.0 + 2.0*i/n;
		p1[0] = -1.0 + 2.0*(i+1)/n;
		glBegin(GL_QUAD_STRIP);
		for (j = 0; j < n; j++) {
			p0[1] = p1[1] = -1.0 + 2.0*j/n;
			glVertex3fv(p0);
			glVertex3fv(p1);
		}
		glEnd();
	}
}

static void def_light_calc(void)
{
    glMaterialfv(GL_FRONT, GL_AMBIENT, face_mat_ambient);
    glMaterialfv(GL_FRONT, GL_SHININESS, face_mat_shininess);
    glMaterialfv(GL_FRONT, GL_SPECULAR, face_mat_specular);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, face_mat_diffuse);

    glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, lmodel_local);
    glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, lmodel_twoside);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);

    glEnable(GL_LIGHTING);

    glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, light1_diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1_specular);
    glLightfv(GL_LIGHT1, GL_POSITION, light1_position);

    glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, light1_spotdir);
    glLightfv(GL_LIGHT1, GL_SPOT_CUTOFF,    light1_spotang);
    glLightfv(GL_LIGHT1, GL_SPOT_EXPONENT,  light1_spotexp);
    glEnable(GL_LIGHT1);

    glLightfv(GL_LIGHT2, GL_AMBIENT, light2_ambient);
    glLightfv(GL_LIGHT2, GL_DIFFUSE, light2_diffuse);
    glLightfv(GL_LIGHT2, GL_SPECULAR, light2_specular);
    glLightfv(GL_LIGHT2, GL_POSITION, light2_position);

    glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, light2_spotdir);
    glLightfv(GL_LIGHT2, GL_SPOT_CUTOFF,    light2_spotang);
    glLightfv(GL_LIGHT2, GL_SPOT_EXPONENT,  light2_spotexp);
    glEnable(GL_LIGHT2);

    glLightfv(GL_LIGHT3, GL_AMBIENT, light3_ambient);
    glLightfv(GL_LIGHT3, GL_DIFFUSE, light3_diffuse);
    glLightfv(GL_LIGHT3, GL_SPECULAR, light3_specular);
    glLightfv(GL_LIGHT3, GL_POSITION, light3_position);

    glLightfv(GL_LIGHT3, GL_SPOT_DIRECTION, light3_spotdir);
    glLightfv(GL_LIGHT3, GL_SPOT_CUTOFF,    light3_spotang);
    glLightfv(GL_LIGHT3, GL_SPOT_EXPONENT,  light3_spotexp);
    glEnable(GL_LIGHT3);

    glLightfv(GL_LIGHT4, GL_AMBIENT, light4_ambient);
    glLightfv(GL_LIGHT4, GL_DIFFUSE, light4_diffuse);
    glLightfv(GL_LIGHT4, GL_SPECULAR, light4_specular);
    glLightfv(GL_LIGHT4, GL_POSITION, light4_position);

    glLightfv(GL_LIGHT4, GL_SPOT_DIRECTION, light4_spotdir);
    glLightfv(GL_LIGHT4, GL_SPOT_CUTOFF,    light4_spotang);
    glLightfv(GL_LIGHT4, GL_SPOT_EXPONENT,  light4_spotexp);
    glEnable(GL_LIGHT4);



}

#define FUNC(t) (2.0*sin(4.0*t))
#define PI 3.141592654
#define OFF1 (PI/2.0)
#define OFF2 (PI)
#define OFF3 (3.0*PI/2.0)
static void rotate_lights(float t)
{
    float lightpos[3] = { 0.0, 0.0, 2.0 };
    t /= 150.0;
    /* polar (t, FUNC(t)) */
    /* cartesian (FUNC(t)*cos(t), FUNC(t)*sin(t)) */
    lightpos[0] = FUNC(t)*cos(t);
    lightpos[1] = FUNC(t)*sin(t);
    glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, lightpos);
    t += OFF1;
    lightpos[0] = FUNC(t)*cos(t);
    lightpos[1] = FUNC(t)*sin(t);
    glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, lightpos);
    t += OFF1;
    lightpos[0] = FUNC(t)*cos(t);
    lightpos[1] = FUNC(t)*sin(t);
    glLightfv(GL_LIGHT3, GL_SPOT_DIRECTION, lightpos);
    t += OFF1;
    lightpos[0] = FUNC(t)*cos(t);
    lightpos[1] = FUNC(t)*sin(t);
    glLightfv(GL_LIGHT4, GL_SPOT_DIRECTION, lightpos);
}

my_display(int resolution)
{
  int i;
   for (i=0;; i++)
   {
        rotate_lights((float)i);
	draw_plate(resolution);
        auxSwapBuffers();
   }
}

main(int argc, char **argv)
{
    int i,type;
    short val;
    int resolution = (argc > 1) ? atoi(argv[1]) : 30;

#ifdef TK
    strcpy(wind.name, "Spot Test");
    wind.x = 20;
    wind.y = 20;
    wind.width = W;
    wind.height = H;
    wind.type = TK_WIND_REQUEST;
    wind.info = TK_WIND_RGB;
    wind.eventMask = TK_EVENT_EXPOSE | TK_EVENT_KEY | TK_EVENT_CONFIG |
		     TK_EVENT_MOUSEUP |  TK_EVENT_DRAW;
    if (tkNewWindow(&wind) == 0) {
	tkQuit();
    }
#endif

    auxInitPosition(320, 20, WIDTH, HEIGHT);

    type = AUX_DEPTH;
    type |= AUX_RGB;
    type |= AUX_DOUBLE;
    type |= AUX_INDIRECT;

    auxInitDisplayMode(type);

    if (auxInitWindow("Disco Lights") == GL_FALSE) {
         auxQuit();
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(25.0, (float)WIDTH/(float)HEIGHT, .50, 10.0);
    glMatrixMode(GL_MODELVIEW);
    gluLookAt(0.0,0.0,-5.0,0.0,0.0,0.0,0,1,0);

    def_light_calc();
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(clearMask);
    auxSwapBuffers();
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(clearMask);

    my_display(resolution);
}
