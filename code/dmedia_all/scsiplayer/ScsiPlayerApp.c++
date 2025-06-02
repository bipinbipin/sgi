//////////////////////////////////////////////////////////////////////////////
//
// ScsiPlayerApp.c++
//
// 	The "app" object for the cd/datplayer 
//
//
// Copyright 1996, Silicon Graphics, Inc.
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

#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <libc.h>
#include <libgen.h>
#include <invent.h>
#include <mntent.h>
#include <string.h>
#include <mediad.h>
#include <signal.h>
#include <siginfo.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/stat.h>

#include <X11/Intrinsic.h>
#include <Xm/AtomMgr.h>
#include <VkWarningDialog.h>
#include <VkErrorDialog.h>
#include <VkProgressDialog.h>
#include <VkRunOnce2.h>
#include <Vk/VkNameList.h>
#include <Vk/VkResource.h>
#include <VkChildrenHandler.h>

#include <dmedia/dataudio2.h>
#include "CdAudio.h"
#include "MainWindow.h"
#include "ScsiPlayerApp.h"
#include "CdPlayer.h"
#include "DatPlayer.h"

//#define DBG(x) {x;}
#define DBG(x) {}  

static Atom _xaCLIPBOARD = None;
static Atom _xaSGI_AUDIO_FILE = None;
static Atom _xa_SGI_AUDIO_FILENAME = None;
static Atom _xaSCSIPLAYER_PRIVATE = None;

// possible command line arguments, used by parseCommandLine()
//
static XrmOptionDescRec commandLineArgs[] = {
    {"-device", "*device", XrmoptionSepArg, "cd"},
    {"-nofork", "*autoFork", XrmoptionNoArg, "False"},
    {"-sm", "*small", XrmoptionNoArg, "True"},
    {"-dsdebug", "*dsdebug", XrmoptionNoArg, "True"}
};

ScsiPlayerApp* theScsiPlayerApp = NULL;

// resources to be looked for in the X resource database by getResources()

XtResource ScsiPlayerApp::resources[] = {
    { 
	"device", 
	"Device", 
	XmRString,  
	sizeof(XmRString), 
	XtOffset(ScsiPlayerApp*, _device), 
	XmRString, 
	NULL,
    }, 
    { 
	"autoFork", 
	"AutoFork", 
	XmRBoolean, 
	sizeof(Boolean), 
	XtOffset(ScsiPlayerApp*, _autoFork), 
	XmRString, 
	(XtPointer)"True",
    },
    { 
	"small", 
	"Small", 
	XmRBoolean, 
	sizeof(Boolean), 
	XtOffset(ScsiPlayerApp*, _smallDisplay), 
	XmRString, 
	(XtPointer)"False",
    }, 
    { 
	"dsdebug", 
	"Dsdebug", 
	XmRBoolean, 
	sizeof(Boolean), 
	XtOffset(ScsiPlayerApp*, _dsdebug), 
	XmRString, 
	(XtPointer)"False",
    } 
};


