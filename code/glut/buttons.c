
/*  gcc -o buttons buttons.c -lglut -lGLU -lGL -lXmu -lXi -lXext -lX11 -lm */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glut.h>

int *dials, *buttons;
int numdials, numbuttons;
int dw, bw;

void
displayButtons(void)
{
  int i, n;

  glClear(GL_COLOR_BUFFER_BIT);
  glBegin(GL_QUADS);
  for(i=0,n=0;i<32;i++,n++) {
    switch(n) {
    case 0:
    case 5:
    case 30:
      n++;
    }
    if(buttons[i]) {
      glColor3f(1,0,0);
    } else {
      glColor3f(1,1,1);
    }
    glVertex2f((n%6)*40+10,(n/6)*40+10);
    glVertex2f((n%6)*40+30,(n/6)*40+10);
    glVertex2f((n%6)*40+30,(n/6)*40+30);
    glVertex2f((n%6)*40+10,(n/6)*40+30);
  }
  glEnd();
  glutSwapBuffers();
}

void
reshape(int w, int h)
{
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, w, 0, h);
  if(glutGetWindow() == bw) {
     glScalef(1, -1, 1);
     glTranslatef(0, -h, 0);
  }
  glMatrixMode(GL_MODELVIEW);
}

void
dobutton(int button, int state)
{
  if(button <= numbuttons) {
    buttons[button-1] = (state == GLUT_DOWN);
    glutPostWindowRedisplay(bw);
  }
}

int
main(int argc, char **argv)
{
  glutInit(&argc, argv);

  numbuttons = glutDeviceGet(GLUT_NUM_BUTTON_BOX_BUTTONS);
  buttons = (int*) calloc(numbuttons, sizeof(int));
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutButtonBoxFunc(dobutton);
  glutReshapeFunc(reshape);
  glutInitWindowSize(240, 240);
  bw = glutCreateWindow("GLUT button box");
  glClearColor(0.5, 0.5, 0.5, 1.0);
  glutDisplayFunc(displayButtons);
  glutReshapeFunc(reshape);
  glutButtonBoxFunc(dobutton);
  glutMainLoop();
  return 0;             /* ANSI C requires main to return int. */
}
