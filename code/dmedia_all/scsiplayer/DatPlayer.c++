/////////////////////////////////////////////////////////////////////////////////
//
// Datplayer.c++
//
//   This is the DAT-specific part of the player.
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
// in Technical Data and Computerputer Software clause at DFARS 252.227-7013 and/or
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


#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <invent.h>
#include <mntent.h>
#include <signal.h>
#include <strings.h>
#include <errno.h>
#include <mediad.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/statfs.h>

#include <Vk/VkResource.h>
#include <Vk/VkInterruptDialog.h>
#include <Vk/VkProgressDialog.h>
#include <Vk/VkErrorDialog.h>
#include <VkWarningDialog.h>
#include <Vk/VkBusyDialog.h>

#include <dmedia/audiofile.h>

#include "ScsiPlayerApp.h"
#include "MainWindow.h"
#include "DatPlayer.h"

//#define DBG(x) {x;}
#define DBG(x) {}  

#define PREBUFBLOCKSIZE 8000
#define NO_SEEK_REQUESTED -1

DatPlayer::DatPlayer(ScsiPlayer* transport)
{

  _transport = transport;
  _datTransport = NULL;
  _datSeekInfo = (DATSEEKINFO*)malloc(sizeof(DATSEEKINFO));
  _datQueryInfo = (DATSEEKINFO*)malloc(sizeof(DATSEEKINFO));
  _playState = Stopped;
  _childThread = NULL;
  _transport->lastTrack = 0;
  _keyFrame  = 0;
  _mediaType = DATmediaUnknown;

  _seekTrackRequest = NO_SEEK_REQUESTED;
  _seekFrameRequest = NO_SEEK_REQUESTED;

  char tmpNameBuf[MNTMAXSTR];
  if (!theScsiPlayerApp->_scsiDeviceName) {
    // Find the DAT drive using system inventory
    inventory_t* inv;
    setinvent();
    for (inv = getinvent(); inv; inv = getinvent())
      if (inv->inv_class == INV_TAPE
	  && (inv->inv_type == INV_SCSIQIC || inv->inv_type == INV_VSCSITAPE)
	  && inv->inv_state == TPDAT)
	break;
    if (inv) {
      sprintf(tmpNameBuf, "/dev/scsi/sc%dd%dl0",
	      inv->inv_controller, inv->inv_unit);
      endinvent();
      theScsiPlayerApp->_scsiDeviceName = strdup(tmpNameBuf);
    } else {
      endinvent( );
      sprintf(_strBuf, "%s\n%s\n",
	      VkGetResource("DatPlayer*noDatDriveMsg", XmCString),
	      strerror(oserror()));  
      theErrorDialog->postAndWait(_strBuf);
      exit(1);
    }
  }
 
  _audioBuffer = new AudioBuffer("ScsiPlayer", out, 0, 50*DAT_NUMSAMPS44K);
  _audioBuffer->setOutputRate(AL_RATE_44100);

  _errorString = _audioBuffer->getErrorString();
  if (_errorString) {
    fprintf(stderr, "%s\n", _errorString);
    exit(1);
  } 

  _preBuf = new short[PREBUFBLOCKSIZE];
  bzero(_preBuf,sizeof(short)*PREBUFBLOCKSIZE);

  if (_transport->exclId = mediad_get_exclusiveuse(theScsiPlayerApp->_scsiDeviceName,
						    "datplayer") < 0) {
    theErrorDialog->postAndWait(VkGetResource("DatPlayer*noDatDriveFromMediadMsg",
					      XmCString));
    exit(1);
  }


  // setup the actual dat 

  DBG(fprintf(stderr,"DatPlayer opening %s\n", theScsiPlayerApp->_scsiDeviceName));
  setreuid(-1, 0);	// get privileges
  if (!(_datTransport = datOpen(theScsiPlayerApp->_scsiDeviceName, "r"))) {
    char* msgBuf = new char[300];
    sprintf(msgBuf, VkGetResource("DatPlayer*couldntOpenDatDriveMsg", XmCString),
	    tmpNameBuf, strerror(oserror()));
    theErrorDialog->postAndWait(msgBuf);
    exit(1);
  }
  setreuid(-1, getuid());	// drop privileges

  datAbort(_datTransport);
  datSeekBOT(_datTransport,DM_TRUE);
}

DatPlayer::~DatPlayer()
{
  closeDevice();
}


DMstatus DatPlayer::closeDevice()
{
  if (_datTransport) {
    DBG(fprintf(stderr,"closing DAT\n"));
    datClose(_datTransport);
    return DM_SUCCESS;
  }

  return DM_FAILURE;

}

DMstatus DatPlayer::reopenDevice()
{
  return DM_SUCCESS;
}


void DatPlayer::framesToTc(unsigned long frames, DMtimecode* tc)
{
  // DBG(fprintf(stderr,"framesToTc...\n"));
  datFrameToTc(frames, tc);
}


