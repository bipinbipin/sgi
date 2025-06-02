/////////////////////////////////////////////////////////////////////////////////
//
// CdPlayer.c++
//
//   This is the player that communicates with the CD library.
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

#include <dmedia/dmedia.h>

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <signal.h>
#include <Vk/VkProgressDialog.h>
#include <Vk/VkErrorDialog.h>
#include <Vk/VkWarningDialog.h>
#include <Vk/VkResource.h>
#include "CdAudio.h"
#include "OutputFile.h"
#include "ScsiPlayerApp.h"
#include "MainWindow.h"
#include "CdPlayer.h"
#include "Semaphore.h"

#define NUM_READ_FRAMES 12
#define AIFF_HEADER_SIZE 54
#define NFS_BUFFER_SIZE 8192
#define PREBUFBLOCKSIZE 20	// number of samples, not frames
#define LATENCY_IN_SAMP_FRAMES 44100

#define AL_QUEUE_SAMP_SIZE (8*NUM_READ_FRAMES*CDDA_NUMSAMPLES)
#define AL_QUEUE_FRAME_SIZE (4*NUM_READ_FRAMES*CDDA_NUMSAMPLES)

//#define DBG(x) {x;}
#define DBG(x) {}  

#ifndef MAX
#define MAX(_x,_y) (((_x) > (_y)) ? (_x) : (_y))
#endif
#ifndef MIN
#define MIN(_x,_y) (((_x) < (_y)) ? (_x) : (_y))
#endif

CdPlayer::CdPlayer(ScsiPlayer* transport)
{
  _transport = transport;

  _playState = Stopped;
  _childThread = -1;
  _seekTrackRequest = -1;
  _seekFrameRequest = -1;
  _selectionStart = 0;
  _selectionEnd = 0;
  _mutex = new Semaphore(1);

  _cdFrameBuffer= new CDFRAME[NUM_READ_FRAMES];
  _audioBuffer = new AudioBuffer("ScsiPlayer", out, 0, AL_QUEUE_SAMP_SIZE);
  _audioBuffer->setOutputRate(AL_RATE_44100);
  _audioBuffer->setGain(theScsiPlayerApp->_initialGain);

  _errorString = _audioBuffer->getErrorString();
  if (_errorString) {
    sprintf(_strBuf, "%s \nExiting...", _errorString);  
    theErrorDialog->postAndWait(_strBuf);
    exit(1);
  } 
  _preBuf = new short[PREBUFBLOCKSIZE];
  bzero(_preBuf,sizeof(short)*PREBUFBLOCKSIZE);
}


CdPlayer::~CdPlayer()
{
  DBG(fprintf(stderr,"Calling delete method\n"));
  stop();
  CDclose(_transport);
  _audioBuffer->closeOutput();

  delete _mutex;
  delete _audioBuffer;
}

void CdPlayer::seekTrackAux(int trackNum) 
{
  reopenDevice();
  CDseektrack(_transport, trackNum);
  CDupdatestatus(_transport);
  if (trackNum != _transport->track) {
    DBG(fprintf(stderr,"seektrack = %d, transport track = %d\n", trackNum, _transport->track));
    do {
      CDreadda(_transport, _cdFrameBuffer, 1);
      CDupdatestatus(_transport);
      DBG(fprintf(stderr,"RETRY: seektrack = %d, transport track = %d\n", trackNum, _transport->track));
    } while (trackNum == _transport->track+1);
  }
}

DMboolean CdPlayer::validTrackNumber(int trackNum)
{
  if ((trackNum < 1) || (trackNum > 99))
    return DM_FALSE;
  else
    return DM_TRUE;
}

DMstatus CdPlayer::seekTrack(int trackNum, DMboolean) // always wait.
{
  if (trackNum > 99 || trackNum < 0) {
    sprintf(_strBuf, "%s %d\n%s",
	    VkGetResource("*ScsiPlayer*invalidProgramNumberMsg", XmCString),
	    trackNum,
	    strerror(oserror()));  
    theErrorDialog->postAndWait(_strBuf);
    fprintf(stderr, "Invalid location: program %d\n", trackNum);
    return DM_FAILURE;
  }

  DBG(fprintf(stderr,"seekTrack %d\n", trackNum));

  if (_playState == Playing)
    _seekTrackRequest = trackNum;
  else {
    if (theScsiPlayerApp->_audioMediaLoaded) {
      seekTrackAux(trackNum);
    }
  }
  return DM_SUCCESS;
}
  
