/////////////////////////////////////////////////////////////////////////////
// File:                       MFfile.c++                                  //
// Author:                     Archer Sully and Jordan M. Slott            //
//                             archer@esd.sgi.com and hordack@esd.sgi.com  //
//                             Digital Media Systems                       //
//                             Silicon Graphics, Inc.                      //
// Date:                       August 4, 1995                              //
// Descrpition:                Implments the MFfile class, to read and     //
//                             write on disk MIDI files.                   //
//                                                                         //
// Copyright (c) August 1995, Silicon Graphics, Inc. All Rights Reserved.  //
/////////////////////////////////////////////////////////////////////////////

// Standard Include Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <values.h>

// Digital Media Include Files
#include <dmedia/midi.h>

#include "midifile.h"

// These are to find the message lengths of Channel Voice and Mode Messages
int __msglengths[] = { 3, 3, 3, 3, 2, 2, 3 };
#define getMsgLen(x) ((__msglengths[(x & 0x7f) >> 4]))

inline MFfile::readchar() 
{
  int c = getc(_fp);
  if (_toread)
	  _toread--;
  return c;
}

MFfile::MFfile()
{
  _division = 120;
  _format   = 0;
  _ntracks  = 0;
  _toread   = 0;
  _tracks   = (MFtrack **)NULL;
}

MFfile::~MFfile()
{
  // Clean out the tracks
  while (getTrack(0) != NULL) {
    deleteTrack(0);
  }
  if (_tracks != NULL) {
    free(_tracks);
  }
}

/////////////////////////////////////////////////////////////////////////////
//                                                                         //
// MIDI File Header Information Access Methods                             //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////
int MFfile::format()
{
  return(_format);
}

int MFfile::numberTracks()
{
  return(_ntracks);
}

int MFfile::division()
{
  return(_division);
}

void MFfile::setFormat(int i)
{
  _format = i;
}

void MFfile::setDivision(int i)
{
  _division = i;
}

/////////////////////////////////////////////////////////////////////////////
//                                                                         //
// Track Management Methods                                                //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////

MFtrack *MFfile::getTrack(int n)
{
  if (n >= _ntracks) {
    return((MFtrack *)NULL);
  }
  return((MFtrack *)_tracks[n]);
}

void MFfile::addTrack(MFtrack *track)
{
  if (track != NULL) {
    _tracks = (MFtrack **)realloc(_tracks, (_ntracks + 1) * sizeof(MFtrack *));
    if (_tracks == NULL) {
      _ntracks = 0;
      return;
    }
    _tracks[_ntracks] = track;
    _ntracks++;
  }
}

void MFfile::deleteTrack(int n)
{
  if (n < _ntracks) {
    MFtrack **newtracks = (MFtrack **)malloc((_ntracks-1) * sizeof(MFtrack *));
    for (int i = 0, j = 0; i < _ntracks; i++) {
      if (i == n) {
	  delete _tracks[i];
      }
      else {
	newtracks[j] = _tracks[i];
	j++;
      }
    }
    if (_tracks != NULL) {
      free(_tracks);
    }
    _tracks = newtracks;
    _ntracks--;
  }
}


/////////////////////////////////////////////////////////////////////////////
//                                                                         //
// File I/O Methods                                                        //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////

int MFfile::load(char *name) 
{
  if ((_fp = fopen(name, "r")) == NULL) {
    return -1;
  }

  int tot = 0;
  int tracksread = readfilehdr();
  MFtrack *track;

  // Clean out the tracks
  while (getTrack(0) != NULL) {
    deleteTrack(0);
  }

  for (int i = 0; i < tracksread; i++) {
    track = new MFtrack();
    tot += readtrack(track);
    this->addTrack(track);
  }

  fclose(_fp);
  this->rewind();
  return(0);
}

