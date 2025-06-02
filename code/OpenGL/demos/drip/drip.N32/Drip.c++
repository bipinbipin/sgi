/*
 * (c) Copyright 1993, 1994, 1995, 1996 Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED 
 * Permission to use, copy, modify, and distribute this software for 
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that 
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission. 
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * US Government Users Restricted Rights 
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * OpenGL(TM) is a trademark of Silicon Graphics, Inc.
 */
#include <GL/glu.h>
#include <GL/glx.h>
extern "C" {
#include <malloc.h>
#include <math.h>
#include <stdlib.h>
}

#include "Drip.h"

Drip::Drip() 
{
  outer_color[0] = ring_color[0] = inner_color[0] = 0.0;
  outer_color[1] = ring_color[1] = inner_color[1] = 0.0;
  outer_color[2] = ring_color[2] = inner_color[2] = 0.0;
  outer_color[3] = inner_color[3] = 0.0;
  ring_color[3] = 1.0;

  outer_radius = 1.0;
  ring_radius = outer_radius / 2.0;

  divisions = 50;
  points = NULL;
  
}

void Drip::draw() 
{
  int i;

  glBegin(GL_TRIANGLES);
  for (i = 0; i < divisions; i++) {
    glColor4fv(inner_color);
    glVertex2f(0.0, 0.0);
    glColor4fv(ring_color);
    glVertex2f(points[2*i] * ring_radius, points[2*i + 1] * ring_radius);
    glVertex2f(points[2*((i+1) % divisions)] * ring_radius,
	       points[(2*((i+1) % divisions)) + 1] * ring_radius);
  }
  glEnd();

  glBegin(GL_QUADS);
  for (i = 0; i < divisions; i++) {
    glColor4fv(ring_color);
    glVertex2f(points[2*i] * ring_radius, points[2*i + 1] * ring_radius);
    glVertex2f(points[2*((i+1) % divisions)] * ring_radius,
	       points[(2*((i+1) % divisions)) + 1] * ring_radius);
    glColor4fv(outer_color);
    glVertex2f(points[2*((i+1) % divisions)] * outer_radius,
	       points[(2*((i+1) % divisions)) + 1] * outer_radius);
    glVertex2f(points[2*i] * outer_radius, points[2*i + 1] * outer_radius);
   }
  glEnd();

}

void Drip::draw_starburst() 
{
  int i;

  glBegin(GL_TRIANGLES);
  for (i = 0; i < divisions; i++) {
    glColor4fv(inner_color);
    glVertex2f(0.0, 0.0);
    glColor4fv(ring_color);
    glVertex2f(points[2*i] * ring_radius, points[2*i + 1] * ring_radius);
    glVertex2f(points[2*((i+1) % divisions)] * ring_radius,
	       points[(2*((i+1) % divisions)) + 1] * ring_radius);
  }
  glEnd();

  glBegin(GL_QUADS);
  for (i = 0; i < divisions; i++) {
    glColor4fv(ring_color);
    glVertex2f(points[2*i] * ring_radius, points[2*i + 1] * ring_radius);
    glVertex2f(points[2*((i+1) % divisions)] * ring_radius,
	       points[(2*((i+1) % divisions)) + 1] * ring_radius);
    glColor4fv(outer_color);
    glVertex2f(points[2*i] * outer_radius, points[2*i + 1] * outer_radius);
    glVertex2f(points[2*((i+1) % divisions)] * outer_radius,
	       points[(2*((i+1) % divisions)) + 1] * outer_radius);
  }
  glEnd();

}


void Drip::set_divisions(int new_divisions) 
{
  divisions = new_divisions;
}

int Drip::get_divisions() {
  return divisions;
}

void Drip::set_points(float *new_points) {
  points = new_points;
}

void Drip::alloc_points() {
  points = (float *)malloc(divisions * 2 * sizeof(*points));
  if (divisions > 0 && points == NULL) {
    fprintf(stderr, "Malloc failed.\n");
    exit(1);
  }
}
					

void Drip::fill_points() {
  int i;
  float theta;
  float delta;

  delta = 2.0 * M_PI / (float)divisions;
  theta = 0.0;
  for (i = 0; i < divisions; i++) {
    points[2 * i] = cos(theta);
    points[2 * i + 1] = sin(theta);
    theta += delta;
  }
}

float *Drip::get_points() {
  return points;
}