void CdPlayer::seekFrameAux(unsigned long frame)
{
  reopenDevice();
  CDseekblock(_transport, frame);
  CDreadda(_transport, NULL, 0); // this actually moves the head.
  CDupdatestatus(_transport);
}  


// NB: Audio seeks are NOT frame-accurate.  
//
void CdPlayer::seekFrame(unsigned long frame, DMboolean )  // always wait
{
  DBG(fprintf(stderr,"seekFrame %d\n", frame));

  if (_playState == Playing)
    _seekFrameRequest = (int)frame;
  else {
    seekFrameAux(frame);
  }
}


void CdPlayer::stateChange()
{
}


// actually just initiates play.  stateChange handle the bookkeeping once
// we are running.
//
void CdPlayer::play() 
{
   DBG(fprintf(stderr,"Entering play()...\n"));
  if (theScsiPlayerApp->_audioMediaLoaded) {	
    reopenDevice();
    _playState = Playing;
    _audioBuffer->setOutputRate(AL_RATE_44100);
    _mutex->usp();
    _childThread = sproc(CdPlayer::staticSpawnedPlay, PR_SALL & ~PR_SID);
    // pthread_create(&_childThread, NULL, CdPlayer::staticSpawnedPlay , NULL);
    _mutex->usv();
  }
}


void CdPlayer::killThread()
{
  DBG(fprintf(stderr,"In killThread...\n"));
  sigset(SIGCLD, SIG_DFL);
  if (_childThread > 0) {
    kill(_childThread, SIGTERM);
    // pthread_cancel(_childThread);
    _childThread = -1;
  }
}