void DatPlayer::reportStatus()
{
  static char* statusStrings[] = {
    /* DAT general status codes */
    "ready to accept next command",
    "device is executing a previous command",
    "not identified error",
    "undefined status, could be a problem",
    "no sense",
    "not ready, no tape or ...",
    "unrecoverable data error encountered",
    "hardware failure",
    "the CDB contains an invalid bit",
    "tape was changed prior to sending command",
    "write protected",
    "an immediate command is in progress",
    "no data condition or wrong data format",
    "write complete with data in the buffer",

    /* Detailed information for DAT_NOSENSE */
    "no additional information available",
    "file mark detected",
    "end of tape",
    "beginning of tape",

    /* Detailed information for DAT_NOTREADY */
    "in process of becoming ready",
    "incompatable media",
    "cleaning cassette installed",
    "casette not present",

    /* Detailed information for DAT_MEDERR */
    "excessive write",
    "unrecovered read error",
    "can't read tape, unknown format",
    "can't read tape, incompatable format",
    "tape format corrupted",
    "sequential positioning error",
    "lost tape position",

    /* Detailed information for DAT_UATTEN */
    "not ready to ready transition",
    "power on or reset",
    "cassette was changed",

    /* Detailed information for DAT_BLNKCHK */
    "end of data detected",
    "can't read tape, unknown format"};


  fprintf(stderr,"state = %d (%s)\ndetail = %d (%s)\n",
	  (int)_datTransport->state, statusStrings[(int)_datTransport->state],
	  (int)_datTransport->detail, statusStrings[(int)_datTransport->detail]);
}

void DatPlayer::reportSeekInfo(char* msg)
{
  DBG(fprintf(stderr,"%s: prog = %d, atime = %d:%d:%d\n",
	      msg, _datSeekInfo->prognum,
	      _datSeekInfo->atime.minute, _datSeekInfo->atime.second,
	      _datSeekInfo->atime.frame));
}

DMboolean DatPlayer::areWeSeeking()
{
  DMstatus status = datTestReady(_datTransport);
  // fprintf(stderr,"areWeSeeking:  status = %d\n", _datStatus->state);
  if (_datTransport->state ==  DAT_BUSYSTATE)
    return DM_TRUE;
  else
    return DM_FALSE;
}

DMstatus DatPlayer::spinDisplays()
{
  DBG(fprintf(stderr,"Entering DatPlayer::spinDisplays\n"));
  theScsiPlayerApp->stopDisplayUpdate(); // to get new redisplay rate
  do {
    updateTransport();
    theScsiPlayerApp->stateChange();
  } while (areWeSeeking());
  updateTransport();
  theScsiPlayerApp->startDisplayUpdate(); // to get new redisplay rate
  return datTestReady(_datTransport);
}


DMboolean DatPlayer::validTrackNumber(int trackNum)
{
  if ((trackNum < 1) || (trackNum > 999))
    return DM_FALSE;
  else
    return DM_TRUE;
}

DMstatus DatPlayer::seekTrack(int trackNum, DMboolean waitForCompletion)
{
  DBG(fprintf(stderr,"Entering DatPlayer::seekTrack\n"));
  if (trackNum > 999 || trackNum < 0) {
    sprintf(_strBuf, "%s %d\n",
	    VkGetResource("*ScsiPlayer*invalidProgramNumberMsg", XmCString),
	    trackNum);  
    theErrorDialog->postAndWait(_strBuf);
    return DM_FAILURE;
  }

  DMstatus status = DM_SUCCESS;
  int oldTrackNum = _transport->track;

  if (_playState == Playing)	// let the play loop handle it.
    _seekTrackRequest = trackNum;
  else {
    PlayState origPlayState = _playState;
    _playState = Seeking;
    status = seekTrackAux(trackNum, waitForCompletion);
    if (status == DM_FAILURE) {
      sprintf(_strBuf, "%s %d.\n",
	      VkGetResource("DatPlayer*failedProgramSeekMsg", XmCString),
	      trackNum);  
      theErrorDialog->post(_strBuf);
      datAbort(_datTransport);
      status = seekTrackAux(oldTrackNum, waitForCompletion);
    }
    _playState = origPlayState;
    updateTransport();     
    // restart the display
    theScsiPlayerApp->startDisplayUpdate();
    // Hack around firmware bug.
    _transport->progHr = _transport->progMin = 0;
    _transport->progSec = _transport->progFrame = 0;

  }

  return status;
}


DMstatus DatPlayer::seekTrackAux(int trackNum, DMboolean waitForCompletion)
{
  DMstatus status;
  DBG(fprintf(stderr,"DAT seekTrackAux %d; waitForCompletion = %d\n",
	      trackNum, waitForCompletion));

  datAbort(_datTransport);

  if (trackNum > _transport->lastTrack)
    waitForCompletion = DM_TRUE;

  if (trackNum < 1) {
    status = datSeekBOT(_datTransport, waitForCompletion);
  }
  else {
    _datSeekInfo->seektype = DAT_SEEK_PROG;
    _datSeekInfo->prognum = trackNum;
    status = datSeekPosition(_datTransport, _datSeekInfo, waitForCompletion);
  }

  DBG(reportStatus());
  updateTransport();		// DM_TRUE

  if (!waitForCompletion) {
    status = spinDisplays();
    updateTransport();
  }
  return status;
}
  

void DatPlayer::seekFrame(unsigned long frame, DMboolean waitForCompletion)
{
  PlayState origPlayState = _playState;
  if (origPlayState == Playing)
    stop();
  seekFrameAux(frame, waitForCompletion);
  updateTransport();
  theScsiPlayerApp->startDisplayUpdate();
  if (origPlayState == Playing)
    play();
}

DMstatus DatPlayer::seekFrameAux(unsigned long frame, DMboolean waitForCompletion)
{
  DMstatus status;
  DBG(fprintf(stderr,"Entering DatPlayer::seekFrameAux %d; waitForCompletion = %d ...",
	      frame, waitForCompletion));
  datAbort(_datTransport);

  if (frame < 1) 
    status =datSeekBOT(_datTransport, waitForCompletion);
  else {
    _datSeekInfo->seektype = DAT_SEEK_ABS;
    datFrameToTc(frame, &_datSeekInfo->atime);
    status = datSeekPosition(_datTransport, _datSeekInfo, waitForCompletion);
  }

  if (status == DM_FAILURE) 
    reportStatus();
  updateTransport();

  if (!waitForCompletion)
    spinDisplays();

  DBG(fprintf(stderr,"done.\n"));
  return DM_SUCCESS;
}

