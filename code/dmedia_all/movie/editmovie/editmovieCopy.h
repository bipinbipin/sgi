/******************************************************************************
 *
 * File:        editmovieCopy.h
 *
 * Description: Declarations for public functions in editmovieCopy.c.
 *
 *****************************************************************************/

#include <dmedia/moviefile.h>

extern void copyImageFrames( MVid theTrack, int firstFrame, int numFrames );

extern void copyAudioFrames( MVid theTrack, int firstFrame, int numFrames );
