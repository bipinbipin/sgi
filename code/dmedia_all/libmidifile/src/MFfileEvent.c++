////////////////////////////////////////////////////////////////////////////
// File:        MFfileEvent.c++                                           //
// Author:      Jordan M. Slott                                           //
//              Digital Media Systems                                     //
//              Silicon Graphics, Inc.                                    //
// Date:        July 28, 1995                                             //
// Description: The code implementing the MFfileEvent class               //
//                                                                        //
// Copyright (c) July 1995, Silicon Graphics, Inc. All Rights Reserved    //
////////////////////////////////////////////////////////////////////////////

// Standard Include Files

#include <stdlib.h>
// Digital Media Include Files
#include <dmedia/midi.h>

// Class definition files
#include "midifile.h"

void deleteEvent(MDevent *e)
{
    if (e) {
	if (e->sysexmsg)
	    free(e->sysexmsg);
	delete e;
    }
}

void deleteMeta(MFmeta *m)
{
    if (m) {
	if (m->msg)
	    free(m->msg);
	delete m;
    }
}


/////////////////////////////////////////////////////////////////////////////
// Constructor: MFfileEvent(MDevent *ev)                                   //
//    The constructor is overloaded for both MDevent and MFmeta. It sets   //
//    the _type variable correspondingly. This constructor is for MIDI     //
//    events.                                                              //
/////////////////////////////////////////////////////////////////////////////
MFfileEvent::MFfileEvent(MDevent *ev)
{
  _type   = MF_MIDI_EVENT;
  _midiev = ev;
  _metaev = (MFmeta *)NULL;
}


/////////////////////////////////////////////////////////////////////////////
// Constructor: MFfileEvent(MFmeta *ev)                                    //
//    The constructor is overloaded for both MDevent and MFmeta. It sets   //
//    the _type variable correspondingly. This constructor is for META     //
//    events.                                                              //
/////////////////////////////////////////////////////////////////////////////
MFfileEvent::MFfileEvent(MFmeta *ev)
{
  _type   = MF_META_EVENT;
  _metaev = ev;
  _midiev = (MDevent *)NULL;
}

/////////////////////////////////////////////////////////////////////////////
// Destructor: ~MFfileEvent(void)                                          //
/////////////////////////////////////////////////////////////////////////////
MFfileEvent::~MFfileEvent() {
    deleteMeta(_metaev);
    deleteEvent(_midiev);
}

/////////////////////////////////////////////////////////////////////////////
// Public Method: unsigned long long MFfileEvent::eventStamp()             //
//    This routine returns the time stamp associated with the MFfileEvent. //
//    It is meant as a convenience for the developer who can obtain the    //
//    time associated with the event without first having to get the       //
//    particular event.                                                    //
/////////////////////////////////////////////////////////////////////////////
unsigned long long MFfileEvent::eventStamp()
{
  if (_type == MF_MIDI_EVENT) {
    return(_midiev->stamp);
  }
  else if (_type == MF_META_EVENT) {
    return(_metaev->stamp);
  }
  return(0);
}


/////////////////////////////////////////////////////////////////////////////
// Public Method: int MFfileEvent::eventType()                             //
//    Returns either MF_MIDI_EVENT or MF_META_EVENT corresponding to the   //
//    type of event it is.                                                 //
/////////////////////////////////////////////////////////////////////////////
int MFfileEvent::eventType()
{
  return(_type);
}


/////////////////////////////////////////////////////////////////////////////
// Public Method: MDevent *midiEvent()                                     //
//    If the event is of type MF_MIDI_EVENT, it returns a pointer to that  //
//    MIDI event. If it is not a MIDI event, this routine returns NULL.    //
/////////////////////////////////////////////////////////////////////////////
MDevent *MFfileEvent::midiEvent()
{
  if (_type == MF_MIDI_EVENT) {
    return(_midiev);
  }
  return((MDevent *)NULL);
}


/////////////////////////////////////////////////////////////////////////////
// Public Method: MFmeta *metaEvent()                                      //
//    If the event is of type MF_META_EVENT, it returns a pointer to that  //
//    Meta event. If it is not a Meta event, this routine returns NULL.    //
/////////////////////////////////////////////////////////////////////////////
MFmeta *MFfileEvent::metaEvent()
{
  if (_type == MF_META_EVENT) {
    return(_metaev);
  }
  return((MFmeta *)NULL);
}







