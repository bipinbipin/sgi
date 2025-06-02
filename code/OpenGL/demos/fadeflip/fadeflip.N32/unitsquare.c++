#include "unitsquare.h"

#include <GL/gl.h>

static float vertices[][3] = {
  {-1.0, -1.0, 0.0},
  {0.0, -1.0, 0.0},
  {0.0, 0.0, 0.0},
  {-1.0, 0.0, 0.0},

  {0.0, -1.0, 0.0},
  {1.0, -1.0, 0.0},
  {1.0, 0.0, 0.0},
  {0.0, 0.0, 0.0},

  {-1.0, 0.0, 0.0},
  {0.0, 0.0, 0.0},
  {0.0, 1.0, 0.0},
  {-1.0, 1.0, 0.0},

  {0.0, 0.0, 0.0},
  {1.0, 0.0, 0.0},
  {1.0, 1.0, 0.0},
  {0.0, 1.0, 0.0}

};

static float texcoord[][2] = {
  {0.0, 0.0},
  {0.5, 0.0},
  {0.5, 0.5},
  {0.0, 0.5},

  {0.5, 0.0},
  {1.0, 0.0},
  {1.0, 0.5},
  {0.5, 0.5},

  {0.0, 0.5},
  {0.5, 0.5},
  {0.5, 1.0},
  {0.0, 1.0},

  {0.5, 0.5},
  {1.0, 0.5},
  {1.0, 1.0},
  {0.5, 1.0}
};

unitsquare::unitsquare()
{
}

void unitsquare::open()
{
}

void unitsquare::draw()
{
  glBegin(GL_QUADS);

  glTexCoord2fv(texcoord[0]);
  glVertex3fv(vertices[0]);
  glTexCoord2fv(texcoord[1]);
  glVertex3fv(vertices[1]);
  glTexCoord2fv(texcoord[2]);
  glVertex3fv(vertices[2]);
  glTexCoord2fv(texcoord[3]);
  glVertex3fv(vertices[3]); 

  glTexCoord2fv(texcoord[4]);
  glVertex3fv(vertices[4]);
  glTexCoord2fv(texcoord[5]);
  glVertex3fv(vertices[5]);
  glTexCoord2fv(texcoord[6]);
  glVertex3fv(vertices[6]);
  glTexCoord2fv(texcoord[7]);
  glVertex3fv(vertices[7]); 

  glTexCoord2fv(texcoord[8]);
  glVertex3fv(vertices[8]);
  glTexCoord2fv(texcoord[9]);
  glVertex3fv(vertices[9]);
  glTexCoord2fv(texcoord[10]);
  glVertex3fv(vertices[10]);
  glTexCoord2fv(texcoord[11]);
  glVertex3fv(vertices[11]); 

  glTexCoord2fv(texcoord[12]);
  glVertex3fv(vertices[12]);
  glTexCoord2fv(texcoord[13]);
  glVertex3fv(vertices[13]);
  glTexCoord2fv(texcoord[14]);
  glVertex3fv(vertices[14]);
  glTexCoord2fv(texcoord[15]);
  glVertex3fv(vertices[15]); 

  glEnd();
}

void unitsquare::draw_backwards()
{
  glBegin(GL_QUADS);

  glTexCoord2fv(texcoord[3]);
  glVertex3fv(vertices[3]);
  glTexCoord2fv(texcoord[2]);
  glVertex3fv(vertices[2]);
  glTexCoord2fv(texcoord[1]);
  glVertex3fv(vertices[1]);
  glTexCoord2fv(texcoord[0]);
  glVertex3fv(vertices[0]); 

  glTexCoord2fv(texcoord[7]);
  glVertex3fv(vertices[7]);
  glTexCoord2fv(texcoord[6]);
  glVertex3fv(vertices[6]);
  glTexCoord2fv(texcoord[5]);
  glVertex3fv(vertices[5]);
  glTexCoord2fv(texcoord[4]);
  glVertex3fv(vertices[4]); 

  glTexCoord2fv(texcoord[11]);
  glVertex3fv(vertices[11]);
  glTexCoord2fv(texcoord[10]);
  glVertex3fv(vertices[10]);
  glTexCoord2fv(texcoord[9]);
  glVertex3fv(vertices[9]);
  glTexCoord2fv(texcoord[8]);
  glVertex3fv(vertices[8]); 

  glTexCoord2fv(texcoord[15]);
  glVertex3fv(vertices[15]);
  glTexCoord2fv(texcoord[14]);
  glVertex3fv(vertices[14]);
  glTexCoord2fv(texcoord[13]);
  glVertex3fv(vertices[13]);
  glTexCoord2fv(texcoord[12]);
  glVertex3fv(vertices[12]); 

  glEnd();
}