void DatPlayer::stateChange()
{

  switch (_mediaType) {
  case DATmediaAudio:
    if (theWarningDialog->baseWidget() && XtIsManaged(theWarningDialog->baseWidget()))
      theWarningDialog->unpost();	// in case something was posted.
    break;
  case DATmediaData:
    if (!theWarningDialog->baseWidget() || !XtIsManaged(theWarningDialog->baseWidget()))
      theWarningDialog->post(VkGetResource("*ScsiPlayer*dataMediaMsg", XmCString));
    break;
  case DATmediaUnknown:
    if (!theWarningDialog->baseWidget() || !XtIsManaged(theWarningDialog->baseWidget()))
      theWarningDialog->post(VkGetResource("*ScsiPlayer*blankMediaMsg", XmCString));
    break;
  case DATmediaNotTape:
    if (!theWarningDialog->baseWidget() || !XtIsManaged(theWarningDialog->baseWidget()))
      theWarningDialog->post(VkGetResource("*ScsiPlayer*noMediaMsg", XmCString));
  }
}

// actually just initiates play.  stateChange handle the bookkeeping once
// we are running.
//
void DatPlayer::play() 
{
  _playState = Playing;
  _childThread = sproc(DatPlayer::staticSpawnedPlay, PR_SALL & ~PR_SID);
  // pthread_create(&_childThread, NULL, DatPlayer::staticSpawnedPlay, NULL);
}

void DatPlayer::killThread()
{
  sigset(SIGCLD, SIG_DFL);
  if (_childThread != NULL) {
    kill(_childThread, SIGTERM);
    // pthread_cancel(_childThread);
    _childThread = NULL;
  }
}


int DatPlayer::setAudioBufferRate(int sampFreq)
{
  switch(sampFreq) {
  case DAT_FREQ32000:
    _audioBuffer->setOutputRate(AL_RATE_32000);
    return DAT_DATASIZE32K;
  case DAT_FREQ48000:
    _audioBuffer->setOutputRate(AL_RATE_48000);
    return DAT_DATASIZE48K;
  case DAT_FREQ44100:
  default :
    _audioBuffer->setOutputRate(AL_RATE_44100);
    return DAT_DATASIZE44K;
  }
}

void DatPlayer::spawnedPlay() 
{
  int bytesRead;
  int localTrackCache = _transport->track;
  int audioBufferSize;
  DMboolean initialPlay = DM_TRUE;
  DMboolean mute = DM_FALSE;

  theScsiPlayerApp->boostPriority();
  datAbort(_datTransport);

  if (_selectionEnd && theScsiPlayerApp->_playSelection)
    seekFrameAux(_selectionStart);

  while (_playState != Stopped) {

    if (_seekTrackRequest != NO_SEEK_REQUESTED) {
      seekTrackAux(_seekTrackRequest);
      localTrackCache = _seekTrackRequest;
      _seekTrackRequest = NO_SEEK_REQUESTED;
      initialPlay = DM_TRUE;
    }

    if (_seekFrameRequest != NO_SEEK_REQUESTED) {
      seekFrameAux(_seekFrameRequest);
      _seekFrameRequest = NO_SEEK_REQUESTED;
      initialPlay = DM_TRUE;
    }

    mute = DM_FALSE;

    bytesRead = datRead(_datTransport, (unchar *)&_datFrame,
			    sizeof(DATFRAME)*DAT_READ_CHUNK_SIZE);

    if (bytesRead < 0) {
      fprintf(stderr,"datRead failed: returned %d; %s\n", bytesRead, strerror(oserror()));
      return;
    }
    else
      _transport->curBlock += DAT_READ_CHUNK_SIZE;

    struct DATtimepack* timepack =
      (struct DATtimepack*)&_datFrame[DAT_READ_CHUNK_SIZE-1].subcode.packs[DAT_PACK_ATIME-1];

    _transport->index = datBcdToDec(timepack->index.dhi, timepack->index.dlo);

    int pno;

    if (_datFrame[0].subcode.sid.pno3 == DAT_EOT_DIGIT)
      pno = DAT_EOT_PROGNUM;
    else
      pno = (int)(timepack->pno1*100 + timepack->pno2*10 + timepack->pno3);
    
    _datSeekInfo->prognum = pno;

    struct DATtimecode timecode = timepack->tc; // This was atime
    
    _datSeekInfo->atime.hour = timecode.hhi*10 + timecode.hlo;
    _datSeekInfo->atime.minute = timecode.mhi*10 + timecode.mlo;
    _datSeekInfo->atime.second = timecode.shi*10 + timecode.slo;
    _datSeekInfo->atime.frame = timecode.fhi*10 + timecode.flo;

    if (!validTrackNumber(pno)
	|| !datTcValid(&timecode))
      mute = DM_TRUE;

    timepack =
      (struct DATtimepack*)&_datFrame[DAT_READ_CHUNK_SIZE-1].subcode.packs[DAT_PACK_PTIME-1];
    timecode = timepack->tc; // This is ptime

    _datSeekInfo->ptime.hour = timecode.hhi*10 + timecode.hlo;
    _datSeekInfo->ptime.minute = timecode.mhi*10 + timecode.mlo;
    _datSeekInfo->ptime.second = timecode.shi*10 + timecode.slo;
    _datSeekInfo->ptime.frame = timecode.fhi*10 + timecode.flo;

    updateTransport();		
    
    // The reads rolled over into a new track.  Do some bookkeeping.

    if ((_transport->track != localTrackCache)
	|| (_transport->track == DAT_EOT_PROGNUM)) { 
      DBG(fprintf(stderr,"New track = %d\n", _transport->track));

      // check the sampling rate again.
      audioBufferSize = setAudioBufferRate(_datFrame[DAT_READ_CHUNK_SIZE-1].subcode.mid.sampfreq);
      
      if (theScsiPlayerApp->_repeatMode == RepeatTrack) {
	// repeat the track
	DBG(fprintf(stderr,"repeat track %d\n", localTrackCache));
	seekTrackAux(localTrackCache);
      }
      // check to see if we ran off the end
      else if (theScsiPlayerApp->_repeatMode == RepeatAlways) {
	if ((_transport->track == 0)
	    || (_transport->track > _transport->lastTrack)
	    || (_transport->track == DAT_EOT_PROGNUM))
	  {
	    seekTrackAux(1);
	}
      }
      // unlike the CD we just keep going
      else {
	updateTransport();	// DM_TRUE
	
	if (pno == DAT_EOT_PROGNUM) {
	  stop();	   // this will terminate the play loop
	  seekTrackAux(1);
	  while (areWeSeeking())
	    sginap(10);
	}
      }
      // track change bookkeeping
      localTrackCache = _transport->track;
    }

    if (initialPlay) {
      _audioBuffer->play((char*)_preBuf, PREBUFBLOCKSIZE*2);
      initialPlay = DM_FALSE;
      audioBufferSize = setAudioBufferRate(_datFrame[DAT_READ_CHUNK_SIZE-1].subcode.mid.sampfreq);
    }

    if (!mute)
      for (int i = 0; i < DAT_READ_CHUNK_SIZE; i++)
	if (!_audioBuffer->play((char*)_datFrame[i].audio, audioBufferSize )) {
	  _errorString = _audioBuffer->getErrorString();
	  fprintf(stderr, "Writing sample to audio failed.\n%s\n",
		  _errorString);
	}
  

    // selection loop
    if (_selectionEnd &&  theScsiPlayerApp->_playSelection
	&& (_transport->curBlock >= _selectionEnd)) {
      seekFrameAux(_selectionStart);
      if (!theScsiPlayerApp->_loopSelection)
	stop();
    }	
  }
  
  _audioBuffer->closeOutput();	// this gets reopened on first read.
  DBG(fprintf(stderr,"spawnedPlay: exiting play thread \n"));
  _childThread = NULL;
}