ScsiPlayerApp::ScsiPlayerApp(char *appName, int *argc, char** argv,
		 XrmOptionDescRec* optionList, int sizeOfOptions)
  : VkApp(appName, argc, argv, optionList, sizeOfOptions)
{
  theScsiPlayerApp = this;
  _programName = strdup(basename(argv[0]));
  _recordReferenceCount = 0;
  _recordingEnabled = DM_FALSE;
  _trackCache = 0;
  _selectedButton = NULL;
  _repeatMode = NoRepeat;
  _playSelection = 0;
  _loopSelection = 0;
  _displayMutex = new Semaphore(1);
  _settingsDb = NULL;
  bzero(_settingsPathname, MAXPATHLEN);
  _initialGain = 1.0;


  _timeoutInterval = STOPPED_TIMEOUT;	// number of milliseconds between redisplays
  _timeoutId = NULL; 
  _audioMediaLoaded = False;
  _dataMediaLoaded = False;
  _flagDisplayForNewMedia = False;
  _flagNotifierPopup = True;

  _scsiDeviceName = NULL;
  _homeDirectory =  strdup(getenv("HOME"));

  // parse the command line and load up the X resource datbase
  //
  int undigested = parseCommandLine(commandLineArgs, XtNumber(commandLineArgs));
  if (undigested > 1) {
    sprintf(_strBuf, VkGetResource("*ScsiPlayer*usageMsg", XmCString),
	    _programName);
    theWarningDialog->postBlocked(_strBuf);
  }

  // look up X resource database and initialize data members 
  //
  getResources(resources, XtNumber(resources) );

  _cutPaste = new VkCutPaste(baseWidget());

  struct stat statBuf;

  // set the communication device index.
  if (strcmp(_programName, "datplayer") == 0) {
    _scsiDevice = DatDev;
  }
  else {
    sprintf(_settingsPathname, "%s/%s", _homeDirectory, ".cdplayerrc");
    if ((stat(_settingsPathname, &statBuf) == 0) && S_ISREG(statBuf.st_mode))
      _settingsDb = XrmGetFileDatabase(_settingsPathname);
    _scsiDevice = CdDev;
  }

  
  _transport[_scsiDevice] = NULL;
  _player[_scsiDevice] = NULL;
  _cdMountDirectory = NULL;
    
  if (_device) {
    if (*_device == '/')
      _scsiDeviceName = _device;
    else
      _scsiDeviceName = strcat("/dev/scsi/", _device);
  }

  // Check to make sure the appropriate device exists.

  inventory_t* inv;
  if (_scsiDevice == CdDev) {
    setinvent( );
    for (inv = getinvent( ); inv; inv = getinvent( )) {
      if (inv->inv_class == INV_SCSI
	  && inv->inv_type == INV_CDROM)
	break;
    }
    endinvent( );
    if (!inv) {
      setoserror(ENODEV);	/* Can't locate a CD-ROM drive */
      theErrorDialog->postAndWait(VkGetResource("CdPlayer*noCdromDriveMsg",
						XmCString));
      exit(EXIT_FAILURE);
    }
  }

  // set up the various user options

  XrmValue value;
  char* type;

  if (XrmGetResource(_settingsDb, "gain", "Gain", &type, &value))  
    _initialGain = atof(value.addr);


  if (_smallDisplay)
    _displayTrackList = False;
  else if (_settingsDb) {
    if (XrmGetResource(_settingsDb, "displayTrackList", "DisplayTrackList", &type, &value)
	&& (strcasecmp(value.addr, "True") == 0 ))  
      _displayTrackList = True;
    else
      _displayTrackList = False;
      //fprintf(stderr,"smallDisplay = %s\n", value.addr);
  } 
  else   
    _displayTrackList = True;

  if (XrmGetResource(_settingsDb, "playSelection", "PlaySelection", &type, &value)
      && (strcasecmp(value.addr, "True") == 0 ))  
    _playSelection = True;
  else
    _playSelection = False;

  // Do an auto fork if neccessary.
  //
  if (_autoFork) {
    switch(fork()) {
    case 0:
      // We are the child.
      break;
    case -1:
      // We are the parent. Unsuccessful fork
      theErrorDialog->postBlocked( VkGetResource("*ScsiPlayer*unsucessfulForkMsg",
						 XmCString));
    default:
      // We are the parent. Successful fork.
      exit(1);
    }
  }
  if (_dsdebug)
    dsdebug = 1;
  else
    dsdebug = 0 ;

  VkRunOnce2 *runOnce = new VkRunOnce2(new VkNameList());
}


