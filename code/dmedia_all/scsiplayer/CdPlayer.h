///////////////////////////////////////////////////////////////////////////////
//
// CdPlayer.h
//
//   This is the client-side player that know about the SiliconSonix stuff.
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



#ifndef _CdPlayer_h
#define _CdPlayer_h

#include "Player.h"
#include "CdAudio.h"
// #include <pthread.h>

class CdPlayer : public Player
{
public:
  CdPlayer(ScsiPlayer* transport);
  ~CdPlayer();
	
  void play();
  void stop();
  DMstatus seekTrack(int trackNum, DMboolean waitForCompletion = DM_FALSE );
  void seekFrame(unsigned long fr, DMboolean waitForCompletion = DM_FALSE );
  void stateChange();
  void saveTrackAs(const char*);
  void saveSelectionAs(const char* filename, char* targetString = NULL);
  DMstatus updateTransport(DMboolean needRead = NULL);
  void scanDat(int startTrack);		// what a crock
  DMstatus setKey();
  void eject();

  void killThread();
  void spawnedPlay();
  void handleThreadDeath();
  DMstatus closeDevice();
  DMstatus reopenDevice();
  void framesToTc(unsigned long frames, DMtimecode* tc);
  void readInfoFile();
  void databaseSetup();
  char* exportClipBoard();
  DMboolean validTrackNumber(int trackNum);

private:

  static void staticSpawnedPlay(void *);
  // static void* staticSpawnedPlay(void *);
  void seekTrackAux(int trackNum);
  void seekFrameAux(unsigned long fr);
  int openTrackFile(int trackNum);
  DMstatus closeTrackFile();
  DMboolean checkDiskSpace(const char* filename, long bytesNeeded);

  volatile pid_t _childThread;
  // pthread_t _childThread;
  Semaphore* _mutex;

  CDFRAME* _cdFrameBuffer;
  int _trackFd;
  long _byteOffset;
  int _seekTrackRequest;
  int _seekFrameRequest;
  short* _preBuf;		// send some zeros when you start to play.
  char _strBuf[512];

};

#endif	// _CdPlayer_h