void DatPlayer::staticSpawnedPlay(void *) 
{
  theScsiPlayerApp->player()->spawnedPlay();
  //return NULL;
}


void DatPlayer::stop()
{
  _playState = Stopped;
}

// The actual eject is done in ScsiPlayerApp.c++ via the shell command "eject".
//
void DatPlayer::eject()
{
  stop();
  datAbort(_datTransport);
  DBG(fprintf(stderr,"Ejecting %s\n",theScsiPlayerApp->_scsiDeviceName));
  theScsiPlayerApp->stopDisplayUpdate();
  if (theWarningDialog->baseWidget() && XtIsManaged(theWarningDialog->baseWidget()))
      theWarningDialog->unpost();

  _mediaType = DATmediaNotTape;
  mediad_release_exclusiveuse(_transport->exclId);
  if (theScsiPlayerApp->_audioMediaLoaded) {
    theScsiPlayerApp->_audioMediaLoaded = NULL;
    MainWindow* mw = (MainWindow*)theScsiPlayerApp->mainWindow();
    mw->clearDisplay();
    theScsiPlayerApp->database()->terminate();
  }
}

// DAT audio frames must have the audio byte-swapped 
//
void SwapCopy(ushort *a, ushort *b, int n)
{
  ushort *done = a + n;
  while(a != done) {
    *b++ = ((*a & 0xff) << 8)+(*a >> 8);
    a++;
  }
}

DMboolean DatPlayer::checkDiskSpace(const char* filename, long bytesNeeded)
{
  struct statfs* statfsBuf;
  statfsBuf = new struct statfs;

  statfs (filename, statfsBuf, sizeof(struct statfs), 0);
  if ((statfsBuf->f_bfree * statfsBuf->f_bsize) <  bytesNeeded)
    {
      theErrorDialog->postAndWait(VkGetResource("*ScsiPlayer*notEnoughDiskSpaceForSaveMsg",
						XmCString));
      return DM_FALSE;
    }
  return DM_TRUE;
}

