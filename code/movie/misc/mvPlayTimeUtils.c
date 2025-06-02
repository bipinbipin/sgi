/*****************************************************************************
 *
 * File:        mvPlayTimeUtils.c
 *
 * Description: Sample playback time code manipulation routines. Source for four
 *		functions, mvStringToFrame(), mvFrameToString(),
 *		mvTimeToFrame(), and mvFrameToTime() are included.
 *
 * Functions:   SGI Movie Library functions used:
 *
 *		mvFindTrackByMedium()
 *		mvGetTrackLength()
 *		mvGetCurrentFrame()
 *   		mvGetImageRate()
 *
 *****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <audio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <dmedia/moviefile.h>
#include "mvPlayTimeUtils.h"

/*********
 *
 * Private boolean function used below.
 *
 ********/

static DMboolean isWholeNum( char* input )
{
    char* p;

    for ( p = input; *p != 0; p++ ) {
	 if ( !isdigit( *p ) ) return DM_FALSE;
    }

    return DM_TRUE;
}	

/***********************************************************************
 *
 * Public API calls follow
 *
 ***********************************************************************/

/*********
 *
 * Find the frame corresponding to the SMPTE time string "timeString". 
 *
 ********/

DMstatus mvStringToFrame( MVid theMovie, char *timeString, MVtimetype timeType,
			 MVframe *frameReturn)
{
    int		hour     = 0; 
    int 	minute   = 0;
    int 	second   = 0;
    MVframe 	frameCnt = 0;
    MVframe 	newFrame; 
    MVframe 	frame; 
    MVframe	maxFrame;
    MVid 	videoTrack;
    short 	tmpi = 0; 
    short	tmpi2 = 0;
    char 	*tmp[4];

    assert( timeString != NULL );

    assert( timeType == MV_TIME_SMPTE_24 ||
	   timeType == MV_TIME_SMPTE_25 ||
	   timeType == MV_TIME_SMPTE_30 ||
	   timeType == MV_TIME_SMPTE_D30 );

    /*
     * For now, this routine is only implemented for smpte_30.
     */

    assert( timeType == MV_TIME_SMPTE_30 );

    /*
     * Determine the number of video frames in the movie
     */

    if ( mvFindTrackByMedium( theMovie, DM_IMAGE, &videoTrack ) != DM_SUCCESS ) {
	return DM_FAILURE;
    }
    maxFrame = mvGetTrackLength( videoTrack );

    for ( tmpi = 0; tmpi < 4; tmpi++ ) {
	 tmp[ tmpi ] == NULL;
    }
    tmpi = 0;

    /*
     * Get current time, so we have defaults for unspecified parts.
     */

    frame = mvGetCurrentFrame( theMovie );
    mvFrameToTime( theMovie, frame, MV_TIME_SMPTE_30,
		  &hour, &minute, &second, &frameCnt );
    
    /*
     * Break up the string into chunks.
     */
    tmp[ tmpi++ ] = ( char* ) strtok( timeString, ":" );
    while ( tmp[ tmpi ] = ( char* ) strtok( NULL, ":" ) ) {
	tmpi++;

	/* 
	 * Check if the user typed too many elements.
	 */

	if ( tmpi > 4 ) return DM_FAILURE;
    }
    
    for ( tmpi2 = 0; tmpi2 < tmpi; tmpi2++ ) {
	if ( !isWholeNum( tmp[ tmpi2 ] ) ) {
	    return DM_FAILURE;                /* Not a number */
        }
    }

    /*
     * Assign chunks into our SMPTE time spec...
     */

    tmpi2 = 0;
    switch ( tmpi ) {
    case 4:
	hour = atoi( tmp[ tmpi2++ ] );
    case 3:
	minute = atoi( tmp[ tmpi2++ ] );
    case 2:
	second = atoi( tmp[ tmpi2++ ] );
    case 1:
	frameCnt = atoi( tmp[ tmpi2 ] );
    }

    /*
     * If the second and/or frame counts are completely bogus,
     * ignore them.
     */

    if ( ( second > 59 ) || ( frameCnt > 29 ) ) {
	return DM_FAILURE;
    }

    /*
     * If any of the specs are negative, ignore them.
     */

    if ( ( hour < 0 ) || ( minute < 0 ) ||
	( second < 0 ) || ( frameCnt < 0 ) ) {
        return DM_FAILURE;
    }

    /*
     * Finally, calculate the frame number and return it.
     */

    if ( mvTimeToFrame( theMovie, hour, minute, second, frameCnt,
		       MV_TIME_SMPTE_30, &newFrame ) != DM_SUCCESS ) {
	return DM_FAILURE;
    }
    if ( newFrame < 0 ) {
	newFrame = 0;
    }
    if ( newFrame >= maxFrame ) {
        newFrame = maxFrame - 1;
    }
    *frameReturn = newFrame;

    return DM_SUCCESS;
}

