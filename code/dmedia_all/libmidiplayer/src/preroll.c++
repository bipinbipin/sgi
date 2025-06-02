/************************************************************************\
 *	File:		preroll.c++					*
 *	Author:		John Kraft					*
 *	Date:		December 3, 1995				*
 *								        *
 *	The pre-roll code attempts to determine what events need to	*
 * 	be transmitted at a particular point in order to restore	*
 *	the playing state for a sequence. For example, let's say that   *
 *	the user stops in the middle of a sequence and then restarts.	*
 *	It may be necessary to retransmit the program change, tempo,	*
 *	controller, and note on events in order for the playback to	*
 *	sound correct.							*
 *									*
\************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <bstring.h>

#include "MPfile.h"

#define UNSET 0xff

struct State {
    char	found;
    char	noteon_searching;
    char	notes[128];		// Note on retransmits
    char	polykey[128];		// Poly key information retransmission
    char	controllers[128];
    
    char	program;
    char	channelpressure[2];
    char	pitchbend[2];
};

/*
 * Build a buffer containing any necessary preroll events, assuming
 * that playback starts with the current event.  We basically scan
 * backwards through all of the tracks looking for events which might
 * need to be retransmitted.
 */

static void clear_state(State state[]);
static void build_state(MidiFile *file, long long, State state[]);
static void add_events(State state[], MDevent **buffer, int *howmany);

MDevent *
MidiFile::getPrerollEvents(int *howmany, long long time)
{
    MDevent *buffer;

    *howmany = 0;

    // Allocate the state array and make sure that all of the channels
    // are cleared the first time around.
    State state[16];
    for (int i = 0; i < 16; i++)
	state[i].found = 1;

    clear_state(state);
    build_state(this, time, state);
    add_events(state, &buffer, howmany);

    return buffer;
}


/*
 * set the state array back to its initial state.
 */

static void clear_state(State state[])
{
    for (int i = 0; i < 16; i++) {

	// If no events were found on this track, we don't need to
	// clear it.
	if (!(state[i].found))
	    continue;

	State *st = &(state[i]);

	// Clear the arrays
	for (int j = 0; j < 128; j++) {
	    
	    st->notes[j]       = UNSET;
	    st->polykey[j]     = UNSET;
	    st->controllers[j] = UNSET;
	}

	st->found   = 0;
	st->noteon_searching = 1;
	st->program = UNSET;
	st->channelpressure[0] = UNSET;
	st->pitchbend[0] = UNSET;
    }
}
	

/*
 * Scan backwards through the given track, looking for events of 
 * interest and logging them in the state array.  Right now,
 * we can from the current pointer location all the way back to the
 * beginning of the track.  After we find the first global event,
 * we stop logging note on and polyphonic key pressure events.
 * We do this because note on events implicitly depend on the 
 * previous controller events.  Since we only log a single set of 
 * controller states, though, we can't accurately reproduce any note 
 * ons which occur before the first logged controller change.  
 *
 * XXX Can this be fixed more cleanly?
 */

static void
build_state(MidiFile *file, long long tick, State state[])
{
    // Now we iterate backwards through all of the events
    MidiEvent *mev = file->seekTick(tick);
    if (mev == NULL)
	return;
    else
	mev = mev->prevEvent;

    while(mev) { 
        MDevent *ev   = &mev->event; 
	int msgtype   = (ev->msg[0] & MD_STATUSMASK);
	int note      = ev->msg[1];
	State *cstate = &(state[ev->msg[0] & MD_CHANNELMASK]);

	// Note that events were found on this track
	cstate->found = 1;

	switch (msgtype) {
	  case MD_NOTEON:
	    // If no note off event was seen, this note must still be playing.
	    // So stuff its velocity into the notes array.
	    if ((cstate->notes[note] == UNSET) && cstate->noteon_searching)
		cstate->notes[note] = ev->msg[2];
	    break;

	  case MD_NOTEOFF:
	    // Put a marker in the notes array to indicate a noteoff was seen
	    if (cstate->notes[note] == UNSET)
		cstate->notes[note] = 0x0;
	    break;

	  case MD_POLYKEYPRESSURE:
	    if ((cstate->polykey[note] == UNSET) && cstate->noteon_searching)
		cstate->polykey[note] = ev->msg[2];
	    break;

	  case MD_CONTROLCHANGE:
	    if (cstate->controllers[note] == UNSET) {
		cstate->controllers[note] = ev->msg[2];
		cstate->noteon_searching = 0;
	    }
	    break;

	  case MD_PROGRAMCHANGE:
	    if (cstate->program == UNSET) {
		cstate->program = ev->msg[1];
		cstate->noteon_searching = 0;
	    }
	    break;

	  case MD_CHANNELPRESSURE:
	    if (cstate->channelpressure[0] == UNSET) {
		cstate->channelpressure[0] = ev->msg[1];
		cstate->channelpressure[1] = ev->msg[2];
		cstate->noteon_searching = 0;
	    }
	    break;

	  case MD_PITCHBENDCHANGE:
	    if (cstate->pitchbend[0] == UNSET) {
		cstate->pitchbend[0] = ev->msg[1];
		cstate->pitchbend[1] = ev->msg[2];
		cstate->noteon_searching = 0;
	    }
	    break;
	}

	mev = mev->prevEvent;
    }
}


