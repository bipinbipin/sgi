#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glc.h>
#include <GL/glut.h>


GLuint contextID;


void Init(void)
{

    if ((contextID = glcGenContext()) == 0) {
	printf("Died at glcGenContext\n");
	exit(1);
    }

    glcContext(contextID);
    if (glcGetError() != GL_NO_ERROR) {
	printf("Died at glcContext\n");
	exit(1);
    }

    glcRenderStyle(GLC_TRIANGLE);

    glClearColor(0.0, 0.0, 0.0, 0.0);
}

void Reshape(int width, int height)
{

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)width, 0.0, (double)height, -10000.0, 10000.0);
    glMatrixMode(GL_MODELVIEW);
}

void Key(unsigned char key, int x, int y)
{
    int i;
    const char *face;

    switch (key) {
	case 27:
	    glcDeleteContext(contextID);
	    exit(1);
    }
}

void Display(void)
{

    glClear(GL_COLOR_BUFFER_BIT);

    glPushMatrix();
    glTranslatef(30.0, 100.0, 0.0);
    glScalef(40.0, 40.0, 1.0);
    glColor3f(0.0, 1.0, 0.0);
    glcRenderString("Hello World!");
    glPopMatrix();

    if (glcGetError() != GL_NO_ERROR) {
	printf("Got a GLC error\n");
	exit(1);
    }

    glutSwapBuffers();
}

void main(int argc, char **argv)
{

    glutInitWindowSize(300, 300);
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB|GLUT_DOUBLE);
    glutCreateWindow("glc test");
    Init();
    glutDisplayFunc(Display);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Key);
    glutMainLoop();
}
