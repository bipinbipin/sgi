/*****************************************************************************
 * GetAudioHardwareInputRate:    acquire audio hardware input sampling rate
 * GetAudioHardwareOutputRate	acquire audio hardware output sampling rate	
 * AudioOutputHardwareInUse    any output ports open or monitor on?  
 *
 *
 *		Written by Chris Pirazzi, Scott Porter, Gints Klimanis
 *			    1991-
 *****************************************************************************/
#include "al.h"
#include <audio.h>

/*
 * These routines expect to be run with AL error handler shut off to prevent
 *  spew of error messages to shell.
 * (call ALseterrorhandler(0)).
 */

/* ******************************************************************
 * GetAudioHardwareInputRate:    acquire audio hardware input or output sampling rate
 * ****************************************************************** */
double
GetAudioHardwareRate(int res)
{
    ALpv pv;

    pv.param = AL_RATE;
    if (alGetParams(res, &pv, 1) < 0) {
	return -1.0;	    /* negative value indicates error */
    }
    return alFixedToDouble(pv.value.ll);
}   /* ---- end GetAudioHardwareRate() ---- */

/* ******************************************************************
 * AudioOutputHardwareInUse    any output ports open or monitor on?  
 * ****************************************************************** */
int
AudioOutputHardwareInUse(int res)
{
    ALpv pv;

    pv.param = AL_PORT_COUNT;
    if (alGetParams(res, &pv, 1) < 0) {
	return 0;
    }
    if (pv.value.i == 0) {
	return 0;
    }
    
    return 1;
} /* ---- end AudioOutputHardwareInUse() ---- */