/*********
 *
 * Find the time corresponding to the frame, return the time in a string. 
 *
 ********/

DMstatus mvFrameToString( MVid theMovie, MVframe frame, MVtimetype timeType,
		     char *timeReturn )
{
    int 	hour     = 0;
    int  	minute   = 0;
    int  	second   = 0;
    MVframe 	frameCnt = 0;

    assert( timeType == MV_TIME_SMPTE_24 ||
	   timeType == MV_TIME_SMPTE_25 ||
	   timeType == MV_TIME_SMPTE_30 ||
	   timeType == MV_TIME_SMPTE_D30 );

    assert( timeReturn != NULL );

    if ( mvFrameToTime( theMovie, frame, timeType,
		       &hour, &minute, &second, &frameCnt ) != DM_SUCCESS ) {
	return DM_FAILURE;
    }

    sprintf( timeReturn, "%.2d:%.2d:%.2d:%.2d",
	    hour, minute, second, frameCnt);

    return DM_SUCCESS;
}

/*********
 *
 * Find the frame corresponding to the time code. 
 *
 ********/

DMstatus mvTimeToFrame( MVid theMovie, int hour, int minute,
		       int second, MVframe frameCnt, MVtimetype timeType,
		       MVframe *frameReturn )
{
    long 	totalSecs = 0;
    MVid 	videoTrack;
    double 	framesPerSec;

    assert( timeType == MV_TIME_SMPTE_24 ||
	   timeType == MV_TIME_SMPTE_25 ||
	   timeType == MV_TIME_SMPTE_30 ||
	   timeType == MV_TIME_SMPTE_D30 );

    /*
     * Determine movie's frames per second count.
     */

    if ( mvFindTrackByMedium( theMovie, DM_IMAGE, &videoTrack ) != DM_SUCCESS ) {
	return DM_FAILURE;
    }
    framesPerSec = mvGetImageRate( videoTrack );

    /*
     * Calcluate and return corresponding frame.
     */

    switch ( timeType ) {
    	case MV_TIME_SMPTE_30:
	    totalSecs = ( hour * 3600 ) + ( minute * 60 ) + second;
	    *frameReturn = ( ( MVframe ) ( ( totalSecs * framesPerSec ) +
		           ( ( double ) frameCnt / ( 30.0 / framesPerSec ) ) ) );
	    break;
    	case MV_TIME_SMPTE_24:
    	case MV_TIME_SMPTE_25:
    	case MV_TIME_SMPTE_D30:
    	default:

	/* 
	 * Other cases haven't been implemented yet.
         */

	assert( timeType == MV_TIME_SMPTE_30 );
	break;
    }
    return DM_SUCCESS;
}


/*********
 *
 * Find the time code corresponding to the frame. 
 *
 ********/

DMstatus mvFrameToTime( MVid theMovie, MVframe frame, MVtimetype timeType,
		       int *hourReturn, int *minuteReturn,
		       int *secondReturn, MVframe *frameCntReturn )
{
    long 	totalSecs = 0; 
    long 	fracSecs  = 0;
    MVid 	videoTrack;
    double 	framesPerSec;

    /*
     * Determine movie's frames per second count.
     */

    if ( mvFindTrackByMedium( theMovie, DM_IMAGE, &videoTrack ) != DM_SUCCESS ) {
	return DM_FAILURE;
    }
    framesPerSec = mvGetImageRate( videoTrack );

    if ( framesPerSec > 0.0 ) {
	totalSecs = ( long ) ( frame / framesPerSec );
    }

    /*
     * Calculate the SMPTE framecount.
     */

    switch ( timeType ) {
    	case MV_TIME_SMPTE_30:
	    if ( framesPerSec > 1.0 )  {
	        fracSecs = frame % ( long ) framesPerSec;
	        ( *frameCntReturn ) = fracSecs * ( long ) ( 30 / framesPerSec );
	    }
	    else {
	        ( *frameCntReturn ) = 0;
	    }

	    ( *hourReturn ) = totalSecs / 3600;
	    totalSecs -= ( *hourReturn ) * 3600;
	    ( *minuteReturn ) = totalSecs / 60;
	    totalSecs -= ( *minuteReturn ) * 60;
	    ( *secondReturn ) = totalSecs;
	    totalSecs -= ( *secondReturn );
	    break;
    	case MV_TIME_SMPTE_24:
    	case MV_TIME_SMPTE_25:
    	case MV_TIME_SMPTE_D30:
    	default:
	/* 
	 * Other cases haven't been implemented yet.
	 */
	assert( timeType == MV_TIME_SMPTE_30 );
	break;
    }
    return DM_SUCCESS;
}
