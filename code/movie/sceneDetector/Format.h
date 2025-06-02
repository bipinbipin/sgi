////////////////////////////////////////////////////////////////////////
//
// Format.h
//	
//	The constants that define data formats
//
// Copyright 1995, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//
////////////////////////////////////////////////////////////////////////

#ifndef _Format_H
#define _Format_H

#define WIDTH			160
#define HEIGHT			120
#define PIXELS_PER_FRAME	(WIDTH * HEIGHT)

#include <X11/Intrinsic.h>		// For Boolean, TRUE and FALSE

typedef unsigned char Component;

struct ABGRPixel
{
    Component	alpha;
    Component	blue;
    Component	green;
    Component	red;
};


#include <GL/gl.h>

#define PIXEL_FORMAT	GL_RGBA
#define PIXEL_TYPE	GL_UNSIGNED_BYTE
#define COMPONENT_COUNT	4

#endif // _Format_H
