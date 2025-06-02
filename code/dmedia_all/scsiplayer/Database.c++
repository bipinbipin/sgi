
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <X11/Intrinsic.h>
#include <X11/Xresource.h>

#include <Vk/VkResource.h>

#include <dmedia/dm_timecode.h>

#include "PlayerState.h"
#include "Database.h"
#include "ScsiPlayerApp.h"

//#define DBG(x) {x;}
#define DBG(x) {}  

Database* theDatabase = NULL;

Database::Database(ScsiDevice device)
{
  char* tmp;
  // figure out the path to look for the database files.
  if (device == CdDev) {
    tmp = getenv("CDDB_PATH");
    if (!tmp) {
      int len = strlen(getenv("HOME")) + strlen("/.cddb") + 1;
      tmp = (char*) malloc(len);
      sprintf(tmp, "%s%s", getenv("HOME"), "/.cddb");
    }
  } else {			// must be a DAT
    tmp = getenv("DATDB_PATH");
    if (!tmp) {
      int len = strlen(getenv("HOME")) + strlen("/.datdb") + 1;
      tmp = (char*) malloc(len);
      sprintf(tmp, "%s%s", getenv("HOME"), "/.datdb");
    }
  }
  _xdb = NULL;
  // prep path to contain a single string, with elements separated by NULLs.
  // compute the number of elements in the _path, _pathCount.
  if (tmp) {
    _pathCount = 1;
    _path = strtok(strdup(tmp), ",:");
    while(strtok(NULL, ",:")) {
      _pathCount++;
    }
    struct stat statBuf;
    if (stat(_path, &statBuf)
	&& (errno == ENOENT))
      mkdir(_path, 00777);
  }
  else {
    _pathCount = 0;
    _path = NULL;
  }

  _filename = NULL;
  if (theDatabase) {
    DBG(fprintf(stderr, "Internal Error. Multiple instances of Database\n"));
  }
  else 
    theDatabase = this;
}

Database::~Database()
{
    DBG(fprintf(stderr,"Deleting Database...\n"));
    if (_path) {
	delete _path;
    }
    terminate();
}

DMboolean Database::initialize(const char* key)
{
    // locate the file that corresponds to this cd.
    //
    char* tmp = _path;
    char buffer[MAXPATHLEN];
    struct stat buf;

    //printf("Input key = %s\n", key); 
    // each element in the _path
    for (int i = 0; i < _pathCount; i++) {
	// construct filename
	sprintf(buffer,"%s/%s.%s", tmp, key, DB_RDB_EXT);
	// see if the file exists
	//printf("buffer = %s...\n", buffer);
	if ((stat(buffer, &buf) == 0) && S_ISREG(buf.st_mode)) {
	    // the file exists, hurrah!
	    _filename = strdup(buffer); 
	    _xdb = XrmGetFileDatabase(_filename);
	    return DM_TRUE;
	}
	tmp = &tmp[strlen(tmp)+1];
    }

    // save the file name in case we want save something later.
    _filename = strdup(buffer); 
    return DM_FALSE;
}

DMboolean Database::isInitialized()
{
  if ((_xdb == NULL) || (_filename == NULL))
    return DM_FALSE;
  else
    return DM_TRUE;
}
	

void Database::terminate()
{
  if (_xdb)
    XrmDestroyDatabase(_xdb);
    
  delete _filename;
  _filename = NULL;
  _xdb = NULL;
  theDatabase = NULL;
}


char* Database::extract(char* resourceName, char* resourceClass,
			char* defaultString)
{
    XrmValue value;
    char* type;

    if (XrmGetResource(_xdb, resourceName, resourceClass, &type, &value)) {
	return value.addr;
    }
    else {
	return defaultString;
    }
}


// "getters"

char* Database::getTitle()
{
    if (!_xdb) {
	// either initialize was not called, or it failed.
	return VkGetResource("ScsiPlayer*unknownTitleString", XmCString);
    }
    return extract("album.title", "Album.Title",
		   VkGetResource("ScsiPlayer*unknownTitleString", XmCString));
}

char* Database::getArtistName()
{
    if (!_xdb) {
	// either initialize was not called, or it failed.
	return VkGetResource("ScsiPlayer*unknownArtistString", XmCString);
    }
    return extract("album.artist", "Album.Artist",
		   VkGetResource("ScsiPlayer*unknownArtistString", XmCString));
}

char* Database::getAlbumDuration()
{
    if (!_xdb) {
	// either initialize was not called, or it failed.
	return  VkGetResource("ScsiPlayer*unknownDurationString", XmCString);
    }
    return extract("album.duration", "Album.Duration",
		   VkGetResource("ScsiPlayer*unknownDurationString", XmCString));
}