void CdPlayer::spawnedPlay() 
{
  DBG(fprintf(stderr,"Entering SpawnedPlay()...\n"));
  int framesRead = 0;
  int localTrackCache = _transport->track;
  float bufferFilled = 0.0;

  DMboolean endOfCd = DM_FALSE;
  DMboolean initialPlay = DM_TRUE;
  theScsiPlayerApp->boostPriority();

  if (_selectionEnd && theScsiPlayerApp->_playSelection)
    seekFrameAux(_selectionStart);

  while (_playState == Playing) {

    if (_seekTrackRequest != -1) {
      seekTrackAux(_seekTrackRequest);
      localTrackCache = _seekTrackRequest;
      _seekTrackRequest = -1;
      initialPlay = DM_TRUE;
    }

    if (_seekFrameRequest != -1) {
      seekFrameAux(_seekFrameRequest);
      _seekFrameRequest = -1;
      initialPlay = DM_TRUE;
    }

    long long trackStart = _transport->trackInfo[_transport->track]->start_block;
    long long trackEnd = trackStart + _transport->trackInfo[_transport->track]->total_blocks;

    int framesToRead = MAX( MIN(NUM_READ_FRAMES, trackEnd - _transport->curBlock), 0);
    
    if (framesToRead != 0)
      framesRead = CDreadda(_transport, _cdFrameBuffer, framesToRead);
    else
      framesRead = 0;

    DBG(fprintf(stderr,"framesToRead = %d; framesRead = %d\n", framesToRead, framesRead));

    if ((framesRead != DM_FAILURE) && (framesRead != 0)) {
      // read successful
      if (initialPlay) {
	// check buffer again.
	if (_audioBuffer->getPort())
	  bufferFilled =  (float)alGetFilled(_audioBuffer->getPort())
	/ (float)AL_QUEUE_FRAME_SIZE;
	DBG(fprintf(stderr,"Prebuffering queue of %d with %d samples; bufferFilled = %f\n",
		    AL_QUEUE_SAMP_SIZE,
		    (int)((AL_QUEUE_SAMP_SIZE)*(1.0-bufferFilled)),
		    bufferFilled));
	for (int i = 0; i < (int)((AL_QUEUE_SAMP_SIZE)*(1.0-bufferFilled)) ; i += PREBUFBLOCKSIZE) {
	  _audioBuffer->play((char*)_preBuf, PREBUFBLOCKSIZE*2);
	}
	initialPlay = DM_FALSE;
      }

      for (int i = 0; i < framesRead; i++) {
	if (!_audioBuffer->play(_cdFrameBuffer[i].audio , CDDA_DATASIZE)) {
	  fprintf(stderr, "Writing samples to audio failed.\n");
	  _errorString = _audioBuffer->getErrorString();
	}
      }
    }
    else if (localTrackCache == _transport->lastTrack)
      endOfCd = DM_TRUE;


    // The locking is because the display code can come along and display the next
    // track while we are relocating to the start of a selection.

    theScsiPlayerApp->_displayMutex->usp();
    if ((CDupdatestatusfromframe(_transport, &_cdFrameBuffer[framesRead-1]) == 0)
	|| (_transport->track == 0)
	|| (_transport->track > localTrackCache+1))
      CDupdatestatus(_transport);

    // selection loop

    if (_selectionEnd
	&&  theScsiPlayerApp->_playSelection
	&& ((_transport->curBlock > _selectionEnd)
	    || (_transport->track != localTrackCache))) {
      DBG(fprintf(stderr,"processing selection end\n"));
      if (!theScsiPlayerApp->_loopSelection) {
	stop();
	seekFrameAux(_selectionStart);
	CDupdatestatus(_transport);
	theScsiPlayerApp->_displayMutex->usv();
	break;
      }
      else {
	seekFrameAux(_selectionStart);
	CDupdatestatus(_transport);
	theScsiPlayerApp->_displayMutex->usv();
	continue;
      }
    } 

    theScsiPlayerApp->_displayMutex->usv();
    if (endOfCd || (_transport->track != localTrackCache)) {
      // The reads rolled over into a new track.  Handle the repeat mode crud
      // here.  
      DBG(fprintf(stderr,"Track changed from %d to %d at %ld;  trackEnd was %lld (%lld)\n",
		  localTrackCache, _transport->track, _transport->curBlock, trackEnd,
		  trackEnd - _transport->curBlock));
      if (theScsiPlayerApp->_repeatMode == RepeatTrack) {
	// repeat the track
	DBG(fprintf(stderr,"repeat track %d\n", localTrackCache));
	seekTrackAux(localTrackCache);
	initialPlay = DM_TRUE;
      }
      else
	// check to see if we ran off the end
	if (endOfCd
	    || (_transport->track > _transport->lastTrack)
	    || (_transport->track < _transport->firstTrack)) {
	  DBG(fprintf(stderr,"ran off end...\n"));
	  endOfCd = DM_FALSE;
	  if (theScsiPlayerApp->_repeatMode == RepeatAlways) {
	    seekTrackAux(1);
	  }
	  else {
	    stop();	   // this will terminate the play loop
	    seekTrackAux(1);
	  }
	}

      if (_audioBuffer->getPort())
	bufferFilled = (float)alGetFilled(_audioBuffer->getPort())
                       / (float)AL_QUEUE_FRAME_SIZE;

      DBG(fprintf(stderr,"buffer at %f on track %d, index %d\n",
		  bufferFilled, _transport->track, _transport->index));

      if ((_transport->index == 0) || (bufferFilled < .75))
	initialPlay = DM_TRUE;
      else if (bufferFilled < .5) 
	initialPlay = DM_TRUE;

      // track change bookkeeping
      localTrackCache = _transport->track;
    }

    if (_audioBuffer->getOutputRate() != AL_RATE_44100) {
      stop();
    }
  }
	
  if ((_playState == Paused) && (_childThread > 0)) {
    blockproc(_childThread);
  }
  else {			// we're stopped
    
    _mutex->usp();
    _childThread = -1;
    _mutex->usv();
    _audioBuffer->closeOutput();	// this gets reopened on first read.
    CDreadda(_transport, NULL, 0);
    return;
  }
}

void CdPlayer::staticSpawnedPlay(void *) 
{
  DBG(fprintf(stderr,"Entering staticSpawnedPlay()...\n"));
  theScsiPlayerApp->player()->spawnedPlay();
  //return NULL;
}


void CdPlayer::stop()
{
  DBG(fprintf(stderr,"CdPlayer: Stopping\n"));
  _playState = Stopped;
}


DMstatus CdPlayer::closeDevice()
{
  if (_transport && _transport->scsiRequest) {
    CDallowremoval(_transport);
    free( SENSEBUF(_transport->scsiRequest) );
    dsclose(_transport->scsiRequest);
    _transport->scsiRequest = NULL;
  }
  return DM_SUCCESS;
}