/*
 * Using the information in the state array, add some events to the
 * preroll event array.  To make things a little faster, we keep
 * track of whether any events were found on a channel and skip
 * unused channels.
 */

static void add_1(MDevent **buffer, char msg[], int *bufsize, int *howmany);

static void add_events(State state[], MDevent **buffer, int *howmany)
{
    char msg[3];
    int  i;

    int bufsize = 64;
    *howmany = 0;

    if ((*buffer = (MDevent*) malloc(bufsize * sizeof(MDevent))) == NULL) {
	*howmany = 0;
	return;
    }

    for (int chan = 0; chan < 16; chan++) {

	State *st = &(state[chan]);

	if (!st->found)
	    continue;
  	// First add the program change event
	if (st->program != UNSET) {
	    msg[0] = MD_PROGRAMCHANGE | chan;
	    msg[1] = st->program;
	    add_1(buffer, msg, &bufsize, howmany);
	}

	// Now add in all of the controller changes
	for (i = 0; i < 128; i++) {
	    if (st->controllers[i] != UNSET) {
		msg[0] = MD_CONTROLCHANGE | chan; 
		msg[1] = i;
		msg[2] = st->controllers[i];
		add_1(buffer, msg, &bufsize, howmany);
	    }
	}
 
	// Now insert the Polykeypressure and note on events
	for (i = 0; i < 128; i++) {

	    if (st->notes[i] != UNSET && st->notes[i] != 0) {
		msg[0] = MD_NOTEON | chan; 
		msg[1] = i;
		msg[2] = st->notes[i];
		add_1(buffer, msg, &bufsize, howmany);
	    }

	    if (st->polykey[i] != UNSET) {
		msg[0] = MD_POLYKEYPRESSURE | chan; 
		msg[1] = i;
		msg[2] = st->polykey[i];
		add_1(buffer, msg, &bufsize, howmany);
	    }
	}

	// Finally, update the channelpressure and pitchbend
	if (st->channelpressure[0] != UNSET) {
	    msg[0] = MD_CHANNELPRESSURE | chan;
	    msg[1] = st->channelpressure[0];
	    msg[2] = st->channelpressure[1];
	    add_1(buffer, msg, &bufsize, howmany);
	}

	if (st->pitchbend[0] != UNSET) {
	    msg[0] = MD_PITCHBENDCHANGE | chan;
	    msg[1] = st->pitchbend[0];
	    msg[2] = st->pitchbend[1];
	    add_1(buffer, msg, &bufsize, howmany);
	}
    }
}


static void add_1(MDevent **buffer, char msg[], int *bufsize, int *howmany)
{

    // Check to see if the buffer has space for the message
    if ((*howmany + 1) == *bufsize) {
	// We're out of space, so increase the size of the 
	// buffer. 
	*bufsize += 32;
	MDevent **oldbuffer = buffer;
	*buffer = (MDevent*) realloc(*buffer, sizeof(MDevent) * (*bufsize));
	if (*buffer == NULL) {
	    buffer = oldbuffer;
	    fprintf(stderr, "MFfile::add_1: could not realloc buffer\n");
	    return;
	}
    }    
	
    MDevent *ev = &((*buffer)[*howmany]);
    (*howmany)++;
	
    ev->msg[0] = msg[0];
    ev->msg[1] = msg[1];
    ev->msg[2] = msg[2];
    ev->stamp  = 0;
    ev->sysexmsg = NULL;
}