Boolean ScsiPlayerApp::checkForTape()
{
  DatPlayer* player = (DatPlayer*)_player[_scsiDevice];

  player->_mediaType = datQueryMediaType(player->_datTransport);

  if (player->_mediaType != DATmediaAudio) { // no audio tape
    _audioMediaLoaded = False;
    _timeoutInterval = STOPPED_TIMEOUT;
    return False;
  }

  _flagDisplayForNewMedia = True;
  _timeoutInterval = PLAYING_TIMEOUT;
  _audioMediaLoaded = True;
  _flagNotifierPopup = True;
  return True;
}

Boolean ScsiPlayerApp::checkForMedia()
{
  if (_scsiDevice == CdDev) {
    if (_dataMediaLoaded || !findCdMedia()) 
      return False;
  }
  else {
    if (!checkForTape())
      return False;
  }
  initialize(_scsiDevice);
  return True;
}

Boolean ScsiPlayerApp::findCdMedia()
{
  char* deviceName;

  DBG(fprintf(stderr,"in ScsiPlayerApp::findCdMedia \n"));

  // The user must explicitely specify /CDROM2
  if (!_device || (strlen(_device) == 0)) {
    deviceName = "/CDROM"; 
  }
  else {
    deviceName = _device;
  }

  FILE* f = setmntent("/etc/mtab", "r");
  struct mntent* p;
  
  while (p = getmntent(f)) {
    DBG(fprintf(stderr,"fsname = %s; dir = %s; type = %s\n",
		p->mnt_fsname, p->mnt_dir, p->mnt_type));
    if ((strcmp(deviceName, p->mnt_dir) == 0) //  something mounted on /CDROM
	|| (strcmp(deviceName, p->mnt_fsname) == 0)) // /dev/scsi/sc<something>
      if (strcmp(p->mnt_type, "cdda") == 0) { // it's an audio cdda
      _scsiDeviceName = strdup(p->mnt_fsname);
      _cdMountDirectory = strdup(p->mnt_dir);
      DBG(fprintf(stderr,"FindCdDevice: Found %s.\n", deviceName));
      _audioMediaLoaded = True;
      _flagNotifierPopup = True;

      if (theWarningDialog->baseWidget() && XtIsManaged(theWarningDialog->baseWidget()))
	theWarningDialog->unpost();	// in case something was posted.
      _timeoutInterval = PLAYING_TIMEOUT;
      endmntent(f);
      return True;
      }
      else {			// it's a data cd
	_scsiDeviceName = strdup(p->mnt_fsname);
	_cdMountDirectory = strdup(p->mnt_dir);
	_dataMediaLoaded = True;
	_flagNotifierPopup = True;
	if (theWarningDialog->baseWidget() && XtIsManaged(theWarningDialog->baseWidget()))
	  theWarningDialog->unpost();	// in case something was posted.
	break;
      }
    }

  _audioMediaLoaded = False;
  _timeoutInterval = STOPPED_TIMEOUT;
  endmntent(f);
  return False;

}

void ScsiPlayerApp::eject()
{
  if (player()) {
    player()->eject();
  }
  
  if (theWarningDialog->baseWidget() && XtIsManaged(theWarningDialog->baseWidget()))
    theWarningDialog->unpost();	// in case something was posted.

  _audioMediaLoaded = False;
  _dataMediaLoaded = False;

  switch(fork()) {
  case 0:
    // We are the child.
    DBG(fprintf(stderr,"ejecting %s\n",
	    theScsiPlayerApp->_scsiDeviceName));
    if (execl("/usr/sbin/eject", "eject", 
	      theScsiPlayerApp->_scsiDeviceName,
	      NULL) == -1)
      perror("Eject failed:");
    break;
  case -1:
    // We are the parent. Unsuccessful fork
    theErrorDialog->postBlocked("Attempt to fork failed");
  default:
    // We are the parent. Successful fork.
    break;
  }
  // This block is for making sure the disk is actually gone.
  // Probably the opposite of what you would expect here.
  while (True) {
    theScsiPlayerApp->findCdMedia();
    if (_audioMediaLoaded ||  _dataMediaLoaded) {
      DBG(fprintf(stderr,"ScsiPlayerApp::eject making sure the CD cleared\n"));
      sginap(100);
      _audioMediaLoaded = False;
      _dataMediaLoaded = False;
      _flagNotifierPopup = True;
    }
    else
      break;
  }
  theScsiPlayerApp->startDisplayUpdate();
}