DMstatus CdPlayer::reopenDevice()
{
  if (_transport && !_transport->scsiRequest) {
    DMboolean deviceOpened = DM_FALSE; 
    // there is a race while the read is finishing up.
    theScsiPlayerApp->busy();
    stop();
    while (deviceOpened == DM_FALSE) {
      DBG(fprintf(stderr,"Trying to re-open CD drive at %s \n",
		  theScsiPlayerApp->_scsiDeviceName));
      sginap(100);
      setreuid(-1, 0);	// get privileges
      CDopen(theScsiPlayerApp->_scsiDeviceName, "r", _transport);
      if (_transport->scsiRequest == NULL)
	deviceOpened = DM_FALSE;
      else
	deviceOpened = DM_TRUE;
      setreuid(-1, getuid());	// drop privileges
    }
    theScsiPlayerApp->notBusy();
  }
  return DM_SUCCESS;
}

int CdPlayer::openTrackFile(int trackNum)
{
  char name[19];

  // free up the scsi device.  But keep the struct alive for a bit.

  stop();
  closeDevice();

  if (_trackFd) 
    fprintf(stderr,"openTrackFile found a file descriptor");

  sprintf(name, "%s/Track%02d.aiff", theScsiPlayerApp->_cdMountDirectory,
	  trackNum);
   _trackFd = open(name, O_RDONLY|O_EXCL); 
   if (_trackFd == -1) {
    fprintf(stderr,"openTrackFile: failed to open %s.\n", name);
    _trackFd = 0;
    return -1;
  }
  else {
    // Read past the header
    if (_byteOffset = lseek(_trackFd, AIFF_HEADER_SIZE, SEEK_SET) != AIFF_HEADER_SIZE) {	 
      fprintf(stderr, "Failed to read over audio header, closing %d.\n", _trackFd);
      close(_trackFd);
      _trackFd = 0;
    }
  }
   return _trackFd;
}

DMstatus CdPlayer::closeTrackFile()
{
  if (_trackFd) {
    close(_trackFd);
    _trackFd = NULL;
  }
  return reopenDevice();
}

void CdPlayer::framesToTc(unsigned long , DMtimecode*)
{
  fprintf(stderr,"CdPlayer::framesToTc is NYI\n");
}

void CdPlayer::scanDat(int)
{
  fprintf(stderr,"CdPlayer::scanDat: This shouldn't be called.\n");
}

DMstatus CdPlayer::setKey()
{
  fprintf(stderr,"CdPlayer::setKey: This shouldn't be called.\n");
  return DM_SUCCESS;
}

// CD audio frames must have the audio byte-swapped 
//
void SwapCopy(ushort *a, ushort *b, int n)
{
  ushort *done = a + n;
  while(a != done) {
    *b++ = ((*a & 0xff) << 8)+(*a >> 8);
    a++;
  }
}

DMboolean CdPlayer::checkDiskSpace(const char* filename, long bytesNeeded)
{
  struct statfs* statfsBuf;
  statfsBuf = new struct statfs;
  int result;

  result = statfs (filename, statfsBuf, sizeof(struct statfs), 0);
  if (result != 0 )
    perror("statfs failed:");

  if ((statfsBuf->f_bfree * statfsBuf->f_bsize) <  bytesNeeded)
    {
      theErrorDialog->postAndWait(VkGetResource("*ScsiPlayer*notEnoughDiskSpaceForSaveMsg",
						XmCString));
      return DM_FALSE;
    }
  return DM_TRUE;
}

void CdPlayer::saveTrackAs(const char* filename)
{

  OutputFile* of = new OutputFile((char*)filename);

  if (!of->open(AL_RATE_44100)) {
    sprintf(_strBuf, "%s\n%s",
	    VkGetResource("*ScsiPlayer*noOutputFileMsg", XmCString),
	    strerror(oserror()));  
    theErrorDialog->postAndWait(_strBuf);
    return;
  }


  TRACKINFO* info = _transport->trackInfo[_transport->track];

  if (!checkDiskSpace(filename, (info->total_blocks * CDDA_DATASIZE)))
    return;

  stop();
  killThread();
  
  theScsiPlayerApp->busy();
  theProgressDialog->setPercentDone(0);

  char msg[400];
  sprintf(msg, VkGetResource("*ScsiPlayer*writingTrackAsFormatString", XmCString),
			     _transport->track, filename);
  theProgressDialog->post(msg);

  seekTrack(_transport->track);	// Get in the right place

  int progressPc = 0;
  int progressPcCache = 0;
  
  for (int i = 0; i < info->total_blocks; i++) {
    int framesRead = CDreadda(_transport, _cdFrameBuffer, 1);

    if (framesRead < 1
	|| theProgressDialog->wasInterrupted()) // was told to shut up
      break;

    SwapCopy((ushort *)&_cdFrameBuffer[0].audio,
	     (ushort *)&_cdFrameBuffer[0].audio,
	     CDDA_NUMSAMPLES);
    
    if (AFwriteframes(of->getAFHandle(), AF_DEFAULT_TRACK, &_cdFrameBuffer[0].audio,
		      CDDA_NUMSAMPLES/2) != CDDA_NUMSAMPLES/2) {
      fprintf(stderr,"File write failed!  Is your disk full?\n");
      break;
    }
    
    progressPc = MIN((i*100) / info->total_blocks, 100);

    if (progressPc != progressPcCache) {
      DBG(fprintf(stderr,"percent = %d\n", progressPc));
      theProgressDialog->setPercentDone(progressPc);
      progressPcCache = progressPc;
    }

  }

  delete of;
  theProgressDialog->unpost();
  theScsiPlayerApp->notBusy();

}

