///////////////////////////////////////////////////////////////////////////////
//
// Player.h
//
//   This is the base class for the device dependent players.
//
// Copyright 1995, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
///////////////////////////////////////////////////////////////////////////////

#ifndef _Player_h
#define _Player_h

#include <dmedia/dm_timecode.h>
#include <AvTimeline.h>
#include "AudioBuffer.h"
#include "PlayerState.h"

class Semaphore;

enum PlayState {
    Playing,
    Rewinding,
    FastFwding,
    Seeking, 
    Paused,
    Stopped,
    NoMedia,
    DataMedia,
    Error
};

enum ScsiDevice {CdDev, DatDev};

#define NUM_SCSI_AUDIO_DEVICES 2

class Player  {

public:

  virtual void play() = 0;
  virtual void stop() = 0;
  virtual DMstatus seekTrack(int trackNum, DMboolean waitForCompletion = DM_TRUE) = 0;
  virtual void seekFrame(unsigned long fr, DMboolean waitForCompletion = DM_TRUE) = 0;
  virtual void stateChange() = 0;
  virtual void saveTrackAs(const char*) = 0;
  virtual void saveSelectionAs(const char*, char* targetString = NULL) = 0;
  virtual DMstatus updateTransport(DMboolean needRead = NULL) = 0;
  virtual void scanDat(int startTrack) = 0;
  virtual DMstatus setKey() = 0;
  virtual void eject() = 0;

  // these get called the theScsiPlayerApp->player() from static callbacks.
  virtual void killThread() = 0;
  virtual void spawnedPlay() = 0;
  virtual DMstatus closeDevice() = 0;
  virtual DMstatus reopenDevice() = 0;
  virtual void framesToTc(unsigned long frames, DMtimecode* tc) = 0;
  virtual void readInfoFile() = 0;
  virtual void databaseSetup() = 0;
  virtual char* exportClipBoard() = 0;
  virtual DMboolean validTrackNumber(int trackNum) = 0;

  ScsiPlayer* _transport;	
  AudioBuffer* _audioBuffer; 
  char* _errorString;
  volatile PlayState _playState;
  Semaphore *_mutex; 
  
  // These get passed to/from the UI.
  
  AvTimeline* _playerTimeline;

  // These are in "absolute" time rather than track-relative time.

  long long _selectionStart;	
  long long _selectionEnd;

};

#endif	// _Player_h

