/////////////////////////////////////////////////////////////////////////////
// File:               libmidifileutils.c++                                //
// Author:             Jordan M. Slott                                     //
//                     hordack@esd.sgi.com or hordack@mit.edu              //
//                     Digital Media Systems                               //
//                     Silicon Graphics, Inc.                              //
// Date:               August 4, 1995                                      //
// Description:        Miscellaneous functions defined for MIDI File       //
//                     events.                                             //
//                                                                         //
// Copyright (c) August 1995, Silicon Graphics, Inc. All Rights Reserved.  //
/////////////////////////////////////////////////////////////////////////////

// Standard Include Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Digital Media Include Files
#include <dmedia/midi.h>

// libmidifile Include Files
#include "midifile.h"

// Macro Definitions
static char *notes[128] = {
  "C-1","C#-1","D-1","D#-1","E-1","F-1","F#-1","G-1","G#-1","A-1","A#-1","B-1",
  "C0","C#0","D0","D#0","E0","F0","F#0","G0","G#0","A0","A#0","B0",
  "C1","C#1","D1","D#1","E1","F1","F#1","G1","G#1","A1","A#1","B1",
  "C2","C#2","D2","D#2","E2","F2","F#2","G2","G#2","A2","A#2","B2",
  "C3","C#3","D3","D#3","E3","F3","F#3","G3","G#3","A3","A#3","B3",
  "C4","C#4","D4","D#4","E4","F4","F#4","G4","G#4","A4","A#4","B4",
  "C5","C#5","D5","D#5","E5","F5","F#5","G5","G#5","A5","A#5","B5",
  "C6","C#6","D6","D#6","E6","F6","F#6","G6","G#6","A6","A#6","B6",
  "C7","C#7","D7","D#7","E7","F7","F#7","G7","G#7","A7","A#7","B7",
  "C8","C#8","D8","D#8","E8","F8","F#8","G8","G#8","A8","A#8","B8",
  "C9","C#9","D9","D#9","E9","F9","F#9","G9"
};

// Function Predeclarations
static void _mfAddField(char *buf, char *field, int length);
static char *_mfEncodeMessage(int type);
static char *_mfEncodeType(MFfileEvent *ev);
static char *_mfEncodeChannelVoiceType(MFfileEvent *ev);
static char *_mfEncodeChannelModeType(MFfileEvent *ev);
static char *_mfEncodeSystemCommonType(MFfileEvent *ev);
static char *_mfEncodeSystemRealTimeType(MFfileEvent *ev);
static char *_mfEncodeMetaType(MFfileEvent *ev);
static char *_mfEncodeMetaInfo(char *buf, MFfileEvent *ev);

static char *_mfEncodeChannelVoiceInfo(char *buf, MFfileEvent *ev);
static char *_mfEncodeChannelModeInfo(MFfileEvent *ev);
static char *_mfEncodeSystemCommonInfo(MFfileEvent *ev);
static char *_mfEncodeSystemRealTimeInfo(MFfileEvent *ev);
static char *_mfEncodeSystemExclusiveInfo(char *buf, MFfileEvent *ev);

// NOTE:  should this be using strncat(buf, field, totallength) ??

static void _mfAddField(char *buf, char *field, int totallength)
{
  strcat(buf, field);
  for (int len = strlen(field); len <= totallength; len++) {
    strcat(buf, " ");
  }
}

/* mfIsSystemCommon: System Common Messages
     The status is binary "1111", and the channel is 0sss, where sss is 
     between 1 and 7
 */
int mfIsSystemCommon(MFfileEvent *fev)
{
  if (fev->eventType() == MF_MIDI_EVENT) {
    MDevent *ev = fev->midiEvent();

    int status = ev->msg[0] & MD_STATUSMASK;
    int channel = ev->msg[0] & MD_CHANNELMASK;
    
    if (status == 0xF0 && channel >= 1 && channel <= 7) {
      return(True);
    }
  }
  return(False);
}

/* mfIsSystemRealTime: System Realtime Messages
     The status is binary "1111", and the channel is 1ttt, where ttt is 
     between 0 and 7
 */
