#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include "atlantis.h"

fishRec sharks[NUM_SHARKS];
fishRec momWhale;
fishRec babyWhale;
fishRec dolph;


void InitFishs(void)
{
    int i;

    for (i = 0; i < NUM_SHARKS; i++) {
        sharks[i].x = 70000.0 + rand() % 6000;
        sharks[i].y = rand() % 6000;
        sharks[i].z = rand() % 6000;
        sharks[i].psi = rand() % 360 - 180.0;
        sharks[i].v = 1.0;
    }

    dolph.x = 30000.0;
    dolph.y = 0.0;
    dolph.z = 6000.0;
    dolph.psi = 90.0;
    dolph.theta = 0.0;
    dolph.v = 3.0;

    momWhale.x = 70000.0;
    momWhale.y = 0.0;
    momWhale.z = 0.0;
    momWhale.psi = 90.0;
    momWhale.theta = 0.0;
    momWhale.v = 3.0;

    babyWhale.x = 60000.0;
    babyWhale.y = -2000.0;
    babyWhale.z = -2000.0;
    babyWhale.psi = 90.0;
    babyWhale.theta = 0.0;
    babyWhale.v = 3.0;
}

void Init(void)
{
    static float ambient[] = {0.1, 0.1, 0.1, 1.0};
    static float diffuse[] = {1.0, 1.0, 1.0, 1.0};
    static float position[] = {0.0, 1.0, 0.0, 0.0};
    static float mat_shininess[] = {90.0};
    static float mat_specular[] = {0.8, 0.8, 0.8, 1.0};
    static float mat_diffuse[] = {0.46, 0.66, 0.795, 1.0};
    static float mat_ambient[] = {0.0, 0.1, 0.2, 1.0};
    static float lmodel_ambient[] = {0.4, 0.4, 0.4, 1.0};
    static float lmodel_localviewer[] = {0.0};
    GLfloat map1[4] = {0.0, 0.0, 0.0, 0.0};
    GLfloat map2[4] = {0.0, 0.0, 0.0, 0.0};


    glEnable(GL_DEPTH_TEST);

    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_POSITION, position);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);
    glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, lmodel_localviewer);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, mat_shininess);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat_diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat_ambient);

    InitFishs();

    glClearColor(0.0, 0.5, 0.9, 0.0);
}

void Reshape(int width, int height)
{

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(40.0, (float)width/height, 2000.0, 2000000.0); 
    glMatrixMode(GL_MODELVIEW);
}

void Animate(void)
{
    int i;

    for (i = 0; i < NUM_SHARKS; i++) {
        SharkPilot(&sharks[i]);
        SharkMiss(i);
    }
    WhalePilot(&dolph);
    dolph.phi++;
    glutPostRedisplay();
    WhalePilot(&momWhale);
    momWhale.phi++;
    WhalePilot(&babyWhale);
    babyWhale.phi++;
}

void Key(unsigned char key, int x, int y)
{

    switch (key) {
	case 27:
	    exit(1);
    }
}

void Display(void)
{
    int i;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (i = 0; i < NUM_SHARKS; i++) {
        glPushMatrix();
          FishTransform(&sharks[i]);
	  glEnable(GL_NORMALIZE);
          DrawShark(&sharks[i]);
	  glDisable(GL_NORMALIZE);
        glPopMatrix();
    }

    glPushMatrix();
      FishTransform(&dolph);
      glEnable(GL_NORMALIZE);
      DrawDolphin(&dolph);
      glDisable(GL_NORMALIZE);
    glPopMatrix();

    glPushMatrix();
      FishTransform(&momWhale);
      glEnable(GL_NORMALIZE);
      DrawWhale(&momWhale);
      glDisable(GL_NORMALIZE);
    glPopMatrix();

    glPushMatrix();
      FishTransform(&babyWhale);
      glScalef(0.45, 0.45, 0.3);
      glEnable(GL_NORMALIZE);
      DrawWhale(&babyWhale);
      glDisable(GL_NORMALIZE);
    glPopMatrix();

    glutSwapBuffers();
}

void main(int argc, char **argv)
{

    glutInitWindowSize(500, 250);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB|GLUT_DOUBLE|GLUT_DEPTH);
    glutCreateWindow("GLUT Atlantis Demo");
    Init();
    glutDisplayFunc(Display);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Key);
    glutIdleFunc(Animate);
    glutMainLoop();
}