int MFfile::load(int fd)
{
  if ((_fp = fdopen(fd, "r")) == NULL) {
    return -1;
  }
  fseek(_fp, 0, SEEK_SET);

  int tot = 0;
  int tracksread = readfilehdr();
  MFtrack *track;

  // Clean out the tracks
  while (getTrack(0) != NULL) {
    deleteTrack(0);
  }

  for (int i = 0; i < tracksread; i++) {
    track = new MFtrack();
    tot += readtrack(track);
    this->addTrack(track);
  }

  fclose(_fp);
  this->rewind();
  return(0);
}

int MFfile::save(char *name)
{
  if ((_fp = fopen(name, "w")) == NULL) {
    return(-1);
  }

  writefilehdr();

  for (int i = 0; i < _ntracks; i++) {
    writetrack(i);
  }
  fclose(_fp);
  return(0);
}

void MFfile::rewind()
{
  for (int i = 0; i < _ntracks; i++) {
    _tracks[i]->seekBeginning();
  }
}

unsigned long long MFfile::tell()
{
  int howmany;
  unsigned long long tmp  = 0xffffffffffffffff;
  if (_format == 0) {
    return(_tracks[0]->currentTimeStamp(&howmany));
  }
  else {
    for (int i = 0; i < _ntracks; i++) { 
      if (_tracks[i]->currentTimeStamp(&howmany) < tmp) {
        tmp = _tracks[i]->currentTimeStamp(&howmany);
      }
    }
  }
  return(tmp);
}

void MFfile::seek(unsigned long long time) 
{
  for (int i = 0; i < _ntracks; i++) {
    _tracks[i]->seekTime(time);
  }
}

/////////////////////////////////////////////////////////////////////////////
// int MFfile::nextevent(MFfileEvent *);                                   //
//   Return all tracks which have an event on the next active tick.        //
/////////////////////////////////////////////////////////////////////////////

MFfileEvent **MFfile::nextEvent(int *nevent)
{
  MFfileEvent **evlist;
  unsigned long long low = 0xffffffffffffffff;
  int howmany;
  int total = 0;

  // find the lowest numbered tick on all the tracks
  for (int i = 0; i < _ntracks; i++) {
    unsigned long long tmptime = _tracks[i]->currentTimeStamp(&howmany);

    if (howmany != 0) {
      if (tmptime < low) {
        low = tmptime;
        total = howmany;
      }
      else if (tmptime == low) {
        total += howmany;
      }
    }
  }


  if (total == 0) {
    evlist = NULL;
    *nevent = 0;
    return(0);
  }

  evlist = (MFfileEvent **)malloc(sizeof(MFfileEvent *) * total);
  total = 0;

  for (i = 0; i < _ntracks; i++) {
    unsigned long long tmptime = _tracks[i]->currentTimeStamp(&howmany);
    if (tmptime == low) {
      for (int j = 0; j < howmany; j++) {
        evlist[total] = _tracks[i]->currentEvent();
        _tracks[i]->nextEvent();
        total++;
      }
    }
  }

  *nevent = total;
  return(evlist);
}

MFfileEvent *MFfile::nextEvent()
{
  MFfileEvent *ev;
  unsigned long long low = 0xffffffffffffffff;
  int howmany;
  int total = 0;
  int track = -1;

  // find the lowest numbered tick on all the tracks
  for (int i = 0; i < _ntracks; i++) {
    unsigned long long tmptime = _tracks[i]->currentTimeStamp(&howmany);

    if (howmany != 0) {
      if (tmptime < low) {
        low = tmptime;
        total = howmany;
	track = i;
      }
      else if (tmptime == low) {
        total += howmany;
      }
    }
  }


  if (total == 0) {
    ev = NULL;
    return(ev);
  }

  //evlist = (MFfileEvent **)malloc(sizeof(MFfileEvent *) * total);

  unsigned long long tmptime = _tracks[track]->currentTimeStamp(&howmany);
  if (tmptime == low && track != -1) {
      ev = _tracks[track]->currentEvent();
      _tracks[track]->nextEvent();
      return(ev);
  }
  return NULL;

}