void CdPlayer::saveSelectionAs(const char* filename, char* targetString)
{

  if (!_selectionEnd){
    theErrorDialog->postAndWait(VkGetResource("*ScsiPlayer*noSelectionMsg",
					      XmCString));	    
    return;
  }

  OutputFile* of = new OutputFile((char*)filename);
  if (!of->open(AL_RATE_44100)) {
    sprintf(_strBuf, "%s\n%s",
	    VkGetResource("*ScsiPlayer*noOutputFileMsg", XmCString),
	    strerror(oserror()));  
    theErrorDialog->postAndWait(_strBuf);
    return;
  }

  TRACKINFO* info = _transport->trackInfo[_transport->track];
  int start = (int)_selectionStart - (int)info->start_block;
  int end = (int)_selectionEnd - (int)info->start_block;

  if (!checkDiskSpace(filename, (end - start) * CDDA_DATASIZE))
    return;

  stop();
  killThread();

  if (targetString == NULL)
    targetString = (char*)filename;

  theScsiPlayerApp->busy();
  theProgressDialog->setPercentDone(0);

  char msg[400];
  char *writingtrack = VkGetResource("writingTrack", "");
  if(writingtrack) 
    sprintf(msg, writingtrack,
	  _transport->track,
	  (int)start/CDDA_FRAMES_PER_SECOND/60,
	  (int)start/CDDA_FRAMES_PER_SECOND % 60,
	  (int)start % CDDA_FRAMES_PER_SECOND,
	  (int)end/CDDA_FRAMES_PER_SECOND/60,
	  (int)end/CDDA_FRAMES_PER_SECOND % 60,
	  (int)end % CDDA_FRAMES_PER_SECOND,
	  targetString);
  else
    sprintf(msg, "Writing Track %d [%02d:%02d.%d - %02d:%02d.%d] to %s.",
	  _transport->track,
	  (int)start/CDDA_FRAMES_PER_SECOND/60,
	  (int)start/CDDA_FRAMES_PER_SECOND % 60,
	  (int)start % CDDA_FRAMES_PER_SECOND,
	  (int)end/CDDA_FRAMES_PER_SECOND/60,
	  (int)end/CDDA_FRAMES_PER_SECOND % 60,
	  (int)end % CDDA_FRAMES_PER_SECOND,
	  targetString);
  theProgressDialog->post(msg);

  int total = (end - start);
  
  seekFrame(_selectionStart);
  int progressPc = 0;
  int progressPcCache = 0;
  for (int i = 0; i < total; i++) {
    int framesRead = CDreadda(_transport, _cdFrameBuffer, 1);

    if (framesRead < 1
	|| theProgressDialog->wasInterrupted()) // was told to shut up
      break;

    SwapCopy((ushort *)&_cdFrameBuffer[0].audio,
	     (ushort *)&_cdFrameBuffer[0].audio,
	     CDDA_NUMSAMPLES);
    
    if (AFwriteframes(of->getAFHandle(), AF_DEFAULT_TRACK, &_cdFrameBuffer[0].audio,
		      CDDA_NUMSAMPLES/2) != CDDA_NUMSAMPLES/2) {
      fprintf(stderr,"file write failed (is your disk full?)\n");
      break;
    }
    
    progressPc = (i*100) / total;

    if (progressPc != progressPcCache) {
      DBG(fprintf(stderr,"percent = %d\n", progressPc));
      theProgressDialog->setPercentDone(progressPc);
      progressPcCache = progressPc;
    }
  }

  delete of;
  theProgressDialog->unpost();
  theScsiPlayerApp->notBusy();
}

