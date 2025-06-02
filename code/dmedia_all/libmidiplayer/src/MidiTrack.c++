/************************************************************************\
 * 	File:  		MidiTrack.c++                              	*
 * 	Authors:        John Kraft & Bryan James  			*
 *			Archer Sully & Jordan Slott 			*
 *                      Digital Media Systems                    	*
 *			Silicon Graphics, Inc.				*
 *	Date:           December 4, 1995				*
 *									*
 * Copyright (c) 1995, Silicon Graphics, Inc. All Rights Reserved.  	*
\************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <memory.h>
#include <values.h>
#include <dmedia/midi.h>

#include "MPfile.h"


/* 
 * MidiTrack constructor
 */

MidiTrack::MidiTrack()
{
    _head         = NULL;
    _tail         = NULL;
    _length       = 0;
    _name	  = NULL;    
}


/* 
 * MFtrack destructor
 */

MidiTrack::~MidiTrack()
{
    // Delete all of the attached MDevents
    MidiEvent *entry = _head;
    MidiEvent *next;

    while (entry) {
	if (entry->prevTrackEvent) 
	    entry->prevTrackEvent = NULL;
	
	next = entry->nextTrackEvent;
	entry->nextTrackEvent = NULL;
	entry = next;
    }

    free(_name);
}


/*
 * Return the position for the beginning of the track
 */
MidiEvent *
MidiTrack::firstEvent(void)
{
    return _head;
}


/*
 * Return the position of the end of the track 
 */
MidiEvent *
MidiTrack::lastEvent(void)
{
    return _tail;
}


/*
 * Return a pointer to the first event whose stamp is >= the specified
 * tick.
 */
MidiEvent *
MidiTrack::seekEvent(long long tick)
{
    MidiEvent *curr = firstEvent();

    while (curr && curr->event.stamp < tick)
	curr = curr->nextTrackEvent;

    return curr;
}


/*
 * Insert an event into the track.  The event is inserted in 
 * timestamp-ascending order in the linked list to maintain the 
 * invariant that timestamps are monotonically increasing.
 */
void 
MidiTrack::insertEvent(MidiEvent *ev)
{
    if (ev == NULL) 
	return;

    if (_tail == NULL || _tail->event.stamp <= ev->event.stamp) {
	_append(ev);
    }
    else {
	for (MidiEvent *oldev = _head; ev != NULL; ev = ev->nextTrackEvent) {
	    if (ev->event.stamp < oldev->event.stamp) {
		_insertBefore(oldev, ev);
		break;
	    }
	}
    }
}


/*
 * Delete the event at the specified position.  This doesn't actually
 * do anything to the event itself; it just removes it from the track
 * list.
 */
void 
MidiTrack::deleteEvent(MidiEvent *event)
{
    if (event == NULL)
	return;

    MidiEvent *prev = event->prevTrackEvent;
    MidiEvent *next = event->nextTrackEvent;

    if (prev) {
	prev->nextTrackEvent = event->nextTrackEvent;
    } else {
	_head = next;
	if (next)
	    next->prevTrackEvent = NULL;
    }

    if (next) {
	next->prevTrackEvent = event->prevTrackEvent;
    } else {
	_tail = prev;
	if (prev)
	    prev->nextTrackEvent = NULL;
    }

    _length--;
}


/* Return the number of events on the track */
int
MidiTrack::numberEvents()
{
    return _length;
}


/* Boolean function to see if track is empty. */
int MidiTrack::empty()
{
    return (_length == 0);
}


/*
 * Return the name of the track
 */
char* MidiTrack::name(void)
{
    if (_name == NULL) {

	// Search through the event list for name meta events
	for (MidiEvent *ev = _head; ev != NULL; ev = ev->nextTrackEvent) {
	   
	    // When we find an event, we allocate space for the name and
	    // copy it.  According to the MIDI file spec it is possible for
	    // name strings to contain unprintable characters; these need to
	    // be removed.
	    if (mfMetaType(ev) == MIDImeta_Name) {

		// Figure out how much space to allocate.  Theoretically,
		// the meta message has the length in byte 2.
		int len = ev->event.sysexmsg[2];
		if (len > ev->event.msglen - 3)
		    len = ev->event.msglen - 3;

		if ((_name = (char*) malloc(len+1)) == NULL)
			return "Unnamed";

		char *p = _name;
		for (int i = 3; i < len+3; i++) {
		    if (isprint(ev->event.sysexmsg[i]))
			*p++ = ev->event.sysexmsg[i];
		}
		*p = '\0';
		
		break;	// found it, no point in looking further
	    }
        }

	// If we didn't find a name, just use "Unnamed"
	if (_name == NULL)
	    _name = strdup("Unnamed");

    } 

    return _name;
}

	
void MidiTrack::print(FILE *fp)
{
    int notempos = 0;
    int nometas = 0;
    int noevents = 0;

    for (MidiEvent *ev = _head; ev != NULL; ev = ev->nextTrackEvent) {

	if (mfIsMidiEvent(ev)) {
	    noevents++;
	}
	else if (mfIsTempoEvent(ev)) {
	    notempos++;
	}
	else {
	    nometas++;
	}
    }

    fprintf(fp, "\t\tno midi events =  %d\n", noevents);
    fprintf(fp, "\t\tno meta events =  %d\n", nometas);
    fprintf(fp, "\t\tno tempo events = %d\n", notempos);
}


/**************************************************************************
 *	Private Methods
 **************************************************************************/

void MidiTrack::_append(MidiEvent *ev)
{
    if (_tail == NULL) {
	_head = ev;
	ev->nextTrackEvent = NULL;
	ev->prevTrackEvent = NULL;
    }
    else {
	_tail->nextTrackEvent = ev;
	ev->prevTrackEvent = _tail;
	ev->nextTrackEvent = NULL;
    }

    _tail = ev;
    _length++;
}


// Insert the given event after the current entry.
// If currententry is NULL, we append the event to the end of the list.
//
void MidiTrack::_insertBefore(MidiEvent *oldev, MidiEvent *newev)
{
    if (oldev == _head) {
	newev->nextTrackEvent = _head;
	newev->prevTrackEvent = NULL;
	_head->prevTrackEvent = newev;
	_head = newev;
    } else {
	MidiEvent *prev = oldev->prevTrackEvent;

	prev->nextTrackEvent  = newev;
	newev->prevTrackEvent = prev;
	newev->nextTrackEvent = oldev;
	oldev->prevTrackEvent = newev;
    }

    _length++;
}