///////////////////////////////////////////////////////////////////////////
//                                                                       //
// Internal Writing Utility Methods                                      //
//                                                                       //
///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////
// int MFfile::writetrackhdr():                                          //
//    MTrk <length of track data> information. Returns the offset of the //
//    file pointing to the end of the header                             //
///////////////////////////////////////////////////////////////////////////
unsigned long
MFfile::writetrackhdr()
{
  unsigned long offset;

  writehd("MTrk");
  offset = ftell(_fp);
  write32(0);                        // length of header data -- dummy

  return(offset);
}

///////////////////////////////////////////////////////////////////////////
// void MFfile::writefilehdr():                                          //
//    MThd <length of header data> information                           //
///////////////////////////////////////////////////////////////////////////
void
MFfile::writefilehdr()
{
  writehd("MThd");
  write32(6);                       // length of header data
  write16(_format);                 // type of MIDI file
  write16(_ntracks);                // number of tracks
  write16(_division);               // division
}

////////////////////////////////////////////////////////////////////////////
// int MFfile::writehd(char *):                                           //
//    Writes either "MTrk" or "MThd"                                      //
////////////////////////////////////////////////////////////////////////////
int
MFfile::writehd(char *looking)
{
  return(fprintf(_fp, "%s", looking));
}

////////////////////////////////////////////////////////////////////////////
// int MFfile::writevar(int n)                                            //
////////////////////////////////////////////////////////////////////////////
int
MFfile::writevar(int n)
{
  register long buffer;
  int sum = 0;

  buffer = n & 0x7f;
  while ((n >>= 7) > 0) {
    buffer <<= 8;
    buffer |= 0x80;
    buffer += (n & 0x7f);
  }

  while (1) {
    sum++;
    fprintf(_fp, "%c", buffer & 0xff);
    if (buffer & 0x80) {
      buffer >>= 8;
    }
    else {
      break;
    }
  }
  return(sum);

}

////////////////////////////////////////////////////////////////////////////
// int MFfile::writetime(int n)                                           //
////////////////////////////////////////////////////////////////////////////
int
MFfile::writetime(int t)
{
  return(writevar(t));
}

////////////////////////////////////////////////////////////////////////////
// void MFfile::write32(int n)                                            //
////////////////////////////////////////////////////////////////////////////
void
MFfile::write32(int n)
{
  int mask = 0xff000000;
  fprintf(_fp,"%c",(n & mask) >> 24); mask >>= 8;
  fprintf(_fp,"%c",(n & mask) >> 16); mask >>= 8;
  fprintf(_fp,"%c",(n & mask) >> 8); mask >>= 8;
  fprintf(_fp,"%c",(n & mask));
}

////////////////////////////////////////////////////////////////////////////
// void MFfile::write16(int n)                                            //
////////////////////////////////////////////////////////////////////////////
void
MFfile::write16(int n)
{
  int mask = 0xff00;
  fprintf(_fp,"%c",(n & mask) >> 8); mask >>= 8;
  fprintf(_fp,"%c",(n & mask));
}


////////////////////////////////////////////////////////////////////////////
// int MFfile::writemetaevent(MFmeta *me):                                //
//    Writes the MIDI Meta event to the files. Returns the total number   //
//    of bytes that were written.                                         //
////////////////////////////////////////////////////////////////////////////

int
MFfile::writemetaevent(MFmeta *me, unsigned long long *time)
{
  int sum = writetime(me->stamp - *time) + 2;
  *time = me->stamp;

  fprintf(_fp, "%c%c", 0xFF, me->type);
  sum += writevar(me->msglen);
  for (int i = 0; i < me->msglen; i++) {
    fprintf(_fp, "%c", me->msg[i]);
  }
  return(sum + me->msglen);
}

