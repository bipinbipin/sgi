////////////////////////////////////////////////////////////////////////
//
// Movie.h
//	
//	A simple movie object: opens a movie file and reads images.
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

#ifndef _Movie_H
#define _Movie_H

#include <sys/types.h>		// size_t
#include <dmedia/dm_params.h>	// DMparams
#include <dmedia/moviefile.h>	// MVtime, MVtimescale

class Movie
{
public:
    static Movie* create(
	const char*	fileName,
	DMparams*	imageParams,
	MVtimescale	timescale,
	MVtime		frameDuration 
	);
    
    virtual ~Movie();

    virtual void writeImage(
	void*		buffer,
	size_t		bufferSize,
	DMparams*	bufferParams,
	int		startFrameNumber,
	int		endFrameNumber
	) = 0;
};

#endif // _Movie_H