char* Database::getAlbumTracks()
{
    if (!_xdb) {
	// either initialize was not called, or it failed.
	return VkGetResource("ScsiPlayer*unknownTracksString", XmCString);
    }
    return extract("album.tracks", "Album.Tracks",
		   VkGetResource("ScsiPlayer*unknownTracksString", XmCString));
}

char* Database::getTrackTitle(int num)
{
    if (!_xdb) {
	// either initialize was not called, or it failed.
	return VkGetResource("ScsiPlayer*unknownTrackNameString", XmCString);
    }
    char strName[MAXNAME];
    char strClass[MAXCLASS];
    sprintf(strName, "%s%d.%s", "track", num, "title");
    sprintf(strClass, "%s%d.%s", "Track", num, "Title");
    return extract(strName, strClass,
		   VkGetResource("ScsiPlayer*unknownTrackNameString", XmCString));
}

char* Database::getTrackStart(int num)
{
    if (!_xdb) {
	// either initialize was not called, or it failed.
	return VkGetResource("ScsiPlayer*unknownTrackStartString", XmCString);
    }
    char strName[MAXNAME];
    char strClass[MAXCLASS];
    sprintf(strName, "%s%d.%s", "track", num, "start");
    sprintf(strClass, "%s%d.%s", "Track", num, "Start");
    return extract(strName, strClass,
		   VkGetResource("ScsiPlayer*unknownTrackStartString", XmCString));
}


char* Database::getTrackDuration(int num)
{
    if (!_xdb) {
	// either initialize was not called, or it failed.
	return VkGetResource("ScsiPlayer*unknownTrackDurationString", XmCString);
    }
    char strName[MAXNAME];
    char strClass[MAXCLASS];
    sprintf(strName, "%s%d.%s", "track", num, "duration");
    sprintf(strClass, "%s%d.%s", "Track", num, "Duration");
    return extract(strName, strClass,
		   VkGetResource("ScsiPlayer*unknownTrackDurationString", XmCString));
}

// "setters"

void Database::setTitle(char* title)
{
    if (!_filename)
      return;
    XrmPutStringResource(&_xdb, "album.title", title);
    save();
}

void Database::setArtistName(char* artist)
{
    if (!_filename)
      return;
    XrmPutStringResource(&_xdb, "album.artist", artist);
    save();
}

void Database::setAlbumTracks(int totalTracks)
{
    if (!_filename)
      return;
    char tmpString[4];
    sprintf(tmpString, "%d",totalTracks);
    XrmPutStringResource(&_xdb, "album.tracks", tmpString);
    save();
}

void Database::setTrackTitle(int trackNumber, char* trackName)
{
  if (!_filename)
    return;
  char tmpString[14];
  sprintf(tmpString, "track%d.title", trackNumber);
  XrmPutStringResource(&_xdb, tmpString, trackName);
  save();
}

void Database::setTrackStart(int trackNum, unsigned long frames)
{
  DMtimecode* tmpTc = (DMtimecode*)malloc(sizeof(DMtimecode));
  theScsiPlayerApp->player()->framesToTc(frames, tmpTc);
  setTrackStart(trackNum, tmpTc);
  save();
}

void Database::setTrackStart(int trackNum, DMtimecode* tc)
{
  if (!_filename)
    return;

  char titleString[14];
  char tcString[13];
  sprintf(titleString, "track%d.start",trackNum);
  sprintf(tcString, "%02d:%02d:%02d", tc->minute, tc->second, tc->frame);
  XrmPutStringResource(&_xdb, titleString, tcString);
  save();
}

void Database::setTrackDuration(int trackNum, unsigned long frames)
{
  DMtimecode* tmpTc = (DMtimecode*)malloc(sizeof(DMtimecode));
  theScsiPlayerApp->player()->framesToTc(frames, tmpTc);
  setTrackDuration(trackNum, tmpTc);
}

void Database::setTrackDuration(int trackNum, DMtimecode* tc)
{
  if (!_filename)
    return;

  char titleString[17];
  char tcString[13];
  sprintf(titleString, "track%d.duration",trackNum);
  sprintf(tcString, "%02d:%02d:%02d", tc->minute, tc->second, tc->frame);
  XrmPutStringResource(&_xdb, titleString, tcString);
  save();
}

void Database::save()
{
  if (_xdb && _filename)
    XrmPutFileDatabase(_xdb, _filename);
}