////////////////////////////////////////////////////////////////////////////
// int MFfile::writetrack(int trackno)                                    //
////////////////////////////////////////////////////////////////////////////
int
MFfile::writetrack(int trackno)
{
  int sum = 0;

  // Get the track
  MFtrack *track = _tracks[trackno];

  // Write the header for the track
  unsigned long offset = writetrackhdr();

  // Goto the beginning of the track
  track->seekTime(0); 

  // Write all of the events
  MFfileEvent *fev;
  unsigned long long time = 0;
  for (fev = track->currentEvent(); fev != NULL; fev = track->nextEvent()) {
    sum += writeevent(fev, &time);
  }

  // Write the length of the track after the header information
  long here = ftell(_fp);
  fseek(_fp, offset, 0);
  write32(sum);
  fseek(_fp, here, 0);

  return(sum);
}

int
MFfile::writeevent(MFfileEvent *fev, unsigned long long *time)
{
 if (fev->eventType() == MF_MIDI_EVENT) {
   return(MFfile::writemidievent(fev->midiEvent(), time));
 }
 else if (fev->eventType() == MF_META_EVENT) {
   return(MFfile::writemetaevent(fev->metaEvent(), time));
 }
 return(0);
}

int
MFfile::writemidievent(MDevent *ev, unsigned long long *time)
{
  char *m;

  int sum = writetime(ev->stamp - *time);

  *time = ev->stamp;
  int length;

  if (ev->msg[0] < MD_SYSEX) {     // Channel Voice/Mode Message
    int status = ev->msg[0] & 0xFF;
    length = getMsgLen(status);
    m = ev->msg;
  }
  else {
    length = ev->msglen;
    m = ev->sysexmsg;
  }
  
  for (int i = 0; i < length; i++) { 
    fprintf(_fp, "%c", m[i]);
  }
  return(sum + length);
}

///////////////////////////////////////////////////////////////////////////
//                                                                       //
// Internal Reading Utility Methods                                      //
//                                                                       //
///////////////////////////////////////////////////////////////////////////

MFfile::readtrack(MFtrack *track)
{
  int status, running = 0;
  unsigned long long time = 0;
  
  if (track == NULL) {
    return(-1);
  }
  
  MFfile::readtrackhdr();
  
  while (_toread > 0) {
    time += readvar();
    int c = readchar();
    if (c == EOF)
	break;
    status = c & 0xff;

    if (status < MD_SYSEX) {
      MDevent *ev = MFfile::readmidievent(status, &running, time);
      MFfileEvent *fev = new MFfileEvent(ev);
      track->insertEvent(fev);
    } 
    else {		// not a channel message
      MFfileEvent *fev;
      switch(status) {
      case MD_SYSEX:	// for now, ignore sysex in files
      case MD_EOX:
	MDevent *ev = MFfile::readsysexevent(time);
        fev = new MFfileEvent(ev);
        track->insertEvent(fev);
	break;

      case MD_META:
	MFmeta *mev = MFfile::readmetaevent(time);
        fev = new MFfileEvent(mev);
        track->insertEvent(fev);
	break;
      }
    }
  }
  return(track->numberEvents());
}


///////////////////////////////////////////////////////////////////////////
// int MFfile::readtrackhdr():                                           //
///////////////////////////////////////////////////////////////////////////

MFfile::readtrackhdr()
{
  if (readhd("MTrk", 1) == NULL) {
    return 0;
  }
  _toread = read32();

  return _toread;
}


MFfile::read32()
{
  char c = readchar();
  int value = c & 0xff;
  c = readchar();
  value = (value << 8) + (c & 0xff);
  c = readchar();
  value = (value << 8) + (c & 0xff);
  c = readchar();
  value = (value << 8) + (c & 0xff);

  return(value);
}
 
MFfile::read16()
{
  char c = readchar();
  int value = c & 0xff;
  c = readchar();
  value = (value << 8) + (c & 0xff);

  return(value);
}

MFfile::readvar()
{
  int value, c;
  
  c = readchar();
  if (c == EOF)
    return 0;

  value = c;
  if (c & 0x80) {
    value &= 0x7f;
    do {
      c = readchar();
      if (c == EOF)
	  break;
      value = (value << 7) + (c & 0x7f);
    } while (c & 0x80);
  }
  return value;
}

