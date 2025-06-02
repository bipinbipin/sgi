///////////////////////////////////////////////////////////////////////////////
//
// DatPlayer.h
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
#ifndef _DatPlayer_h
#define _DatPlayer_h

//#include <pthread.h>
#include <dmedia/dataudio2.h>
#include <dmedia/dataudio2_private.h>
#include "Player.h"

#define DAT_READ_CHUNK_SIZE 4
#define DAT_READ_CHUNK_SIZE_IN_BYTES sizeof(DATFRAME)*DAT_READ_CHUNK_SIZE
// Macros and data to be used in generation of a unique key by setKey().

#define KEY_SIZE	50
#define	TABLE_SIZE	66
#define DB_ID_NTRACKS	5
static char table[TABLE_SIZE] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
  'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
  'U', 'V', 'W', 'X', 'Y', 'Z', '@', '_', '=', '+',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 
  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 
  'u', 'v', 'w', 'x', 'y', 'z',
};


class DatPlayer : public Player
{
public:
  DatPlayer(ScsiPlayer* transport);
  ~DatPlayer();
	
  // virtual functions derived from base Player class
  void play();
  void stop();
  DMstatus seekTrack(int trackNum, DMboolean waitForCompletion = DM_FALSE);
  void seekFrame(unsigned long fr, DMboolean waitForCompletion = DM_FALSE);
  void stateChange();
  void saveTrackAs(const char*);
  void saveSelectionAs(const char* filename, char* targetString = NULL);
  DMstatus updateTransport(DMboolean forceUpdate = DM_FALSE);
  void scanDat(int startTrack);
  DMstatus setKey();
  DMboolean tryKey();

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
  DMboolean validTrackNumber(int trackNum); // for the purposes of display

  DATTRANSPORT* _datTransport;
  DATMEDIATYPE _mediaType;

private:

  void reportStatus();
  void reportSeekInfo(char* msg);

  DMstatus updateTransportFromRead(); 
  DMstatus spinDisplays();
  DMboolean areWeSeeking();
  DMboolean checkDiskSpace(const char* filename, long bytesNeeded);
  

  DATSEEKINFO* _datSeekInfo;
  DATSEEKINFO* _datQueryInfo; 

  DATFRAME _datFrame[DAT_READ_CHUNK_SIZE];

  // static void* staticSpawnedPlay(void *);
  static void staticSpawnedPlay(void *);
  DMstatus seekTrackAux(int trackNum, DMboolean waitForCompletion = DM_TRUE);
  DMstatus seekFrameAux(unsigned long fr, DMboolean waitForCompletion = DM_TRUE);
  void computeTrackInfo(int trackNum);
  void readTrackForLength(int trackNum);
  int setAudioBufferRate(int sampFreq);

  // pthread_t _childThread;
  pid_t _childThread;
  long _byteOffset;
  int _seekTrackRequest;
  int _seekFrameRequest;
  char _keyString[KEY_SIZE];
  int _keyFrame;
  short* _preBuf;
  char _strBuf[1024];		// This needs to accomodate oserror messages plus
				// long resource strings.

};



/*
	map produces side effects on b and len.
	val is an int whose information contain must be rolled into b.
	b is a char*, and map appends to b information contained in val.
*/

#define	map(b, len, val)\
	if (val >= TABLE_SIZE) {\
		if ((len + 2) < KEY_SIZE) {\
			sprintf(b, "%02d", val);\
			b += 2;\
			len += 2;\
		}\
	}\
	else {\
		if (len + 1 < KEY_SIZE) {\
			*b++ = table[val];\
			len++;\
		}\
	}\


#endif	// _DatPlayer_h