void ScsiPlayerApp::initialize(ScsiDevice device)
{
  if (!_player[device]) {

    switch (device) {
    case CdDev:
      // for CDs the app's transport struct and the device's transport
      // struct are one in the same.

      // We need to figure out the scsi device name and squirrel it away so 
      // we can use the eject button to open the tray when it is empty.


      if (_audioMediaLoaded || findCdMedia()) {
	setreuid(-1, 0);	// get privileges
	_transport[device] = CDopen(_scsiDeviceName, "r", NULL);
	setreuid(-1, getuid());	// drop privileges
	DBG(fprintf(stderr,"getuid() = %d, geteuid() = %d\n", getuid(), geteuid()));
	if (!_transport[device]) {
	  sprintf(_strBuf, "CDopen():  %s",
		  strerror(oserror()));  
	  theErrorDialog->postAndWait(_strBuf);
	  exit(1);
	}
	CDstop(_transport[device]); // in case cdheadphones was running.
	memset(transport()->trackInfo, 0,
	       999 * sizeof(transport()->trackInfo[0]));
	DBG(fprintf(stderr,"_scsiDeviceName = %s\n", _scsiDeviceName));
	if (_transport[device]) 
	  _database[device] = new Database(device);
	_player[device] = new CdPlayer(_transport[device]);
	_flagDisplayForNewMedia = True;
      }
      break;

    case DatDev:
      _transport[device] = (ScsiPlayer*)malloc(sizeof(ScsiPlayer));
      if (_transport[device]) 
	_database[device] = new Database(device);
      _player[device] = new DatPlayer(_transport[device]);
      DatPlayer* player = (DatPlayer*)_player[device];

      if (player->_mediaType == DATmediaAudio)
	_flagDisplayForNewMedia = True;
      memset(transport()->trackInfo, 0,
	     999 * sizeof(transport()->trackInfo[0]));
      break;
    default:
      theErrorDialog->postAndWait("Bogus Device, exiting...\n");
      exit(1);
      break;
    }
    
  }

  // Stuff we do everytime we re-initialize.

  if (_transport[device]) {
    _transport[device]->track = 0;
    if (_audioMediaLoaded) {
      if (theWarningDialog->baseWidget() && XtIsManaged(theWarningDialog->baseWidget()))
	theWarningDialog->unpost();	// in case something was posted.
      player()->stop();
      if (device == CdDev)
	player()->seekTrack(1);
    }  
    else
      checkForMedia();
  }
  startDisplayUpdate();

}

void ScsiPlayerApp::startDisplayUpdate(Boolean removeOldTimeout)
{
  DBG(fprintf(stderr,"Entering ScsiPlayerApp::startDisplayUpdate\n"));
  if (removeOldTimeout)
    stopDisplayUpdate();
  if (player()) {
    switch (player()->_playState) {
    case Playing:
      _timeoutInterval = PLAYING_TIMEOUT;
      break;
    case FastFwding:
    case Rewinding:
    case Seeking:
      _timeoutInterval = WINDING_TIMEOUT;
      break;
    case Stopped:
    default:
      _timeoutInterval = STOPPED_TIMEOUT;
    }
  }
  // DBG(fprintf(stderr,"startDisplayUpdate at %dms\n",(int)_timeoutInterval));
  _timeoutId = XtAppAddTimeOut(appContext(), _timeoutInterval,
				 ScsiPlayerApp::stateChangeCallback, this);
}

void ScsiPlayerApp::stopDisplayUpdate()
{
  if (_timeoutId) {
    DBG(fprintf(stderr,"stopDisplayUpdate\n"));
    XtRemoveTimeOut(_timeoutId);
  }
}

