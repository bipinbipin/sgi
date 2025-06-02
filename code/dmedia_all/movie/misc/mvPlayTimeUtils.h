#ifndef __MOVIEPLAYTIMEUTIL_H__
#define __MOVIEPLAYTIMEUTIL_H__

/*****************************************************************************
 *
 * File:        mvPlayTimeUtils.h
 *
 * Description: Header file for mvPlayTimeUtils.c. Defines a set of time code
 * 		mapping utilities for use with libmovie applications which
 *		perform movie playback.
 *
 *****************************************************************************/

#include <dmedia/moviefile.h>

#ifdef __cplusplus
extern "C" {
#endif

/********
 *
 * MVtimetype
 *
 ********/

typedef enum __MVtimetype {
    MV_TIME_SMPTE_24		= 0,	    /* 24 frames/sec (motion film) */
    MV_TIME_SMPTE_25		= 1,	    /* 25 frames/sec (PAL video) */
    MV_TIME_SMPTE_30		= 2,	    /* 30 frames/sec (NTSC video) */
    MV_TIME_SMPTE_D30		= 3	    /* 30 frames/sec drop (NTSC 29.97) */
} MVtimetype;

extern DMstatus mvStringToFrame	    ( MVid		movieid,
				      char		*timestring,
				      MVtimetype	timetype,
				      MVframe		*framereturn ); 

extern DMstatus mvFrameToString	    ( MVid		movieid,
				      MVframe		frame,
				      MVtimetype	timetype,
				      char		*timereturn ); 

extern DMstatus mvTimeToFrame	    ( MVid		movieid,
				      int		hour,
				      int		minute,
				      int		second,
				      MVframe		framecnt,
				      MVtimetype	timetype,
				      MVframe		*framereturn);

extern DMstatus mvFrameToTime	    ( MVid		movieid,
				      MVframe		frame,
				      MVtimetype	timetype,
				      int		*hourreturn,
				      int		*minutereturn,
				      int		*secondreturn,
				      MVframe		*framecntreturn);

#ifdef __cplusplus
}
#endif
#endif /* !__MOVIEPLAYTIMEUTIL_H__ */