int mfIsSystemRealTime(MFfileEvent *fev)
{
  if (fev->eventType() == MF_MIDI_EVENT) {
    MDevent *ev = fev->midiEvent();

    int status = ev->msg[0] & MD_STATUSMASK;
    int channel = ev->msg[0] & MD_CHANNELMASK;
    
    if (status == 0xF0 && channel >= 8 && channel <= 15) {
      return(True);
    }
  }
  return(False);
}

/* mfIsSystemExclusive: System Exclusive Messages
     The status is binary "1111", and the channel is 0000.
 */
int mfIsSystemExclusive(MFfileEvent *fev)
{
  if (fev->eventType() == MF_MIDI_EVENT) {
    MDevent *ev = fev->midiEvent();

    int status = ev->msg[0] & MD_STATUSMASK;
    int channel = ev->msg[0] & MD_CHANNELMASK;
    
    if (status == 0xF0 && channel == 0x00) {
      return(True);
    }
  }
  return(False);
}

/* mfIsChannelVoice: Channel Voice Messages
     The status is between binary "1000" (0x80) and "1110" (0xE0). An 
     exception is "1011" which
     is a Channel Mode message whose first data byte is between 121 and 127
 */
int mfIsChannelVoice(MFfileEvent *fev)
{
  if (fev->eventType() == MF_MIDI_EVENT) {
    MDevent *ev = fev->midiEvent();

    int status = ev->msg[0] & MD_STATUSMASK;
    int dbyte = ev->msg[1];
    
    if (status == 0x80 || status == 0x90 || status == 0xA0 ||
	status == 0xC0 || status == 0xD0 || status == 0xE0) {
      return(True);
    }
    else if (status == 0xB0 && (dbyte < 121 || dbyte > 127)) {
      return(True);
    }
  }
  return(False);
}

/* mfIsChannelMode: Channel Mode Messages
     The status is binary "1011" (0xB0) and the first data byte is between 
     121 and 127
 */
int mfIsChannelMode(MFfileEvent *fev)
{
  if (fev->eventType() == MF_MIDI_EVENT) {
    MDevent *ev = fev->midiEvent();

    int status = ev->msg[0] & MD_STATUSMASK;
    int dbyte = ev->msg[1];
    
    if (status == 0xB0 && dbyte >= 121 && dbyte <= 127) {
      return(True);
    }
  }
  return(False);
}

/* mfIsMeta: Meta File Messages
 */
int mfIsMeta(MFfileEvent *fev)
{
  if (fev->eventType() == MF_META_EVENT) {
    return(True);
  }
  return(False);
}


/* mfMessageType: Returns the type of the Message
     Can either be: MD_SYSTEMCOMMON, MD_SYSTEMREALTIME, MD_SYSEX, 
                    MD_CHANNELVOICE, or
                    MD_CHANNELMODESELECT.
     Return -1 if the event is ill-formed.
 */
int mfMessageType(MFfileEvent *ev)
{
  if (mfIsSystemCommon(ev)) {
    return(MD_SYSTEMCOMMON);
  }
  else if (mfIsSystemRealTime(ev)) {
    return(MD_SYSTEMREALTIME);
  }
  else if (mfIsSystemExclusive(ev)) {
    return(MD_SYSEX);
  }
  else if (mfIsChannelVoice(ev)) {
    return(MD_CHANNELVOICE);
  }
  else if (mfIsChannelMode(ev)) {
    return(MD_CHANNELMODESELECT);
  }
  else if (mfIsMeta(ev)) {
    return(MD_META);
  }
  return(-1);
}

int mfChannelVoiceType(MFfileEvent *fev)
{
  if (!mfIsChannelVoice(fev)) {
    return(-1);
  }
  MDevent *ev = fev->midiEvent();

  int status = ev->msg[0] & MD_STATUSMASK;

  /* All we have to do is return the status */
  return(status);
}

int mfChannelModeType(MFfileEvent *fev)
{
  if (!mfIsChannelMode(fev)) {
    return (-1);
  }
  MDevent *ev = fev->midiEvent();
  
  int dbyte1 = ev->msg[1];

  switch(dbyte1) {
  case 121:        /* Reset All Controllers */
    return (MD_RESETALLCONTROLLERS);

  case 122:        /* Local Control         */
    return (MD_LOCALCONTROL);

  case 123:        /* All Notes Off         */
    return (MD_ALLNOTESOFF);

  case 124:        /* Omni Mode Off         */
    return (MD_OMNIMODEOFF);

  case 125:        /* Omni Mode On          */
    return (MD_OMNIMODEON);

  case 126:       /* Mono Mode On           */
    return (MD_MONOMODEON);

  case 127:       /* Poly Mode On           */
    return (MD_POLYMODEON);

  default:
    return (-1);
  }
}

