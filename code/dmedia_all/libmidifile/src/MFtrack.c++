/////////////////////////////////////////////////////////////////////////////
// File:                   MFtrack.c++                                     //
// Author:                 Archer Sully and Jordan M. Slott                //
//                         archer@esd.sgi.com or hordack@esd.sgi.com       //
//                         Digital Media Systems                           //
//                         Silicon Grahpics, Inc.                          //
// Date:                   August 4, 1995                                  //
// Description:            Implements the MFtrack class, a  storage class  //
//                         for a MIDI file track.                          //
//                                                                         //
// Copyright (c) August 1995, Silicon Graphics, Inc. All Rights Reserved.  //
/////////////////////////////////////////////////////////////////////////////

// Standard Include Files
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <values.h>

#ifdef DEBUG
#define DB(stmt) {stmt;}
#else
#define DB(stmt)
#endif

// MIDI File/Track Include Files
#include <dmedia/midi.h>

#include "midifile.h"

///////////////////////////////////////////////////////////////////////////
// MFentry Class                                                         //
///////////////////////////////////////////////////////////////////////////
class MFentry {
  public:
    MFentry(MFfileEvent *data);

    MFfileEvent    *_data;
    MFentry        *_prev;
    MFentry        *_next;
};

MFentry::MFentry(MFfileEvent *data)
{
  _data = data;
  _prev = NULL;
  _next = NULL;
}


//////////////////////////////////////////////////////////////////////////
// Constructor                                                          //
//////////////////////////////////////////////////////////////////////////
MFtrack::MFtrack()
{
  _head         = (MFentry *)NULL;
  _tail         = (MFentry *)NULL;
  _currententry = (MFentry *)NULL;
  _length       = 0;
}

//////////////////////////////////////////////////////////////////////////
// Destructor                                                           //
//////////////////////////////////////////////////////////////////////////
MFtrack::~MFtrack()
{
    // delete all entries and their associated data

    MFentry *e = _tail;
    while(e) {
	MFentry *prev = e->_prev;
	DB(printf("deleting e->_data 0x%x\n", e->_data));
	assert(e->_data != NULL);
	delete e->_data;
	e->_data = NULL;
	DB(printf("deleting e 0x%x\n", e));
	delete e;
	e = prev;
    }
    _head = NULL;
    _tail = NULL;
    _currententry = NULL;
    _length = 0;
    DB(printf("deleting this = 0x%x\n", this));
}

//////////////////////////////////////////////////////////////////////////
// MFtrack::seekbeginning(): Moves internal pointer to the beginning    //
//////////////////////////////////////////////////////////////////////////
MFfileEvent *MFtrack::seekBeginning()
{
  return(_first());
}

//////////////////////////////////////////////////////////////////////////
// MFtrack *MFtrack::seekend(): Goes to and returns the last event      //
//////////////////////////////////////////////////////////////////////////
MFfileEvent *MFtrack::seekEnd(void)
{
  return(_last());
}

//////////////////////////////////////////////////////////////////////////
// MFtrack::insertevent(MFfileEvent *ev): Insert an event into the track//
//    The event is inserted into a sorted order in the linked list to   //
//    maintain the invariant that the linked list contains events in    //
//    a non-decreasing time-order. Note that a new event comes after    //
//    all events of the same time.                                      //
//////////////////////////////////////////////////////////////////////////
MFfileEvent *MFtrack::insertEvent(MFfileEvent *ev)
{
  // Insert events in time order. Note that this routine optimizes for
  // the common case that we want to insert events at the end of the
  // track.
  if (ev != NULL) {
    MFfileEvent *oldev = _last();
    if (oldev == NULL) {
      _append(ev);
    }
    else {
      for (; oldev != NULL; oldev = _previous()) {
        if (ev->eventStamp() >= oldev->eventStamp()) {
          _insert(ev);
          return(ev);
        }
      }
    }
  }
  return((MFfileEvent *)NULL);
}

