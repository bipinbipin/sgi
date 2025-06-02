/************************************************************************\
 *	File:		MFtempomap.c++					*
 *	Author:		John Kraft (jfk@sgi.com)			*
 *									*
 *	Provides routines for manipulating and computing with a		*
 *	tempo map.  							*
 *									*
 *    Copyright (C) 1995 Silicon Graphics Inc.  All rights reserved.	*
\************************************************************************/

#include <malloc.h>
#include "MPfile.h"
#include "MPtempomap.h"

struct _TempoEntry {
    long long	tick;
    long long	time;
    long long   picosPerTick;
    long long	tempo;
};


static const int DEFAULT_TEMPO = 500000;

/* Constructor */
TempoMap::TempoMap(int division)
{
    _tempoMap     = NULL;
    _numTempos    = 0;
    _numAllocated = 0;
    _division     = division;
    
    // Initialize the default tempo entry
    _defaultEntry = new _TempoEntry;
    _defaultEntry->time  = 0;
    _defaultEntry->tick  = 0;
    _defaultEntry->tempo = 500000;
    _defaultEntry->picosPerTick = _defaultEntry->tempo * 1000000 / _division;
}


/* Destructor */
TempoMap::~TempoMap()
{
    free(_tempoMap);
}


/* Add a single tempo to the map */
int
TempoMap::addTempo(long long tick, int usecs)
{
    if (_addTempo(tick, usecs) == -1) {
	return -1;
    } else {
	_updateTimes();
	return 0;
    }
}


/* Add a single tempo from a tempo meta event */
int
TempoMap::addTempo(MidiEvent *msg)
{
    if (mfIsTempoEvent(msg)) {
	return addTempo(msg->event.stamp, mfGetTempo(msg));
    } else
	return -1;
}


/* Add all the tempo events found in the given track */
int
TempoMap::addAllTempos(MidiTrack *track)
{
    for (MidiEvent *ev = track->firstEvent(); ev != NULL; 
	 ev = ev->nextTrackEvent) {
	if (mfIsTempoEvent(ev) && 
	    _addTempo(ev->event.stamp, mfGetTempo(ev)) == -1)
	    return -1;
    }

    _updateTimes();
    return 0;
}


/* Remove a tempo at a given tick from the tempo map */
int
TempoMap::removeTempo(long long tick)
{
    for (int pos = 0; pos < _numTempos; pos++) {
	if (_tempoMap[pos].tick == tick) 
	    break;
    }

    if (pos == _numTempos) {
	// We didn't find the tempo entry
	return -1;
    }
    else {
	// Collapse the array by one entry
	for (int i = pos; i < _numTempos-1; i++) 
	    _tempoMap[i] = _tempoMap[i+1];

	_numTempos--;

	return 0;
    }
}	


/* Return the number of tempo entries currently in the map */
int
TempoMap::numTempos()
{
    return _numTempos;
}


/* Return the tick value of the nth tempo event */
long long 
TempoMap::nthTempoTick(int n)
{	
    if (n >= 0 && n < _numTempos)
	return _tempoMap[n].tick;
    else 
	return -1;
}


/* Return the time of the nth tempo event */
long long
TempoMap::nthTempoTime(int n)
{ 
    if (n >= 0 && n < _numTempos)
	return _tempoMap[n].time;
    else
	return -1;
}


/* Return the actual nth tempo value */
int
TempoMap::nthTempo(int n)
{
    if (n >= 0 && n < _numTempos)
	return _tempoMap[n].tempo;
    else
	return -1;
}


/* 
 * Return the first tempo in the map.  If the map is empty, return
 * the default tempo.
 */
int
TempoMap::firstTempo(void)
{
    if (_numTempos > 0 && _tempoMap[0].tick == 0)
	return _tempoMap[0].tempo;
    else
	return DEFAULT_TEMPO;
}


/* Return the tempo at the relative time indicated by tick. */
int 
TempoMap::tempoAtTick(long long tick)
{
    _TempoEntry *t = _entryAtTick(tick);
    return t->tempo;
}