void DatPlayer::saveTrackAs(const char* filename)
{
  int sz = 0;
  unsigned long trackLength = _transport->trackInfo[_transport->track]->total_blocks;


  if (!checkDiskSpace(filename, trackLength * DAT_DATASIZE))
    return;

  AFfilehandle file = NULL;
  AFfilesetup fset = AFnewfilesetup();
  AFinitchannels(fset,AF_DEFAULT_TRACK,2);
  switch (_datFrame[0].subcode.mid.sampfreq) {
  case DAT_FREQ32000 :
    sz = DAT_DATASIZE32K/sizeof(short);
    AFinitrate(fset,AF_DEFAULT_TRACK,32000);
    break;
  case DAT_FREQ44100 :
    sz = DAT_DATASIZE44K/sizeof(short);
    AFinitrate(fset,AF_DEFAULT_TRACK,44100);
    break;
  case DAT_FREQ48000 :
    sz = DAT_DATASIZE48K/sizeof(short);
    AFinitrate(fset,AF_DEFAULT_TRACK,48000);
    break;
  }
	
  file = AFopenfile(filename, "w", fset);
  AFfreefilesetup(fset);
  if (file == NULL) {
    sprintf(_strBuf, "%s\n%s",
	    VkGetResource("*ScsiPlayer*noOutputFileMsg", XmCString),
	    strerror(oserror()));  
    theErrorDialog->postAndWait(_strBuf);
    return;
  }

  theScsiPlayerApp->busy();
  stop();
  seekTrack(_transport->track);	// Get in the right place
  while (areWeSeeking())
    sginap(10);

  sprintf(_strBuf, VkGetResource("*ScsiPlayer*writingTrackAsFormatString", XmCString),
	  _transport->track, filename);
  if (trackLength) {
    theProgressDialog->setPercentDone(0);
    theProgressDialog->post(_strBuf);
  }
  else
    theBusyDialog->post(_strBuf);

  int readSize = sizeof(DATFRAME); // *DAT_READ_CHUNK_SIZE;
  
  for (int i = 0; ; i++) {
    int bytesRead = datRead(_datTransport, (unchar *)&_datFrame[0], sizeof(DATFRAME));

    if (bytesRead < readSize) {
      fprintf(stderr,"datRead failed: returned %d; %s\n", bytesRead, strerror(oserror()));
      break;
    }

    // get the program number from the frame

    int pno = datPnoToDec(_datFrame[0].subcode.sid.pno1, 
			  _datFrame[0].subcode.sid.pno2,
			  _datFrame[0].subcode.sid.pno3);

    if (_transport->track != pno) {
      DBG(fprintf(stderr,"Found program number %d.  Terminate write.\n"));
      break;			// break from the write loop
    }

    SwapCopy((ushort *)&_datFrame[0].audio,
	     (ushort *)&_datFrame[0].audio,
	     sz);
    
    if (AFwriteframes(file, AF_DEFAULT_TRACK, &_datFrame[0].audio,
		      sz / 2) != sz/2) {
      fprintf(stderr,"file write failed (is your disk full?)\n");
      break;
    }
    
    if (theProgressDialog->wasInterrupted()) 
      break;

    if ( (i % 10) == 0)		// let's not do this every frame
      theProgressDialog->setPercentDone((i*100)/trackLength);
  }
    
  if (file)
    AFclosefile(file);

  theProgressDialog->unpost();
  theScsiPlayerApp->notBusy();
}

void DatPlayer::saveSelectionAs(const char* filename, char* targetString)
{

  int sz = 0;
  unsigned long selectionLength = _selectionEnd - _selectionStart;
  if (!checkDiskSpace(filename, selectionLength * DAT_DATASIZE))
    return;
 
  AFfilehandle file = NULL;
  AFfilesetup fset = AFnewfilesetup();
  AFinitchannels(fset,AF_DEFAULT_TRACK,2);
  switch (_datFrame[0].subcode.mid.sampfreq) {
  case DAT_FREQ32000 :
    sz = DAT_DATASIZE32K/sizeof(short);
    AFinitrate(fset,AF_DEFAULT_TRACK,32000);
    break;
  case DAT_FREQ44100 :
    sz = DAT_DATASIZE44K/sizeof(short);
    AFinitrate(fset,AF_DEFAULT_TRACK,44100);
    break;
  case DAT_FREQ48000 :
    sz = DAT_DATASIZE48K/sizeof(short);
    AFinitrate(fset,AF_DEFAULT_TRACK,48000);
    break;
  }
	
  file = AFopenfile(filename, "w", fset);
  AFfreefilesetup(fset);
  if (file == NULL) {
    sprintf(_strBuf, "%s\n%s",
	    VkGetResource("*ScsiPlayer*noOutputFileMsg", XmCString),
	    strerror(oserror()));  
    theErrorDialog->postAndWait(_strBuf);
    return;
  }

  if (targetString == NULL)
    targetString = (char*)filename;

  theScsiPlayerApp->busy();
  stop();

  if (_selectionEnd)			      
    sprintf(_strBuf, "Writing Track %d [%02d:%02d.%d - %02d:%02d.%d] to %s.",
	    _transport->track,
 	    (int)_selectionStart/33/60,
	    (int)_selectionStart/33 % 60,
	    (int)_selectionStart % 33,
 	    (int)_selectionEnd/33/60,
	    (int)_selectionEnd/33 % 60,
	    (int)_selectionEnd % 33,
	    targetString);
  else {
    fprintf(stderr,"No Selection.\n");
    return;
  }

  seekFrame(_selectionStart, DM_FALSE);	// Get in the right place
  theProgressDialog->setPercentDone(0);
  theProgressDialog->post(_strBuf);

  int readSize = sizeof(DATFRAME); // *DAT_READ_CHUNK_SIZE

  for (int i = 0; i < selectionLength; i++) {
    int bytesRead = datRead(_datTransport, (unchar *)&_datFrame[0], readSize);
    if (bytesRead < readSize) {
      fprintf(stderr,"datRead failed: returned %d; %s\n", bytesRead, strerror(oserror()));
      break;
    }
     
    SwapCopy((ushort *)&_datFrame[0].audio,
	     (ushort *)&_datFrame[0].audio,
	     sz);

    if (AFwriteframes(file, AF_DEFAULT_TRACK, &_datFrame[0].audio,
		      sz / 2) != sz/2) {
      fprintf(stderr,"file write failed (is your disk full?)\n");
      return;
    }
    
    if (theProgressDialog->wasInterrupted()) 
      break;

    if ( (i % 10) == 0)		// let's not do this every frame
      theProgressDialog->setPercentDone((i*100)/selectionLength);
  }
    
  if (file)
    AFclosefile(file);
  theProgressDialog->unpost();
  theScsiPlayerApp->notBusy();
}