MFfile::readhd(char *looking, int skip)
{
  int c;
  
  if (skip) {
    do {
      c = readchar();
    } while (c != EOF && c != looking[0]);
  }
  else { 
    c = readchar();
  }

  if (c != looking[0]) return 0;
  c = readchar();
  if (c != looking[1]) return 0;
  c = readchar();
  if (c != looking[2]) return 0;
  c = readchar();
  if (c != looking[3]) return 0;
  return 1;
}

MFfile::readfilehdr()
{
  if (readhd("MThd",0) == NULL) {
    fseek(_fp, 0, SEEK_SET);
    if (readhd("RIFF",0)) {        // File may be RIFF MIDI
	if (readhd("RMID", 1) == NULL)
	    return 0;
	else if (readhd("data", 1) == NULL)
	    return 0;
	else if (readhd("MThd",1) == NULL)
	    return 0;
    }
  }

  _hdrlength = read32();         // Read the length, 32-bit quantity
  _toread = _hdrlength;
  _format = read16();            // Type 0, 1, or 2
  int notracks = read16();       // Number of tracks
  _division = read16();          // quarter-note divisions

  if (_division < 0) {
    fprintf(stderr, "file in SMPTE format\n");
  }
  if (_toread > 0) {
    fprintf(stderr, "header more than 6 bytes, ignoring %d bytes.\n", _toread);
  }
  while (_toread > 0) {
    readchar();
  }
  return(notracks);
}

MDevent *MFfile::readsysexevent(unsigned long long time)
{
  MDevent *ev = new MDevent;
  ev->msglen = readvar() + 1;

  ev->sysexmsg = (char *)malloc(ev->msglen);
  ev->stamp = time;
  ev->msg[0] = MD_SYSEX;
  ev->msg[1] = 0;
  ev->msg[2] = 0;
  ev->sysexmsg[0] = MD_SYSEX;
  for (int i = 1; i < ev->msglen; i++) {
    ev->sysexmsg[i] = readchar();
  }
  return(ev);
}

MDevent *
MFfile::readmidievent(int status, int *oldstatus, unsigned long long time)
{
  MDevent *ev = new MDevent();
  int running, needed;

  if (status & 0x80) {    // not running status, high bit is a 1
    running = 1;
    *oldstatus = status;
    ev->msg[0] = status;
  }
  else {                 // running status, high bit is a 0
    ev->msg[0] = *oldstatus;
    ev->msg[1] = status;
    status = *oldstatus;
    running = 2;
  }
  ev->msglen = 0;
  needed = getMsgLen(status);
  
  // if running status in effect, read mess[2],
  // otherwise read mess[1] and mess[2]
  while (running < needed) {
    char c = readchar();
    ev->msg[running] = c & 0xFF;
    running++;
  }
  ev->stamp = time;
  return(ev);
}

MFmeta *MFfile::readmetaevent(unsigned long long time)
{
  MFmeta *mev = new MFmeta();

  mev->type = (MIDImetaEvents)readchar();
  mev->msglen = readvar();
  mev->stamp = time;

  mev->msg = (char *)malloc(mev->msglen);
  for (int i = 0; i < mev->msglen; i++) {
    mev->msg[i] = readchar();
  }

  return(mev);
}

void MFfile::print()
{
  fprintf(stderr, "MFfile ->\n");
  fprintf(stderr, "\tno of tracks = %d\n", _ntracks);
  fprintf(stderr, "\tformat       = %d\n", _format);
  fprintf(stderr, "\tdivision     = %d\n", _division); 
  fprintf(stderr, "\t------------------------------------------\n");

  for (int i = 0; i < _ntracks; i++) {
    fprintf(stderr, "\tTrack #%d ->\n", i + 1);
    _tracks[i]->print();
    fprintf(stderr, "\t<- End Track #%d\n", i + 1);
  }
  fprintf(stderr, "<- End MFfile\n");
}