///////////////////////////////////////////////////////////////////////////
// MFtrack::deleteevent(MFfileEvent *ev): Removes the member ev          //
//    This looks for the actual pointer associated with ev.              //
///////////////////////////////////////////////////////////////////////////
MFfileEvent *MFtrack::deleteEvent(MFfileEvent *ev)
{
  return(_remove(ev));
}

MFfileEvent *MFtrack::seekTime(unsigned long long time)
{
    unsigned long long stamp;

    // Fast path #1: seek to beginning of track
    if (time == 0) {
	_first();
	goto _myend;
    }
    
    // Slow, but maybe it will work....
    _first();
    stamp = 0;
    while (_currententry != NULL) {
	stamp = _currententry->_data->eventStamp();
	if (stamp >= time)
       	    goto _myend;
	_currententry = _currententry->_next;
    }
_myend:
    return(_current());
}
 
/*
 * XXX what happens here when we don't find a Meta event?
 * 
 * Let's say we start at beginning of the file and there are
 * no meta events. This code would indicate that we spin all
 * the way to the end of the file and then return a NULL event.
 */
MFfileEvent *MFtrack::seekMetaEvent(MIDImetaEvents type)
{
  for (MFfileEvent *ev = _first(); ev != NULL; ev = _next()) {
    if (ev->eventType() == MF_META_EVENT) {
      if ((ev->metaEvent())->type == type) {
        return(ev); 
      }
    }
  }
  return((MFfileEvent *)NULL);
}

MFfileEvent *MFtrack::nextMetaEvent(MIDImetaEvents type)
{
  if (_current() == NULL) {
    return(NULL);
  }

  for (MFfileEvent *fev = _next(); fev != NULL; fev = _next()) {
    if (fev->eventType() == MF_META_EVENT) {
      if ((fev->metaEvent())->type == type) {
	return(fev);
      }
    }
  }
  return(NULL);
}

MFfileEvent *MFtrack::prevMetaEvent(MIDImetaEvents type)
{
    MFentry *p;
    MFfileEvent *ev;

    p = _currententry;	// grab the current location
    if (p == NULL || p->_prev == NULL) return NULL;

    p = p->_prev;
    ev = p->_data;
    while (p != NULL && p->_prev != NULL && ev != NULL) {
	if (ev->eventType() == MF_META_EVENT &&
	   (ev->metaEvent())->type == type)
		return ev;
	p = p->_prev;
	ev = p->_data;
    }
    return NULL;
}

MFfileEvent *MFtrack::seekMidiEvent(int type)
{
  for (MFfileEvent *fev = _first(); fev != NULL; fev = _next()) {
    if (fev->eventType() == MF_MIDI_EVENT) {
      MDevent *ev = fev->midiEvent();
      int eventtype;
      if (mfIsChannelVoice(fev) == True || mfIsChannelMode(fev) == True) {
	eventtype = ev->msg[0] & MD_STATUSMASK;
      }
      else {
	eventtype = ev->msg[0];
      }
      if (eventtype == type) {
        return(fev); 
      }
    }
  }
  return((MFfileEvent *)NULL);
}

MFfileEvent *MFtrack::nextMidiEvent(int type)
{
  if (_current() == NULL) {
    return(NULL);
  }

  for (MFfileEvent *fev = _next(); fev != NULL; fev = _next()) {
    if (fev->eventType() == MF_MIDI_EVENT) {
      MDevent *ev = fev->midiEvent();
      int eventtype;
      if (mfIsChannelVoice(fev) == True || mfIsChannelMode(fev) == True) {
	eventtype = ev->msg[0] & MD_STATUSMASK;
      }
      else {
	eventtype = ev->msg[0];
      }
      if (eventtype == type) {
        return(fev); 
      }
    }
  }
  return((MFfileEvent *)NULL);
}