void ScsiPlayerApp::clearCdPlayer() 
{
  if (transport()->key) {
    memset(transport()->key, 0, MAX_KEY_SIZE); // void the key
  }
  delete _player[CdDev];
  _player[CdDev] = NULL;

  CDclose(_transport[CdDev]);
  delete _transport[CdDev];
  _transport[CdDev] = NULL;

  database()->terminate();
  _database[CdDev] = NULL;
}

ScsiPlayerApp::~ScsiPlayerApp()
{
  stopDisplayUpdate();
  theScsiPlayerApp = NULL;
  if (_player[CdDev]) {
    clearCdPlayer();
  }
  if (_player[DatDev]) {
    delete _player[DatDev];
    delete _transport[DatDev];
  }
  delete _programName;
  delete _displayMutex;
} 

void ScsiPlayerApp::run()
{
  // call next method.
  VkApp::run();
}


void ScsiPlayerApp::stateChangeCallback(XtPointer clientData, XtIntervalId*)
{
  ScsiPlayerApp* fakeThis = (ScsiPlayerApp*) clientData;
  DBG(fprintf(stderr,"  ScsiPlayerApp::stateChangeCallback\n"));
  fakeThis->stateChange();
  fakeThis->startDisplayUpdate();
}

void ScsiPlayerApp::stateChange()
{
  DBG(fprintf(stderr,"ScsiPlayerApp::stateChange; Interval = %d ...", _timeoutInterval));
  _displayMutex->usp();
  if (_audioMediaLoaded) {
    MainWindow* mw = (MainWindow*)mainWindow();
    mw->stateChange(transport());
  }
  else {
    DBG(fprintf(stderr,"Sniffing for Media...\n"));
    checkForMedia();
  }
  if (theScsiPlayerApp->_dataMediaLoaded
      && (!theWarningDialog->baseWidget() || !XtIsManaged(theWarningDialog->baseWidget()))
      && _flagNotifierPopup) {
    theWarningDialog->post(VkGetResource("*ScsiPlayer*dataMediaMsg", XmCString));
    _flagNotifierPopup = False;
  }
  else if (!_audioMediaLoaded
	   && (!theWarningDialog->baseWidget() || !XtIsManaged(theWarningDialog->baseWidget()))
	   && _flagNotifierPopup)
  {
    theWarningDialog->post(VkGetResource("*ScsiPlayer*noMediaMsg", XmCString));
    _flagNotifierPopup = False;
  }
  if (player())
    player()->stateChange();	// player specific tasks.
  _displayMutex->usv();
  DBG(fprintf(stderr,"done\n", _timeoutInterval));

}

void ScsiPlayerApp::quitYourself()
{
  sigignore(SIGCLD);	// unregister the signal handler
  if (player()) {
    player()->killThread();
    player()->closeDevice();
  }
  VkApp::quitYourself();
}


void ScsiPlayerApp::initCutPaste() 
{

  if(_xaSCSIPLAYER_PRIVATE == None) {
    
    Display* dpy = display();
	
    // must initialize X Copy/Paste support	   
    _xaCLIPBOARD = _cutPaste->clipboardAtom();
    _xaSCSIPLAYER_PRIVATE = 
      XmInternAtom(dpy, "_SGI_SCSIPLAYER_PRIVATE_", False);
    _xaSGI_AUDIO_FILE = 
      XmInternAtom(dpy, "SGI_AUDIO_FILE", False);
    _xa_SGI_AUDIO_FILENAME = 
      XmInternAtom(dpy, "_SGI_AUDIO_FILENAME", False);
    _cutPaste->registerDataType(_xaSCSIPLAYER_PRIVATE, 
				_xaSCSIPLAYER_PRIVATE,
				8);	    
    _cutPaste->registerConverter(_xaSCSIPLAYER_PRIVATE, 
				 _xaSGI_AUDIO_FILE, 
				 convertCB, 
				 NULL, 
				 this);
    _cutPaste->registerConverter(_xaSCSIPLAYER_PRIVATE,
				 _xa_SGI_AUDIO_FILENAME, 
				 convertCB, 
				 NULL,
				 this);    
  }
}