char* DatPlayer::exportClipBoard()
{
  char* pathname = tempnam("/usr/tmp", "datplayer_clipboard.aiff");
  saveSelectionAs(pathname, "the Clipboard");
  return pathname;
}


void UpdateStartMsf(TRACKINFO* info)
{
  DMtimecode tc;

  datFrameToTc(info->start_block, &tc);
  info->start_min = tc.minute;
  info->start_sec = tc.second;
  info->start_frame = tc.frame;

}

void UpdateTotalMsf(TRACKINFO* info)
{
  DMtimecode tc;

  datFrameToTc(info->total_blocks, &tc);
  info->total_min = tc.minute;
  info->total_sec = tc.second;
  info->total_frame = tc.frame;
}

void UpdateMsf(TRACKINFO* info)
{
  UpdateStartMsf(info);
  UpdateTotalMsf(info);
}

// This should only get called when we stumble on a new track.
//
void DatPlayer::computeTrackInfo(int trackNum)
{
  DBG(fprintf(stderr,"computeTrackInfo for %d\n", trackNum));
  switch (trackNum) {		
  case 0: 
    return;
  case DAT_EOT_PROGNUM:
    // try to compute this from the current time.
    _transport->trackInfo[_transport->track]->total_blocks =
      _transport->curBlock - _transport->trackInfo[_transport->track]->start_block;
    theScsiPlayerApp->database()->
      setTrackDuration(_transport->track,
		       _transport->trackInfo[_transport->track]->total_blocks);
    UpdateMsf(_transport->trackInfo[_transport->track]);
    break;
  default:
    
    if (_transport->trackInfo[trackNum] == NULL) {
      _transport->trackInfo[trackNum] = (TRACKINFO*)malloc(sizeof(TRACKINFO));
      memset(_transport->trackInfo[trackNum], 0, sizeof(TRACKINFO));
    }
    _transport->trackInfo[trackNum]->start_block = _transport->curBlock;
    _transport->trackInfo[trackNum]->total_blocks = 0;

    UpdateMsf(_transport->trackInfo[trackNum]);
    theScsiPlayerApp->database()->setTrackStart(trackNum,
						_transport->trackInfo[trackNum]->start_block);
    if (trackNum > 1) {
      if (_transport->trackInfo[trackNum-1] == NULL) {
	_transport->trackInfo[trackNum-1] = (TRACKINFO*)malloc(sizeof(TRACKINFO));
	memset(_transport->trackInfo[trackNum-1], 0, sizeof(TRACKINFO));
      }
      _transport->trackInfo[trackNum-1]->total_blocks =
	_transport->trackInfo[trackNum]->start_block
	-_transport->trackInfo[trackNum-1]->start_block;

      theScsiPlayerApp->database()->
	setTrackDuration(trackNum-1,
			 _transport->trackInfo[trackNum-1]->total_blocks);
    UpdateTotalMsf(_transport->trackInfo[trackNum-1]);
    }
  }
}


DMstatus DatPlayer::updateTransportFromRead() 
{

  int readSize = sizeof(DATFRAME);

  int bytesRead = datRead(_datTransport, (unchar *)&_datFrame[0], sizeof(DATFRAME));

  if (bytesRead < readSize) {
    fprintf(stderr,"updateTransportFromRead: datRead failed, returned %d; %s\n",
	    bytesRead, strerror(oserror()));
    return DM_FAILURE;
  }
 
  struct DATtimepack* timepack =
      (struct DATtimepack*)&_datFrame[0].subcode.packs[DAT_PACK_ATIME-1];

  struct DATtimecode timecode = timepack->tc; // This was atime
  
  if (!datTcValid(&timecode))
    return DM_FAILURE;

  _transport->index = datBcdToDec(timepack->index.dhi, timepack->index.dlo);

  int pno = datPnoToDec(timepack->pno1, timepack->pno2, timepack->pno3);

  
  _transport->track = pno;
      
  _transport->absHr = datBcdToDec(timecode.hhi, timecode.hlo);
  _transport->absMin = datBcdToDec(timecode.mhi, timecode.mlo);
  _transport->absSec = datBcdToDec(timecode.shi, timecode.slo);
  _transport->absFrame = datBcdToDec(timecode.fhi, timecode.flo);

  DMtimecode dmtc;
  datTimeToDMtime(&timecode, &dmtc);
  _transport->curBlock = datTcToFrame(&dmtc);

  timepack = (struct DATtimepack*)&_datFrame[0].subcode.packs[DAT_PACK_PTIME-1];
  timecode = timepack->tc; // This is ptime

  _transport->progHr = datBcdToDec(timecode.hhi, timecode.hlo);
  _transport->progMin = datBcdToDec(timecode.mhi, timecode.mlo);
  _transport->progSec = datBcdToDec(timecode.shi, timecode.slo);
  _transport->progFrame = datBcdToDec(timecode.fhi, timecode.flo);

  DBG(fprintf(stderr,
	      "updateTransportFromRead reports:  track = %x, atime = %d:%d:%d, ptime = %d:%d:%d\n",
	      _transport->track, 
	      _transport->absMin, _transport->absSec, _transport->absFrame,
	      _transport->progMin, _transport->progSec, _transport->progFrame
	      ));

  return DM_SUCCESS;
}

