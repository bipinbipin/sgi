#ifndef _ScsiPlayerApp_H
#define _ScsiPlayerApp_H

#include <sys/stat.h>
#include <sys/param.h>		// for MAXPATHLEN
#include <VkApp.h>
#include <Vk/VkGenericDialog.h>
#include <Vk/VkCutPaste.h>

#include "PlayerState.h"
#include "Database.h"
#include "Player.h"
#include "Semaphore.h"

#define __sgi

class ScsiPlayerApp;
extern ScsiPlayerApp*	theScsiPlayerApp;	// global pointer to the app

enum RepeatMode {
    NoRepeat,
    RepeatAlways,
    RepeatTrack
};

#define STOPPED_TIMEOUT 1000
#define PLAYING_TIMEOUT 100
#define WINDING_TIMEOUT 20

class ScsiPlayerApp : public VkApp {

public:
  ScsiPlayerApp(char*, int*, char**,
	  XrmOptionDescRec* optionList = NULL, int sizeOfOptions = 0);
  ~ScsiPlayerApp();

  ScsiDevice scsiDevice() { return _scsiDevice; };
  void setScsiDevice(ScsiDevice device) { _scsiDevice = device; };

  void startDisplayUpdate(Boolean removeOldTimeout = True);
  void stopDisplayUpdate();
  void boostPriority();
  void updateViewSetting();
  void updatePlaySelectionSetting();
  void updateGainSetting();

  Player* player() { return _player[_scsiDevice]; };

  ScsiPlayer* transport() { return _transport[_scsiDevice]; };

  pid_t serverId() { return _childPid[_scsiDevice]; };
  Database* database() { return _database[_scsiDevice]; };
  Boolean findCdMedia();
  Boolean checkForTape();
  Boolean checkForMedia();
  void eject();

  void clearCdPlayer(); 
  void initCutPaste();
  void xcopy();

  int _recordReferenceCount;	// is there anything to record?
  DMboolean _recordingEnabled; // the "Rec" button has been pushed.
  int _trackCache;		// this is how the client knows the track changed.
  Widget _selectedButton;
  RepeatMode _repeatMode;
  Boolean _displayTrackList;
  Boolean _loopSelection;
  Boolean _playSelection;
  XrmDatabase _trackInfoDb;
  XrmDatabase _settingsDb;
  ScsiPlayer* _transport[NUM_SCSI_AUDIO_DEVICES];

  Boolean _audioMediaLoaded;
  Boolean _dataMediaLoaded;
  Boolean _flagDisplayForNewMedia;
  Boolean _flagNotifierPopup;
  char* _scsiDeviceName;
  char* _cdMountDirectory;
  char* _homeDirectory;		// This gets used a bunch and needs to be copied.
  char _settingsPathname[MAXPATHLEN];
  unsigned long _timeoutInterval;
  Semaphore* _displayMutex;
  int _highestDisplayTrack;	// the display's highwater mark (for DAT)
  float _initialGain;

  virtual void initialize(ScsiDevice);
  virtual void run();
  virtual void quitYourself();
  virtual void stateChange();
  static void exitCallback(XtPointer, XtIntervalId*);

protected:

  // callback which listens for changes to the controller state.
  static void stateChangeCallback(XtPointer, XtIntervalId*);

  // x CnP protocol
  static Boolean convertCB(Widget w, 
			   Atom selection,
			   void *clientData,
			   Atom srcTarget,
			   XtPointer src,
			   unsigned long numSrcBytes,
			   Atom dstTarget,
			   XtPointer *dst,
			   unsigned long *numDstBytes);

  Boolean convert(Atom selection, 
		  Atom srcTarget, 
		  XtPointer src, 
		  unsigned long numSrcBytes, 
		  Atom dstTarget, 
		  XtPointer *dst, 
		  unsigned long *numDstBytes);

  // data members initialized by looking up the X resource database.  
  // Inherited from CDMAN

  char* _device;
  Boolean _autoFork;
  Boolean _smallDisplay;
  Boolean _dsdebug;

  char* _programName;
  char _strBuf[512];

  // the app is the communication center for the client side.
  ScsiDevice _scsiDevice;
  Player* _player[NUM_SCSI_AUDIO_DEVICES];
  Database* _database[NUM_SCSI_AUDIO_DEVICES];
  VkCutPaste* _cutPaste;	// ViewKit support for X Cut/Copy/Paste
  
  static XtResource resources[];

  XtIntervalId _timeoutId;
  pid_t _childPid[NUM_SCSI_AUDIO_DEVICES];

};
#endif