int mfSystemCommonType(MFfileEvent *fev)
{
  if (mfIsSystemCommon(fev) == False) {
    return(-1);
  }

  MDevent *ev = fev->midiEvent();
  return(ev->msg[0]);
}

int mfSystemRealTimeType(MFfileEvent *fev)
{
  if (mfIsSystemRealTime(fev) == False) {
    return(-1);
  }

  MDevent *ev = fev->midiEvent();
  return(ev->msg[0]);
}

int mfMetaType(MFfileEvent *fev)
{
  if (!mfIsMeta(fev)) {
    return(-1);
  }

  MFmeta *mev = fev->metaEvent();
  return(mev->type);
}

/* mfEncodeEvent: To replace mdPrintEvent */
char *mfEncodeEvent(MFfileEvent *fev)
{
  char *buf = (char *)malloc(512); // note: this was 256 -- should we limit?
  char stamp[64];
  char tmpbuf[1024];
  strcpy(buf, "");

  int messagetype = mfMessageType(fev);
  sprintf(stamp, "%-12llu", fev->eventStamp());
  _mfAddField(buf, stamp, 10);
  _mfAddField(buf, _mfEncodeMessage(messagetype), 16);
  _mfAddField(buf, _mfEncodeType(fev), 24);

  if (messagetype == MD_META) {
    _mfAddField(buf, _mfEncodeMetaInfo(tmpbuf, fev), 50);
  }
  else if (messagetype == MD_CHANNELVOICE) {
    _mfAddField(buf, _mfEncodeChannelVoiceInfo(tmpbuf, fev), 40);
  }
  else if (messagetype == MD_CHANNELMODESELECT) {
    _mfAddField(buf, _mfEncodeChannelModeInfo(fev), 40);
  }
  else if (messagetype == MD_SYSTEMCOMMON) {
    _mfAddField(buf, _mfEncodeSystemCommonInfo(fev), 40);
  }
  else if (messagetype == MD_SYSTEMREALTIME) {
    _mfAddField(buf, _mfEncodeSystemRealTimeInfo(fev), 40);
  }
  else if (messagetype == MD_SYSEX) {
    _mfAddField(buf, _mfEncodeSystemExclusiveInfo(tmpbuf, fev), 20);
  }
  return(buf);
}

/* mfEncodeMessage: Turns a value into a static string */
static char *_mfEncodeMessage(int value)
{
  switch(value) {
  case MD_CHANNELVOICE:
    return ("Channel Voice");

  case MD_CHANNELMODESELECT:
    return ("Channel Mode");

  case MD_SYSEX:
    return ("System Exclusive");

  case MD_SYSTEMCOMMON:
    return ("System Common");

  case MD_SYSTEMREALTIME:
    return("System RealTime");

  case MD_META:
    return("Meta Event");

  default:
    fprintf(stderr, "libmidifile: Unrecognized message type, type=%d\n", value);
    return("");
  }
}

/* mfEncodeType: Turns the type of message into a string */
static char *_mfEncodeType(MFfileEvent *fev)
{
  if (mfIsChannelVoice(fev) == True) {
    return(_mfEncodeChannelVoiceType(fev));
  }
  else if (mfIsChannelMode(fev) == True) {
    return(_mfEncodeChannelModeType(fev));
  }
  else if (mfIsSystemCommon(fev) == True) {
    return(_mfEncodeSystemCommonType(fev));
  }
  else if (mfIsSystemRealTime(fev) == True) {
    return(_mfEncodeSystemRealTimeType(fev));
  }
  else if (mfIsMeta(fev) == True) {
    return(_mfEncodeMetaType(fev));
  }
  else {
    return("");
  }
}