DMstatus DatPlayer::updateTransport(DMboolean needRead)
{

  // figure out the best way to get the information.
  
  if (_playState == Playing) {
    // do nothing
  }
  else if (needRead) {		//  && (_playState != Playing)
    _datSeekInfo->seektype = DAT_SEEK_PROG;
    if (datReadPosition(_datTransport, _datSeekInfo, needRead, DM_FALSE,
			  &_datFrame[0]) == DM_FAILURE) {
      DBG(fprintf(stderr,"datReadPosition  failed\n"));
      DBG(reportStatus());
      return DM_FAILURE;
    }
  }
  else {
    _datQueryInfo->seektype = DAT_SEEK_ABS;
    if (datQueryPosition(_datTransport, _datSeekInfo) == DM_SUCCESS) {    
      DBG(fprintf(stderr,
		  "updateTransports call to datQueryPosition reports:  track = %d, atime = %d:%d:%d\n",
		  _datSeekInfo->prognum, 
		  _datSeekInfo->atime.minute,
		  _datSeekInfo->atime.second,
		  _datSeekInfo->atime.frame
		  ));
    }
    else {
      DBG(fprintf(stderr,"datQueryPosition failed\n"));
      DBG(reportStatus());
      return DM_FAILURE;
    }
  }
  
  _transport->track = _datSeekInfo->prognum;
  _transport->absMin = _datSeekInfo->atime.minute;
  _transport->absSec = _datSeekInfo->atime.second;
  _transport->absFrame = _datSeekInfo->atime.frame;
  
  _transport->progMin = _datSeekInfo->ptime.minute;
  _transport->progSec = _datSeekInfo->ptime.second;
  _transport->progFrame = _datSeekInfo->ptime.frame;

  DBG(fprintf(stderr,
	      "updateTransport reports:  track = %d, atime = %d:%d:%d, ptime = %d:%d:%d\n",
	      _transport->track, 
	      _transport->absMin, _transport->absSec, _transport->absFrame,
	      _transport->progMin, _transport->progSec, _transport->progFrame
	      ));
  
  // keep a high-water mark in case we are building these incrementally.
  
  int trackNum = _transport->track;
  
  if (trackNum == (_transport->lastTrack+1)) {
    _transport->lastTrack = trackNum;
    if (theScsiPlayerApp->database())
      theScsiPlayerApp->database()->setAlbumTracks(trackNum);
    computeTrackInfo(trackNum);
  }
  
  _transport->curBlock = datTcToFrame(&_datSeekInfo->atime);
  
  return DM_SUCCESS;
}


void DatPlayer::readTrackForLength(int trackNum)
{
  datWaitForCompletion(_datTransport);
  if (seekTrackAux(trackNum) == DM_FAILURE) {
    //    sprintf(_strBuf, "readTrackForLength: Couldn't seek to track %d\n", trackNum);
    fprintf(stderr,"readTrackForLength: Couldn't seek to track %d\n", trackNum);
    reportStatus();
    }

  DMtimecode tc;
  int absHr, absMin, absSec, absFrame;
  unsigned long oldFrame, newFrame;
  oldFrame = absMin = absSec =  absFrame = 0;

  datWaitForCompletion(_datTransport);
  datAbort(_datTransport);
  while (updateTransportFromRead() != DM_FAILURE) {

    tc.hour = _transport->absHr;
    tc.minute = _transport->absMin;
    tc.second = _transport->absSec;
    tc.frame = _transport->absFrame;
    newFrame = datTcToFrame(&tc);
    if ((_transport->track != trackNum)
	 || (newFrame < oldFrame)
	) {
      break;
    }
    else {
      oldFrame = newFrame;
      absHr = _transport->absHr;
      absMin = _transport->absMin;
      absSec = _transport->absSec;
      absFrame = _transport->absFrame;
    }
  }
  _transport->track = trackNum;
  _transport->absHr = absHr;
  _transport->absMin = absMin;
  _transport->absSec = absSec;
  _transport->absFrame = absFrame;
  _transport->curBlock = oldFrame;
}

void DatPlayer::scanDat(int startTrack)
{

  MainWindow* mw = (MainWindow*)theScsiPlayerApp->mainWindow();
  if (mw->visible()) 
    mw->clearDisplay();

  if (!theScsiPlayerApp->database()->isInitialized())
    theScsiPlayerApp->database()->initialize(_transport->key);
  
  theScsiPlayerApp->stopDisplayUpdate();
  theScsiPlayerApp->setBusyDialog(theInterruptDialog);
  theScsiPlayerApp->busy();
  theInterruptDialog->setTitle("DATplayer");
  datWaitForCompletion(_datTransport);
  datAbort(_datTransport);
  datSeekBOT(_datTransport, DM_TRUE);
  
  _datSeekInfo->seektype = DAT_SEEK_PROG;

  // See if we are starting from scratch
  if ((startTrack == 1) && (_transport->lastTrack != 0)){
    _transport->lastTrack = 0;
  }

  for (int i = startTrack; ; i++) {
    char text[2048];
    sprintf(text, VkGetResource("DatPlayer*datScanningMsg", XmCString ),i);
    
    // only in 6.4 and beyond
    theInterruptDialog->enableCancelButton(True);

    // for some reason, we have to call this everytime around the loop.
    theInterruptDialog->setButtonLabels("",
					VkGetResource("DatPlayer*datScanningButtonLabel",
						      XmCString ));
    theInterruptDialog->post(text);

    // if we are in the middle of something, just wait.
    datWaitForCompletion(_datTransport);
    DMstatus status = seekTrackAux(i);

    DBG(fprintf(stderr,"Scanning for track %d; status = %d\n",
		i, status));
    DBG(reportStatus());

    if (status == DM_SUCCESS) {
      updateTransport();

      // did we find the track we were looking for?
      if ((_transport->track == i)){
	reportSeekInfo("Scanning info");
	computeTrackInfo(i);
      }
    }
    else {
      reportSeekInfo("Scan Automatically Terminated");
      updateTransport();
      readTrackForLength(_transport->lastTrack);
      computeTrackInfo(DAT_EOT_PROGNUM);
      break;
    }
    if (theInterruptDialog->wasInterrupted()) {
      reportSeekInfo("Scan Manually Terminated");
      break;
    }
  }

  seekTrackAux(1);			// cue track one
  updateTransport();
  theInterruptDialog->unpost();
  theScsiPlayerApp->notBusy();
  theScsiPlayerApp->_flagDisplayForNewMedia = True;
  theScsiPlayerApp->startDisplayUpdate();
}


