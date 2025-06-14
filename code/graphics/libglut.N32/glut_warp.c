
/* Copyright (c) Mark J. Kilgard, 1996. */

/* This program is freely distributable without licensing fees
   and is provided without guarantee or warrantee expressed or
   implied. This program is -not- in the public domain. */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <GL/glut.h>
#include "glutint.h"

void
glutWarpPointer(int x, int y)
{
  XWarpPointer(__glutDisplay, None, __glutCurrentWindow->win,
    0, 0, 0, 0, x, y);
  XFlush(__glutDisplay);
}