void ScsiPlayerApp::xcopy() 
{
  _cutPaste->clear(_xaCLIPBOARD);
    
  // create a token to use as private target data
  char text[256];
  sprintf( text, "ScsiPlayerPrivate" );
  _cutPaste->putCopy(_xaCLIPBOARD, _xaSCSIPLAYER_PRIVATE, text, strlen(text) + 1);		     
  _cutPaste->export(_xaCLIPBOARD);	           
}


Boolean ScsiPlayerApp::convertCB(Widget , 
				 Atom selection,
				 void* ,
				 Atom srcTarget,
				 XtPointer src,
				 unsigned long numSrcBytes,
				 Atom dstTarget,
				 XtPointer *dst,
				 unsigned long *numDstBytes)
{
  //EditAction *action = (EditAction*)clientData;

  return theScsiPlayerApp->convert(selection, 
				   srcTarget, src, numSrcBytes, 
				   dstTarget, dst, numDstBytes);
}

Boolean ScsiPlayerApp::convert(Atom selection, 
			       Atom srcTarget, 
			       XtPointer, 
			       unsigned long, 
			       Atom dstTarget, 
			       XtPointer *dst, 
			       unsigned long *numDstBytes)
{
  if(selection == _xaCLIPBOARD && 
     srcTarget == _xaSCSIPLAYER_PRIVATE) {
    
    if(dstTarget == _xaSGI_AUDIO_FILE ||
       dstTarget == _xa_SGI_AUDIO_FILENAME) {
		
      // Create the progress dialog
      setBusyDialog(theProgressDialog);
      busy("*ScsiPlayer*exportingDataMsg", NULL);
	    		
      char* filename = 
	player()->exportClipBoard(); 

      // Cleanup the progress dialog
      notBusy();
      setBusyDialog(NULL);
					   
      if(filename != NULL) {
	*dst = filename;
	*numDstBytes = strlen(filename) + 1;
	return True;		
      }
    }
  }    			     
  return False;				 
}


void ScsiPlayerApp::boostPriority()
{
  setreuid(-1, 0);	// get privileges

  /*
   * Do some rooteous work:
   *	    make this process memory-resident
   *	    give this process a high,  non-degrading priority.
   * XXX we don't check to see if these calls fail; however,  we assume
   *     that if they do,  this is because the user deliberately did not
   *     run the tools setuid root.
   */
  prctl(PR_RESIDENT);
  if (schedctl(NDPRI, 0, NDPHIMIN) < 0) {
    sprintf(_strBuf, "%s\n%s",
	    VkGetResource("*ScsiPlayer*priorityBoostFailedMsg", XmCString),
	    strerror(oserror()));  
    theWarningDialog->postAndWait(_strBuf);
  }

  DBG(fprintf(stderr,"boosted priority.\n"));

  setreuid(-1, getuid());	// drop privileges
}


void ScsiPlayerApp::updateViewSetting()
{
  XrmPutStringResource(&_settingsDb, "DisplayTrackList",
		       (_displayTrackList ? "True" : "False"));
  XrmPutFileDatabase(_settingsDb, _settingsPathname);
}

void ScsiPlayerApp::updatePlaySelectionSetting()
{
  XrmPutStringResource(&_settingsDb, "PlaySelection",
		       (_playSelection ? "True" : "False"));
  XrmPutFileDatabase(_settingsDb, _settingsPathname);
}

void ScsiPlayerApp::updateGainSetting()
{
  char gainStr[50];
  sprintf(gainStr, "%f", player()->_audioBuffer->getGain());
  XrmPutStringResource(&_settingsDb, "Gain", gainStr);
  XrmPutFileDatabase(_settingsDb, _settingsPathname);
}