/* Return the tempo at the specified time. */
int
TempoMap::tempoAtTime(long long time)
{
    _TempoEntry *t = _entryAtTime(time);
    return t->tempo;
}


/*
 * Convert a time value to a ticks value.  This is more complicated
 * than simply doing a division, since we need to know the tempo
 * in effect.
 */
long long
TempoMap::timeToTicks(long long time)
{
    _TempoEntry *t = _entryAtTime(time);

    // To avoid problems with truncation, we do all of our calculations
    // in picoseconds
    long long deltaTime  = 1000 * (time - t->time);
    long long deltaTicks = deltaTime / t->picosPerTick;

    return t->tick + deltaTicks;
}


long long
TempoMap::ticksToTime(long long ticks)
{
    _TempoEntry *t = _entryAtTick(ticks);

    long long deltaTime = (ticks - t->tick) * t->picosPerTick / 1000;

    return t->time + deltaTime;
}


/************************************************************************
 *		Private methods						*
 ************************************************************************/

/*
 * Add a tempo to the tempo map.  This inserts in place; while this can
 * be expensive (O(N^2)) if the tempos are inserted in random order,
 * we realistically expect the tempos to be inserted in sorted order
 * as a rule.  
 */
int
TempoMap::_addTempo(long long tick, int usecs)
{
    // Figure out where to insert the new tempo.  Since we expect tempos
    // to typically be inserted in sorted order, we fastpath this case.
    int pos;
    if (_numTempos && tick >= _tempoMap[_numTempos-1].tick) {
	pos = _numTempos;
    } else {
	for (pos = 0; pos < _numTempos; pos++) 
	    if (tick < _tempoMap[pos].tick) 
		break;
    }

    // We need to allocate more space.  To keep space allocation from
    // becoming too expensive, we allocate in blocks of 20.
    if (_numTempos == _numAllocated) {
	_numAllocated += 20;
	_TempoEntry *old = _tempoMap;
	_tempoMap = (_TempoEntry*) realloc(_tempoMap, 
					  sizeof(_TempoEntry) * _numAllocated);
	if (_tempoMap == NULL) {
	    _tempoMap = old;
	    return -1;
	}
    }

    // Open up a space in the array for the new tempo
    for (int i = _numTempos; i > pos; i--) 
	_tempoMap[i] = _tempoMap[i-1];

    // And stuff the tempo in
    _tempoMap[pos].tick  = tick;
    _tempoMap[pos].tempo = usecs;

    _numTempos++;
    return 0;
}


/*
 * This routine goes through the tempos and updates the time fields to
 * the time at which the tempo change takes effect.
 */
void
TempoMap::_updateTimes(void)
{
    _TempoEntry *prevEntry = _defaultEntry;

    for (int i = 0; i < _numTempos; i++) {
	long long deltaTicks   = _tempoMap[i].tick - prevEntry->tick;
	long long deltaTime    = prevEntry->picosPerTick * deltaTicks / 1000;

	_tempoMap[i].time         = prevEntry->time + deltaTime;
	_tempoMap[i].picosPerTick = _tempoMap[i].tempo * 1000000 / _division;

	prevEntry = &_tempoMap[i];
    }
}


/* Find the _tempoEntry at the given time and return it */
_TempoEntry *
TempoMap::_entryAtTime(long long time)
{
    if (_numTempos == 0 || time < _tempoMap[0].time)
	return _defaultEntry;

    for (int i = 1; i < _numTempos; i++) 
	if (time < _tempoMap[i].time)
	    return &_tempoMap[i-1];

    return &_tempoMap[_numTempos-1];
}


/* Find the _TempoEntry of the tempo event for a given tick and return it */
_TempoEntry *
TempoMap::_entryAtTick(long long tick)
{
    if (_numTempos == 0 || tick < _tempoMap[0].tick)
	return _defaultEntry;

    for (int i = 1; i < _numTempos; i++)
	if (tick < _tempoMap[i].tick)
	    return &_tempoMap[i-1];

    return &_tempoMap[_numTempos-1];
}










