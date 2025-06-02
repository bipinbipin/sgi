/*  compile with cc ride.c -o ride -lglut -lGLU -lGL -lXmu -lXi -lXext -lX11 -lm -laudio*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glut.h>
#include <dmedia/audio.h>

int *dials;
int numdials;
int dw;
int i=0;
int dial1value[44000];
    
    int         latency;
    int         i,j;
    short       buf[60000];
    ALconfig    config;
    ALport      audioPort;
    double      samplingRate;
    double      arg, argInc;
    double      amplitude, frequency, phase;
    ALpv        pv;


#define M_PI 3.14159265358979323846
#define LATENCY_FACTOR 100

void
dial1record(int value)
{
   dial1value[i] = value;
   i = i + 1;
   printf("dial1record was called: passed value = %d   recorded value %d = %d\n",value,i,dial1value[i]);}

void
drawCircle(int x, int y, int r, int dir)
{
   float angle;

   glPushMatrix();
   glTranslatef(x,y,0);
   glBegin(GL_TRIANGLE_FAN);
   glVertex2f(0,0);
   for(angle = 2*M_PI; angle >= 0; angle -= M_PI/12) {
      glVertex2f(r*cos(angle),r*sin(angle));
   }
   glEnd();
   glColor3f(0,0,1);
   glBegin(GL_LINES);
   glVertex2f(0,0);
   glVertex2f(r*cos(dir*M_PI/180),r*sin(dir*M_PI/180));
   glEnd();
   glPopMatrix();
   /* printf("drawCircle was called\n"); */
}

void
displayDials(void)
{
  int i;

  glClear(GL_COLOR_BUFFER_BIT);
  for(i=0;i<numdials;i++) {
    glColor3f(0, 1, 0);
    drawCircle(100 + (i%2) * 100, 100 + (i/2) * 100, 40, -dials[i]+90);
  }
  glutSwapBuffers();
  /*printf("displayDials was called\n");*/
}

void
reshape(int w, int h)
{
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(0, w, 0, h);
  glMatrixMode(GL_MODELVIEW);
  printf("reshape was called : w = %d   h = %d\n",w, h);
}

void
dodial(int dial, int value)
{
  int j;

  dials[dial - 1] = value % 360;
  /* dial1record(value); */
  glutPostWindowRedisplay(dw);
  printf("dodial was called : dial = %d  value = %d\n",dial,value);


switch (dial)  {
	case 1:
	  frequency = value;
	  break;
	case 2:
	  phase = value:
	  break;
 }

    argInc = frequency*phase*M_PI/samplingRate;
    arg = M_PI / phase;
 
    for (i = 0; i < latency; i++, arg += argInc) 
        buf[i] = (short) (32767.0*cos(arg));
        
    alWriteFrames(audioPort, buf, latency);
 

}

int
main(int argc, char **argv)
{

    pv.param = AL_RATE;
    if (alGetParams(AL_DEFAULT_OUTPUT,&pv,1) < 0) {
        printf("alGetParams failed: %s\n",alGetErrorString(oserror()));
        exit(-1);
    }
    samplingRate = alFixedToDouble(pv.value.ll);
    if (samplingRate < 0) {
        samplingRate = 44100.0;
    }

    config = alNewConfig();
    latency = samplingRate / LATENCY_FACTOR;
    alSetQueueSize(config, latency);
    alSetChannels(config, AL_MONO);

    audioPort = alOpenPort("outport", "w", config);
    if (!audioPort) {
        fprintf(stderr, "couldn't open port: %s\n",alGetErrorString(oserror()));
        exit(-1);
    }

    frequency = 440.0;
    phase = 2.0;
    amplitude = 32000;

  glutInit(&argc, argv);
  numdials = glutDeviceGet(GLUT_NUM_DIALS);
  if(numdials <= 0) {
     fprintf(stderr, "dials: No dials available\n");
     exit(1);
  }
  dials = (int*) calloc(numdials, sizeof(int));
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize(300, ((numdials+1)/2) * 100 + 100);
  dw = glutCreateWindow("dials debug");
  glClearColor(0.5, 0.5, 0.5, 1.0);
  glLineWidth(3.0);
  glutDialsFunc(dodial);
  glutDisplayFunc(displayDials);
  glutReshapeFunc(reshape);
  glutMainLoop();
 
    while (alGetFilled(audioPort) > 0)
        sginap(1);

    alClosePort(audioPort);
    alFreeConfig(config);

  return 0;             /* ANSI C requires main to return int. */
}