static char *_mfEncodeChannelVoiceType(MFfileEvent *fev)
{  
  switch(mfChannelVoiceType(fev)) {
  /* Channel Voice Messages */
  case MD_NOTEOFF:
    return ("Note Off");

  case MD_NOTEON:
    return ("Note On");

  case MD_POLYKEYPRESSURE:
    return ("After Touch");

  case MD_CONTROLCHANGE:
    return ("Control Change");

  case MD_PROGRAMCHANGE:
    return ("Program Change");

  case MD_CHANNELPRESSURE:
    return ("Channel Pressure");

  case MD_PITCHBENDCHANGE:
    return ("Pitch Bend Change");

  default:
    return ("");
  }
}


static char *_mfEncodeChannelModeType(MFfileEvent *fev)
{
  switch(mfChannelModeType(fev)) {

  /* Channel Mode Messages */
  case MD_RESETALLCONTROLLERS:
    return ("Reset All Controllers");

  case MD_LOCALCONTROL:
    return ("Local Control");

  case MD_ALLNOTESOFF:
    return ("All Notes Off");

  case MD_OMNIMODEOFF:
    return ("Omni Mode Off");

  case MD_OMNIMODEON:
    return ("Omni Mode On");

  case MD_MONOMODEON:
    return ("Mono Mode On");

  case MD_POLYMODEON:
    return ("Poly Mode On");

  default:
    return ("");
  }
}


static char *_mfEncodeSystemCommonType(MFfileEvent *fev)
{
  switch(mfSystemCommonType(fev)) {

  /* System Common Messages */
  case MD_TIMECODEQUARTERFRAME:
    return ("Time Code 1/4 Frame");

  case MD_SONGPOSITIONPOINTER:
    return ("Song Position Pointer");

  case MD_SONGSELECT:
    return ("Song Select");

  case MD_UNDEFINED1:
    return ("Undefined");

  case MD_TUNEREQUEST:
    return ("Tune Request");

  case MD_EOX:
    return ("EOX");

  default:
    return ("");
  }
}


static char *_mfEncodeSystemRealTimeType(MFfileEvent *fev)
{
  switch(mfSystemRealTimeType(fev)) {

  /* System Realtime Messages */
  case MD_TIMINGCLOCK:
    return ("Timing Clock");

  case MD_UNDEFINED3:
  case MD_UNDEFINED4:
    return ("Undefined");

  case MD_START:
    return ("Start");

  case MD_CONTINUE:
    return ("Continue");

  case MD_STOP:
    return ("Stop");

  case MD_ACTIVESENSING:
    return ("Active Sensing");

  case MD_SYSTEMRESET:
    return ("System Reset");

  default:
    return ("");
  }
}


static char *_mfEncodeMetaType(MFfileEvent *fev)
{
  switch(mfMetaType(fev)) {

  /* Meta Messages */
  case MIDImeta_SeqNumber:
    return ("Sequence Number");

  case MIDImeta_Text:
    return ("Text Event");

  case MIDImeta_Copyright:
    return ("Copyright Notice");

  case MIDImeta_Name:
    return ("Sequence/Track Name");

  case MIDImeta_Instrument:
    return ("Instrument Name");

  case MIDImeta_Lyric:
    return ("Lyric");

  case MIDImeta_Marker:
    return ("Marker");

  case MIDImeta_CuePoint:
    return ("Cue Pointer");

  case MIDImeta_ChannelPrefix:
    return ("Channel Prefix");

  case MIDImeta_EOT:
    return ("End of Track");

  case MIDImeta_SetTempo:
    return ("Set Tempo");

  case MIDImeta_SMPTEoffset:
    return ("SMPTE Offset");

  case MIDImeta_TimeSignature:
    return ("Time Signature");

  case MIDImeta_KeySignature:
    return ("Key Signature");

  case MIDImeta_SeqSpecific:
    return ("Sequencer-Specific Event");

  default:
    return ("");
  }
}