////////////////////////////////////////////////////////////////////////////
// MFfileEvent *MFtrack::seekeevent(MFfileEvent *ev): Seeks to a particular 
// event
////////////////////////////////////////////////////////////////////////////
MFfileEvent *MFtrack::seekEvent(MFfileEvent *ev)
{
  for (MFfileEvent *fev = _first(); fev != NULL; fev = _next()) {
    if (ev = fev) {
      return(ev);
    }
  }
  return(NULL);
}

/////////////////////////////////////////////////////////////////////////////
// MFfileEvent *MFtrack::seeknthevent(int n): Seeks to the nth event in the//
//    track, where the first event is 1.                                   //
/////////////////////////////////////////////////////////////////////////////
MFfileEvent *MFtrack::seekNthEvent(unsigned int n)
{
  return(_nth(n));
}

/////////////////////////////////////////////////////////////////////////////
// MFfileEvent *MFtrack::nextevent(): Jumps to the next event. Returns the //
//    nextevent.                                                           //
/////////////////////////////////////////////////////////////////////////////
MFfileEvent *MFtrack::nextEvent()
{
  return(_next());
}

////////////////////////////////////////////////////////////////////////////
// MFfileEvent *MFtrack::currentevent(): Returns the current event        //
////////////////////////////////////////////////////////////////////////////
MFfileEvent *MFtrack::currentEvent()
{
  return(_current());
}

////////////////////////////////////////////////////////////////////////////
// MFfileEvent *MFtrack::prevevent(): Jumps to the next event. Returns the//
//    previous event.                                                     //
////////////////////////////////////////////////////////////////////////////
MFfileEvent *MFtrack::prevEvent()
{
  return(_previous());
}

unsigned long long MFtrack::currentTimeStamp(int *howmany)
{
  MFfileEvent *tmp = _current();

  unsigned long long retrn;
  *howmany = 0;
  if (_current() != NULL) { 
    retrn = _current()->eventStamp();
    *howmany = 1;
  }
  else {
    return MAXINT;
  }
 
  MFentry *entry = _currententry;
  for (tmp = _next(); tmp != NULL; tmp = _next()) {
    if (tmp->eventStamp() != retrn) {
      break;
    }
    (*howmany)++;
  }
  
  _currententry = entry;
  return retrn;
}

////////////////////////////////////////////////////////////////////////////
// unsigned long long MFtrack::lasttimestamp(): Retuns the greatest time  //
//    stamp in the list. We just need to return the stamp of the last     //
//    element because we maintain the invariant that the list is sorted.  //
////////////////////////////////////////////////////////////////////////////
long long MFtrack::lastTimeStamp(int *howmany)
{
    MFentry *p;
    MFfileEvent *ev;
    long long time = 0;

    p = _tail;
    if (p == NULL) return -1;

    while (p && p->_data) {
	if (p->_data->eventType() == MF_MIDI_EVENT) {
	    time = p->_data->eventStamp();
	    break;
	} else {
	    p = p->_prev;
	}
    }

    return time;
}

////////////////////////////////////////////////////////////////////////////
// unsigned long long MFtrack::firsttimestamp(): Retuns the smallest time //
//    stamp in the list. We just need to return the stamp of the last     //
//    element because we maintain the invariant that the list is sorted.  //
////////////////////////////////////////////////////////////////////////////
unsigned long long MFtrack::firstTimeStamp(int *howmany)
{
  MFfileEvent *old = _current();
  MFfileEvent *tmp = _first();

  unsigned long long retrn;
  *howmany = 0;
  if (_current() != NULL) {
    retrn = _current()->eventStamp();
  }
  else {
    return MAXINT;
  }

  for (tmp = _previous(); tmp != NULL; tmp = _previous()) {
    if (tmp->eventStamp() != retrn) {
      break;
    }
    (*howmany)++;
  }

  _seek(old);
  return(retrn);
}