char* CdPlayer::exportClipBoard()
{
  char* pathname = tempnam("/usr/tmp", "cdplayer_clipboard.aiff");
  saveSelectionAs(pathname, "the Clipboard");
  return pathname;
}

void CdPlayer::eject()
{
  stop();
  DBG(fprintf(stderr,"Ejecting %s\n",theScsiPlayerApp->_scsiDeviceName));
  theScsiPlayerApp->stopDisplayUpdate();

  theScsiPlayerApp->_audioMediaLoaded = NULL;
  theScsiPlayerApp->_dataMediaLoaded = NULL;
  if (theWarningDialog->baseWidget() && XtIsManaged(theWarningDialog->baseWidget()))
    theWarningDialog->unpost();	// in case something was posted.
  MainWindow* mw = (MainWindow*)theScsiPlayerApp->mainWindow();
  mw->clearDisplay();
  DBG(fprintf(stderr,"CdPlayer::eject clearing the CdPlayer\n"));
  theScsiPlayerApp->clearCdPlayer();
}


// Read track info from the Motif format resources file on /CDROM.
//
void CdPlayer::readInfoFile()
{
  // see if the file exists
  if (theScsiPlayerApp->_audioMediaLoaded) {
    char infoFileName[300];
    sprintf(infoFileName, "%s/info.disc", theScsiPlayerApp->_cdMountDirectory);
    theScsiPlayerApp->_trackInfoDb = XrmGetFileDatabase(infoFileName);
  }
  if (!theScsiPlayerApp->_audioMediaLoaded || !theScsiPlayerApp->_trackInfoDb) {
    fprintf(stderr,"No Track information file available.  Do you have an audio \n\
                    CD in the drive?");
    return;
  }

  XrmValue value;
  char* type;

  if (XrmGetResource(theScsiPlayerApp->_trackInfoDb, "album.key", "Album.Key",
		     &type, &value)) {
    strcpy(_transport->key, value.addr);
  }
  if (XrmGetResource(theScsiPlayerApp->_trackInfoDb, "album.duration", "Album.Duration",
		     &type, &value)) {
    sscanf(value.addr, "%02d:%02d:%02d", &_transport->totalMin, &_transport->totalSec,
	   &_transport->totalFrame);
  }

  if (XrmGetResource(theScsiPlayerApp->_trackInfoDb, "album.tracks", "Album.Tracks",
		     &type, &value)) {
    sscanf(value.addr, "%d", &_transport->lastTrack);
  }

  memset(_transport->trackInfo, 0,
	 (_transport->lastTrack+1) * sizeof(_transport->trackInfo[1]));

  for (int i = 1; i <= _transport->lastTrack; i++) {
    char nameString[17];
    char classString[17];

    CDTRACKINFO* info;

    sprintf(nameString, "track%d.start", i);
    sprintf(classString, "Track%d.Start", i);
    if (XrmGetResource(theScsiPlayerApp->_trackInfoDb, nameString, classString,
		       &type, &value)) {

      if (_transport->trackInfo[i] == NULL) 
	_transport->trackInfo[i] = (TRACKINFO*)malloc(sizeof(TRACKINFO));
					   
      info = _transport->trackInfo[i];

      sscanf(value.addr, "%02d:%02d:%02d", &info->start_min,
	     &info->start_sec,
	     &info->start_frame);
    }
    else
      fprintf(stderr,"Can't find Track info.\n");

    info->start_block = CDmsftoblock(_transport, info->start_min,
				     info->start_sec, info->start_frame);

    sprintf(nameString, "track%d.duration", i);
    sprintf(classString, "Track%d.Duration", i);
    if (XrmGetResource(theScsiPlayerApp->_trackInfoDb, nameString, classString,
		       &type, &value)) {

      sscanf(value.addr, "%02d:%02d:%02d", &_transport->trackInfo[i]->total_min,
	     &info->total_sec,
	     &info->total_frame);
    }

    info->total_blocks = CDmsftoframe(info->total_min, info->total_sec,
				      info->total_frame);
  }
}


void CdPlayer::databaseSetup()
{
  CDupdatestatus(_transport);
  readInfoFile();
  if (!theScsiPlayerApp->database()->initialize(_transport->key)) {
    DBG(fprintf(stderr, "Couldn't init DB file.\n"));
  }
}

// This is so the UI can call into the player to force an update
//
DMstatus CdPlayer::updateTransport(DMboolean)
{
  if (CDupdatestatus(_transport))
    return DM_FAILURE;
  else
    return DM_SUCCESS;
}