static char *_mfEncodeChannelVoiceInfo(char *buf, MFfileEvent *fev)
{
  if (mfIsChannelVoice(fev) == False) {
    return("");
  }

  int type = mfChannelVoiceType(fev);
  MDevent *ev = fev->midiEvent();

  switch(type) {
  case MD_NOTEOFF:
  case MD_NOTEON:
    sprintf(buf, "Channel %d  Note %d %4s  Velocity %d", 
	    (ev->msg[0] & 0x0F) + 1, ev->msg[1], notes[ev->msg[1]],
	    ev->msg[2]);
    break;

  case MD_POLYKEYPRESSURE:
    sprintf(buf, "Channel %d  Note %d %4s  Pressure %d", 
	    (ev->msg[0] & 0x0F) + 1, ev->msg[1], notes[ev->msg[1]],
	    ev->msg[2]);
    break;

  case MD_CONTROLCHANGE:
    sprintf(buf, "Channel %d  Controller %d  Value %d", 
	    (ev->msg[0] & 0x0F) + 1, ev->msg[1], ev->msg[2]);
    break;

  case MD_PROGRAMCHANGE:
    sprintf(buf, "Channel %d  Program %d", 
	    (ev->msg[0] & 0x0F) + 1, ev->msg[1] + 1);
    break;

  case MD_CHANNELPRESSURE:
    sprintf(buf, "Channel %d  Pressure %d", 
	    (ev->msg[0] & 0x0F) + 1, ev->msg[1]);
    break;

  case MD_PITCHBENDCHANGE:
    sprintf(buf, "Channel %d  Value %d", 
	    (ev->msg[0] & 0x0F) + 1, ((int)ev->msg[2] << 8) | ev->msg[1]);
    break;

  default:
    sprintf(buf, "");
    break;
  }
  return(buf);
}

static char *_mfEncodeChannelModeInfo(MFfileEvent *)
{
  return("");
}

static char *_mfEncodeSystemCommonInfo(MFfileEvent *)
{
  return("");
}

static char *_mfEncodeSystemRealTimeInfo(MFfileEvent *)
{
  return("");
}

static char *_mfEncodeSystemExclusiveInfo(char *buf, MFfileEvent *fev)
{
  if (mfIsSystemExclusive(fev) == False) {
    return ("");
  }

  MDevent *ev = fev->midiEvent();
  sprintf(buf, "Length %d", ev->msglen);

  return(buf);
}

static char *_mfEncodeMetaInfo(char *buf, MFfileEvent *fev)
{
  if (mfIsMeta(fev) == False) {
    return("");
  }

  MFmeta *mev = fev->metaEvent();
  switch(mev->type) {

  /* Meta Messages */
  case MIDImeta_SeqNumber:
    unsigned long no = (int)((mev->msg[0] << 8) & 0xF0) + (mev->msg[1] & 0x0F);
    sprintf(buf, "%lu", no);
    return(buf);

  case MIDImeta_Text:
  case MIDImeta_Copyright:
  case MIDImeta_Name:
  case MIDImeta_Instrument:
  case MIDImeta_Lyric:
  case MIDImeta_Marker:
  case MIDImeta_CuePoint:
    for (int i = 0; i < mev->msglen; i++) {
      buf[i] = mev->msg[i];
    }
    buf[i] = '\0';
    return(buf);

  case MIDImeta_SetTempo:
    no = (unsigned long)((unsigned long)mev->msg[0] << 16);
    no += (unsigned long)((unsigned long)mev->msg[1] << 8);
    no += (unsigned long)mev->msg[2];
    sprintf(buf, "%lu microsecs", no);
    return (buf);

  case MIDImeta_SMPTEoffset:
    sprintf(buf, "%d: %d: %d: %d: %d", mev->msg[0], mev->msg[1],
	    mev->msg[2], mev->msg[3], mev->msg[4]);
    return (buf);

  case MIDImeta_TimeSignature:
    sprintf(buf, "%d %d %d %d", mev->msg[0], mev->msg[1],
	    mev->msg[2], mev->msg[3]);
    return (buf);

  case MIDImeta_KeySignature:
    switch (mev->msg[0]) {
    case (-7):
      strcpy(buf, "7 flats  ");
      break;
    case (-1):
      strcpy(buf, "1 flat   ");
      break;
    case (0):
      strcpy(buf, "Key of C ");
      break;
    case (1):
      strcpy(buf, "1 sharp  ");
      break;
    case (7):
      strcpy(buf, "7 sharps ");
      break;

    default:
      strcpy(buf, "         ");
      break;
    }

    if (mev->msg[1] == 0) {
      strcat(buf, "Major Key");
    }
    else if (mev->msg[1] == 1) {
      strcat(buf, "Minor Key");
    }
    return (buf);

  case MIDImeta_SeqSpecific:
  case MIDImeta_ChannelPrefix:
  case MIDImeta_EOT:
  default:
    strcpy(buf, "");
    return(buf);
  }
}