int MFtrack::numberEvents()
{
  return(_length);
}

int MFtrack::empty()
{
  return(_length == 0);
}

void MFtrack::print()
{
  seekBeginning();
  int notempos = 0;
  int nometas = 0;
  int noevents = 0;
  for (MFfileEvent *fev = _current(); fev != NULL; fev = _next()) {
    if (fev->eventType() == MF_MIDI_EVENT) {
      noevents++;
    }
    else if ((fev->metaEvent())->type == MIDImeta_SetTempo) {
      notempos++;
    }
    else {
      nometas++;
    }
  }
  fprintf(stderr, "\t\tno midi events =  %d\n", noevents);
  fprintf(stderr, "\t\tno meta events =  %d\n", nometas);
  fprintf(stderr, "\t\tno tempo events = %d\n", notempos);
}


////////////////////////////////////////////////////////////////////////////
// Private Methods Dealing with the List of Events                        //
////////////////////////////////////////////////////////////////////////////

void MFtrack::_append(MFfileEvent *data)
{
  MFentry *entry = new MFentry(data);
  if (_tail == NULL) {
    _head = entry;
  }
  else {
    entry->_prev = _tail;
    _tail->_next = entry;
  }
  _tail = entry;
  _length++;
}

void MFtrack::_insert(MFfileEvent *data)
{
  if (_currententry != NULL && _currententry != _tail) {
    MFentry *newentry = new MFentry(data);
    if (_currententry != _tail) {
      (_currententry->_next)->_prev = newentry;
    }
    newentry->_next = _currententry->_next;
    newentry->_prev = _currententry;
    _currententry->_next = newentry;
    _length++;
  }
  else {
    _append(data);
  }
  return;
}

MFfileEvent *MFtrack::_remove(MFfileEvent *data)
{
  MFentry *oldentry;
  for (oldentry = _head; oldentry != NULL; oldentry = oldentry->_next) {
    if (oldentry->_data == data) {
      break;
    }
  }
  if (oldentry != NULL) {
    MFentry *preventry = oldentry->_prev;
    MFentry *nextentry = oldentry->_next;

    if (nextentry != NULL) {
      nextentry->_prev = preventry;
    }
    if (preventry != NULL) {
      preventry->_next = nextentry;
    }
    if (oldentry == _tail) {
      _tail = preventry;
    }
    if (oldentry == _head) {
      _head = nextentry;
    }
    _length--;
  }
  return(data);
}

MFfileEvent *MFtrack::_nth(int n)
{
  MFentry *entry = _head;
  for (int i = 1; i < n; i++) {
    if (entry == NULL) {
      return(NULL);
    }
    entry = entry->_next;
  }
  return(entry->_data);
}

MFfileEvent *MFtrack::_previous()
{
  if (_currententry != NULL) {
    _currententry = _currententry->_prev;
    if (_currententry == NULL) {
      return(NULL);
    }
  }
  return(_currententry->_data);
}

MFfileEvent *MFtrack::_next()
{
  if (_currententry != NULL) {
    if ((_currententry = _currententry->_next) == NULL) {
      return(NULL);
    }
  }
  return(_currententry->_data);
}

MFfileEvent *MFtrack::_first()
{
  _currententry = _head;
  if (_currententry == NULL) {
    return(NULL);
  }
  return(_currententry->_data);
}

MFfileEvent *MFtrack::_last()
{
  _currententry = _tail;
  if (_currententry == NULL) {
    return(NULL);
  }
  return(_currententry->_data);
}

MFfileEvent *MFtrack::_current()
{
  if (_currententry == NULL) {
    return(NULL);
  }
  return(_currententry->_data);
}

void MFtrack::_seek(MFfileEvent *ev)
{
  for (MFentry *entry = _head; entry != NULL; entry = entry->_next) {
    if (entry->_data == ev) {
      _currententry = entry;
      return;
    }
  }
}