// This function tries to mimic what the cd database key generator does.  
//
DMstatus DatPlayer::setKey()
{
  datWaitForCompletion(_datTransport);
  datAbort(_datTransport);
  datSeekBOT(_datTransport, DM_TRUE);
  seekFrameAux(_keyFrame);	
  datWaitForCompletion(_datTransport);
  int readSize = sizeof(DATFRAME);
  int bytesRead = datRead(_datTransport, (unchar*)&_datFrame[0], readSize);
 if (bytesRead < readSize) {
   fprintf(stderr,"datRead in setkey() failed: returned %d.\n", bytesRead);
   reportStatus();
   return DM_FAILURE;
  }

  char n[6];
  n[0] = _datFrame[0].audio[7]; // Nate
  n[1] = _datFrame[0].audio[18]; 
  n[2] = _datFrame[0].audio[91]; 
  n[3] = _datFrame[0].audio[4]; // Ellen
  n[4] = _datFrame[0].audio[19]; 
  n[5] = _datFrame[0].audio[96]; 
  
  int i, j;
  for (i = 0,  j = 0 ; i < 6; i++, j += 2) {
    _keyString[j] = table[(int)n[i]>>2 ];
    _keyString[j+1] = table[(int)n[i]&0x3f ];
  }
  _keyString[12] = '\0';
  strcpy(_transport->key, _keyString);
  DBG(fprintf(stderr,"key = %s\n",_keyString));
  return DM_SUCCESS;
} 

void DatPlayer::readInfoFile()
{
  
  DBG(fprintf(stderr,"Entering readInfoFile..."));
  _transport->track = 0;
  _transport->index = 0;
  
  XrmValue value;
  char* type;
  
  _transport->firstTrack = 1;
  if (XrmGetResource(theScsiPlayerApp->database()->xDb(), "album.tracks", "Album.Tracks",
		     &type, &value)) {
    sscanf(value.addr, "%d", &_transport->lastTrack);
  }
  
  memset(_transport->trackInfo, 0,
	 (_transport->lastTrack+1) * sizeof(_transport->trackInfo[1]));
  
  for (int i = 1; i <= _transport->lastTrack; i++) {
    char nameString[17];
    char classString[17];
    
    TRACKINFO* info;
    
    sprintf(nameString, "track%d.start", i);
    sprintf(classString, "Track%d.Start", i);
    if (XrmGetResource(theScsiPlayerApp->database()->xDb(), nameString, classString,
		       &type, &value)) {
      
      if (_transport->trackInfo[i] == NULL) 
	_transport->trackInfo[i] = (TRACKINFO*)malloc(sizeof(TRACKINFO));
      
      info = _transport->trackInfo[i];
      
      sscanf(value.addr, "%02d:%02d:%02d", &info->start_min,
	     &info->start_sec,
	     &info->start_frame);
      
      _transport->trackInfo[i]->start_block =
	(2000*info->start_min + 33* info->start_sec + info->start_sec/3
	 + info->start_frame);
    }
    else
      fprintf(stderr,"Can't find Track start info for Track %d.\n", i);
    
    sprintf(nameString, "track%d.duration", i);
    sprintf(classString, "Track%d.Duration", i);
    if (XrmGetResource(theScsiPlayerApp->database()->xDb(), nameString, classString,
		       &type, &value)) {
      sscanf(value.addr, "%02d:%02d:%02d", &_transport->trackInfo[i]->total_min,
	     &info->total_sec,
	     &info->total_frame);
      _transport->trackInfo[i]->total_blocks =
	(2000*info->total_min + 33* info->total_sec + info->total_sec/3
	 + info->total_frame);
    }
  }
  DBG(fprintf(stderr,"done.\n"));
}

DMboolean DatPlayer::tryKey()
{
  for (_keyFrame = 200; _keyFrame < 1000; _keyFrame = _keyFrame + 200) {
    setKey();
    if (strcmp(_transport->key, "000000000000") != 0) 
      return DM_TRUE;
    setKey();
    if (strcmp(_transport->key, "000000000000") != 0) 
      return DM_TRUE;
  }
  return DM_FALSE;
}

void DatPlayer::databaseSetup()
{
  // this gets called from the display code 
  if (!theDatabase || (strcmp(_transport->key, "") == 0)) {
    if (tryKey()
	&& theScsiPlayerApp->database()->initialize(_transport->key)) {
      readInfoFile();
      seekTrackAux(1);
    }
    else {
      DBG(fprintf(stderr, "didn't init DB file, scanning DAT...\n"));
      scanDat(1);
      readInfoFile();
    }
  }
}

