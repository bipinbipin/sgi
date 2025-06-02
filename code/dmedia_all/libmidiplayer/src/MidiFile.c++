/************************************************************************\
 *	File:		MidiFile.c++					*
 *	Author:		John Kraft (jfk@sgi.com)			*
 *									*
 *   Copyright (c) 1995-96 Silicon Graphics Inc.  All rights reserved.  *
\************************************************************************/

// Standard Include Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <values.h>
#include <assert.h>
#include <bstring.h>
#include <fcntl.h>
#include <sys/file.h>

// Digital Media Include Files
#include <dmedia/midi.h>

#include "MPfile.h"

#define dprintf 0 &&

// These are to find the message lengths of Channel Voice and Mode Messages
int __msglengths[] = { 3, 3, 3, 3, 2, 2, 3 };
#define getMsgLen(x) ((__msglengths[(x & 0x7f) >> 4]))


/* Read a single character from the midifile */
inline int MidiFile::_readchar() 
{
    int c = getc(_fp);
    _toread--;
    return c;
}


/*
 * Constructor - Initialize the method variables.  Since the constructor
 * can't return any indication of failure, we avoid all but the most
 * innocuous of initialization operations.  Dangerous stuff is done in
 * MidiFile::load().
 */
MidiFile::MidiFile()
{
    _division = 120;
    _format   = 0;
    _ntracks  = 0;
    _tracks   = (MidiTrack **)NULL;
    _toread   = 0;

    _head     = NULL;
    _tail     = NULL;
    _lastEvent= NULL;
    _sequenceName = _fileName = NULL;
}


/*
 * Destructor - Deallocate any dynamically allocated memory.
 */
MidiFile::~MidiFile()
{
    _cleanup();
}



/* Return the midi file format type (either 0, 1, or 2) */
int MidiFile::format()
{
    return(_format);
}


/* Return the number of tracks in the file */
int MidiFile::numberTracks()
{
    return(_ntracks);
}


/* 
 * Return the division used in the file.  The division is the number of
 * ticks in a midi file quarter note.
 */
int MidiFile::division()
{
    return(_division);
}


/*
 * Return the name of the sequence, if we can find one.  If
 * we can't, return the name of the file if we know it.
 * If we don't, return NULL.
 */
const char * MidiFile::sequenceName(void)
{
    if (_sequenceName == NULL) {
	// Search for a name meta event on track 0
	MidiEvent *mev = getTrack(0)->firstEvent();
	while (mev != NULL) {
	    if (mfMetaType(mev) == MIDImeta_Name) {
		int length = mev->event.sysexmsg[2];
		_sequenceName = new char[length + 1];
		strncpy(_sequenceName, &mev->event.sysexmsg[3], length);

		// Make sure it is null-terminated
		_sequenceName[length] = '\0';
	    }
	    
	    mev = mev->nextTrackEvent;
	}

	// If we got a name, see if it is kosher.  A lot of sequences
	// have crap in this field for some reason.
	if (_sequenceName != NULL) {
	    int kosher = 0;
	    for (int i = 0; i < strlen(_sequenceName); i++) {
		if (isalpha(_sequenceName[i])) {
		    kosher = 1;
		    break;
		}
	    }
	    if (!kosher) {
		delete _sequenceName;
		_sequenceName = NULL;
	    }
	}

	if (_sequenceName == NULL && _fileName != NULL) {

	    // Get the basename (chop of any directory path
	    char *basename = strrchr(_fileName, '/');
	    if (basename == NULL)
		basename = _fileName;
	    else 
		basename++;

	    _sequenceName = new char[strlen(basename) + 1];
	    strcpy(_sequenceName, basename);
	}
    }

    return (const char *) _sequenceName;
}

	     
int MidiFile::initialTempo()
{
    MidiTrack *track = getTrack(0);
    if (track == NULL) return -1;

    for (MidiEvent *ev = track->firstEvent(); ev != NULL;
	 ev = ev->nextTrackEvent) {
	if (mfIsTempoEvent(ev))
	    break;
    }

    if (ev == NULL) 
	return 500000;
    else
	return(mfGetTempo(ev));
}


/*
 * Set the MIDI file format.  Note that certain formats have certain
 * connotations (for example, type 1 files assume that all tempo events
 * are stored in track 0).  This routine makes no attempt to enforce these
 * conventions.
 */
void MidiFile::setFormat(int i)
{
    _format = i;
}


/* Set the number of ticks in a midi file quarter note */
void MidiFile::setDivision(int i)
{
    _division = i;
}


/* 
 * Return a pointer to the requested track number.
 */
MidiTrack *MidiFile::getTrack(int n)
{
    if (n >= _ntracks) {
	return NULL;
    } else
	return _tracks[n];
}


/*
 * Add the given track to the midifile.  We don't check to see whether
 * the track is already in the file, and it is a bad idea to add a 
 * track multiple times. 
 */
void MidiFile::addTrack(MidiTrack *track)
{
    if (track != NULL) {
	_tracks = (MidiTrack **)realloc(_tracks, (_ntracks + 1) * 
					sizeof(MidiTrack *));
	if (_tracks == NULL) {
	    _ntracks = 0;
	    return;
	}

	_tracks[_ntracks] = track;
	_ntracks++;
    }
}


/* 
 * Remove the track at the given index from the MFfile.  
 */
void MidiFile::deleteTrack(int n)
{
    if (n < _ntracks) {

	// Release the storage held by the old track
	delete _tracks[n];

	// Make the track array contiguous
	for (int i = n; i < _ntracks - 1; i++) 
	    _tracks[i] = _tracks[i+1];
	    
	_ntracks--;
    }
}


/////////////////////////////////////////////////////////////////////////////
//                                                                         //
// File I/O Methods                                                        //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////

int MidiFile::load(char *name) 
{
    int fd;
    if ((fd = open(name, O_RDONLY)) < 0) 
	return -1;

    _fileName = new char[strlen(name) + 1];
    strcpy(_fileName, name);

    int ret = this->load(fd);
    return ret;
}


int MidiFile::load(int fd)
{
    // Cleanup from any previously open file
    _cleanup();

    // Open the file descriptor for use by stdio
    if ((_fp = fdopen(fd, "r")) == NULL) {
	return -1;
    }
    fseek(_fp, 0, SEEK_SET);

    int tot = 0;
    int tracksread = _readfilehdr();
    MidiTrack *track;

    if ( tracksread < 0 ) {
        fclose( _fp );
        return -1;
    }

    // XXX This really should handle chunks, as per the MIDI file spec. 
    // Right now, we expect the given number of tracks.

    for (int i = 0; i < tracksread; i++) {
        int nRead;
	track = new MidiTrack();
        nRead = _readtrack( track );
        if ( nRead < 0 ) {
            fclose( _fp );
            return -1;
        }
        tot += nRead;
	this->addTrack(track);
    }

    fclose(_fp);
    return(0);
}


int MidiFile::save(char *name)
{
    if ((_fp = fopen(name, "w")) == NULL) {
	return(-1);
    }

    _writefilehdr();

    for (int i = 0; i < _ntracks; i++) 
	_writetrack(i);

    fclose(_fp);
    return(0);
}


//////////////////////////////////////////////////////////////////////
// Playback support routines
//////////////////////////////////////////////////////////////////////

MidiEvent *
MidiFile::firstEvent(void)
{
    return _head;
}


MidiEvent *
MidiFile::lastEvent(void)
{
    return _tail;
}


MidiEvent *
MidiFile::seekTick(long long tick)
{
    MidiEvent *ev = _head;

    while (ev && ev->event.stamp < tick)
	ev = ev->nextEvent;

    return ev;
}



void MidiFile::print(FILE *fp)
{
    fprintf(stderr, "MFfile ->\n");
    fprintf(stderr, "\tno of tracks = %d\n", _ntracks);
    fprintf(stderr, "\tformat       = %d\n", _format);
    fprintf(stderr, "\tdivision     = %d\n", _division); 
    fprintf(stderr, "\t------------------------------------------\n");

    for (int i = 0; i < _ntracks; i++) {
	fprintf(stderr, "\tTrack #%d ->\n", i + 1);
	_tracks[i]->print(fp);
	fprintf(stderr, "\t<- End Track #%d\n", i + 1);
    }
    fprintf(stderr, "<- End MFfile\n");
}


/*
 * Insert an event at the proper location in the stream.
 * To speed this process up, we keep around a hint which
 * keeps track of where we last inserted an event.  The
 * rationale is that during reads we do lots of event insertions
 * in time-ascending order, so we can reduce the amount of searching
 * by not restarting the search from the beginning of the list all
 * the time.
 */
void
MidiFile::insertEvent(MidiEvent *ev)
{
    MidiEvent *currev;

    if (_tail == NULL || ev->event.stamp >= _tail->event.stamp) {
	this->_append(ev);
    } else {
	// We need to insert it in the list
	if (_lastEvent && _lastEvent->event.stamp <= ev->event.stamp) 
	    currev = _lastEvent->nextEvent;
	else
	    currev = _head;
	
	for ( ; currev != NULL; currev = currev->nextEvent) {
	    if (ev->event.stamp < currev->event.stamp) {
		_insertBefore(currev, ev);
		break;
	    }
	}
    }

    _lastEvent = ev;
}


/************************************************************************
 *	Private methods							*
 ************************************************************************/

/*
 * Deallocate all structures associated with an open midi file.
 * We do this as a separate routine since we want to be able to
 * clean up if load again after a file has already been loaded.
 */
void
MidiFile::_cleanup(void)
{
    // If nothing was ever allocated, don't deallocate anything
    if (_ntracks == 0)
	return;

    // Get rid of the old tracks
    for (int i = 0; i < _ntracks; i++) 
	delete _tracks[i];

    // Clean up the storage arrays
    free(_tracks);

    if (_sequenceName)
	delete _sequenceName;

    if (_fileName)
    	delete _fileName;

    _ntracks = 0;
}

	

unsigned long
MidiFile::_writetrackhdr()
{
    unsigned long offset;

    _writehd("MTrk");
    offset = ftell(_fp);
    _write32(0);                        // length of header data -- dummy

    return(offset);
}


/* write out the midi file header */
void
MidiFile::_writefilehdr()
{
    _writehd("MThd");
    _write32(6);                       // length of header data
    _write16(_format);                 // type of MIDI file
    _write16(_ntracks);                // number of tracks
    _write16(_division);               // division
}


/* Write out a specific string */
int
MidiFile::_writehd(char *looking)
{
    return(fprintf(_fp, "%s", looking));
}


/* Write a value in the variable length number format */
int
MidiFile::_writevar(int n)
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
	putc(buffer & 0xff, _fp);
	if (buffer & 0x80) {
	    buffer >>= 8;
	}
	else {
	    break;
	}
    }
    return(sum);

}


/* write a time out */
int
MidiFile::_writetime(int t)
{
    return _writevar(t);
}


/* Write a 32-bit value out */
void
MidiFile::_write32(int n)
{
    int mask = 0xff000000;
    putc((n & mask) >> 24, _fp); 	mask >>= 8;
    putc((n & mask) >> 16, _fp);	mask >>= 8;
    putc((n & mask) >> 8,  _fp); 	mask >>= 8;
    putc((n & mask),       _fp);
}


/* Write a 16-bit value */
void
MidiFile::_write16(int n)
{
    int mask = 0xff00;
    putc((n & mask) >> 8, _fp); 	mask >>= 8;
    putc((n & mask), _fp);
}


/* Write out a meta event */
int
MidiFile::_writemetaevent(MidiEvent *ev, unsigned long long *time)
{
    assert(mfIsValidMeta(ev));

    MDevent *me = &ev->event;

    int sum = _writetime(me->stamp - *time) + 2;
    *time = me->stamp;

    putc(0xff, _fp);
    putc(me->sysexmsg[1], _fp);
    sum += _writevar(me->sysexmsg[2]);
    for (int i = 3; i < me->msglen; i++) 
	putc(me->sysexmsg[i], _fp);

    return(sum + me->sysexmsg[2]);
}


/*
 * Write out an entire track
 */

int
MidiFile::_writetrack(int trackno)
{
    int sum = 0;

    // Get the track
    MidiTrack *track = _tracks[trackno];

    // Write the header for the track
    unsigned long offset = _writetrackhdr();

    // Write all of the events
    unsigned long long time = 0;
    for (MidiEvent *ev = track->firstEvent(); ev != NULL; 
	 ev = ev->nextTrackEvent) {
	sum += _writeevent(ev, &time);
    }

    // Write the length of the track after the header information
    long here = ftell(_fp);
    fseek(_fp, offset, 0);
    _write32(sum);
    fseek(_fp, here, 0);

    return(sum);
}


int
MidiFile::_writeevent(MidiEvent *fev, unsigned long long *time)
{
    if (mfIsMidiEvent(fev))
	return _writemidievent(fev, time);
    else 
	return _writemetaevent(fev, time);
}


int
MidiFile::_writemidievent(MidiEvent *mev, unsigned long long *time)
{
    char *m;

    MDevent *ev = &mev->event;

    int sum = _writetime(ev->stamp - *time);

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
  
    for (int i = 0; i < length; i++) 
	putc(m[i], _fp);

    return(sum + length);
}


///////////////////////////////////////////////////////////////////////////
//                                                                       //
// Internal Reading Utility Methods                                      //
//                                                                       //
///////////////////////////////////////////////////////////////////////////

MidiFile::_readtrack(MidiTrack *track)
{
    int status, running = 0;
    unsigned long long time = 0;
  
    if (track == NULL) {
	return(-1);
    }
  
    if (MidiFile::_readtrackhdr() < 0) {
	dprintf(0, "ERROR: did not find MTrk token\n");
	return -1;
    }

    dprintf(0, "  Read track header, %d events\n", _toread);

    while (_toread > 0) {
	time += _readvar();
	char c = _readchar();
	status = c & 0xff;

	if (status < MD_SYSEX) {
	    MidiEvent *ev = MidiFile::_readmidievent(status, &running, time);
	    track->insertEvent(ev);
	    this->insertEvent(ev);
	} 
	else {		// not a channel message
	    switch(status) {
	      case MD_SYSEX:
	      case MD_EOX:
		{
		MidiEvent *ev = MidiFile::_readsysexevent(time);
		dprintf(0, "    sysex event read.\n");
		track->insertEvent(ev);
		this->insertEvent(ev);
		}
		break;

	      case MD_META:
		{
		MidiEvent *mev = MidiFile::_readmetaevent(time);
		track->insertEvent(mev);
		this->insertEvent(mev);
		dprintf(0, "    meta event read\n");
		}
		break;
	    }
	}
    }
    return track->numberEvents();
}


/* Read the track header from a midi file */
MidiFile::_readtrackhdr()
{
    if (_readhd("MTrk", 1) < 0) {
	return -1;
    }
    _toread = _read32();

    return _toread;
}


/* Read a 32-bit quantity from the midi file */
MidiFile::_read32()
{
    char c = _readchar();
    int value = c & 0xff;
    c = _readchar();
    value = (value << 8) + (c & 0xff);
    c = _readchar();
    value = (value << 8) + (c & 0xff);
    c = _readchar();
    value = (value << 8) + (c & 0xff);

    return(value);
}
 

/* Read a 16-bit quantity from the midi file */
MidiFile::_read16()
{
    char c = _readchar();
    int value = c & 0xff;
    c = _readchar();
    value = (value << 8) + (c & 0xff);

    return(value);
}


/* Read a variable length integer from the midi file */
MidiFile::_readvar()
{
    int value, c;
  
    value = c = _readchar();
    if (c & 0x80) {
	value &= 0x7f;
	do {
	    c = _readchar();
	    if (c == -1)
		return -1;

	    value = (value << 7) + (c & 0x7f);
	} while (c & 0x80);
    }
    return value;
}


MidiFile::_readhd(char *looking, int skip)
{
    int c;
  
    if (skip) {
	do {
	    c = _readchar();
	} while (c != EOF && c != looking[0]);
    }
    else { 
	c = _readchar();
    }

    if (c != looking[0]) return -1;
    c = _readchar();
    if (c != looking[1]) return -1;
    c = _readchar();
    if (c != looking[2]) return -1;
    c = _readchar();
    if (c != looking[3]) return -1;
    return 1;
}


/* Read the midi file header from the midi file */
MidiFile::_readfilehdr()
{
    if (_readhd("MThd",0) < 0) {
	return -1;
    }

    _hdrlength = _read32();         // Read the length, 32-bit quantity
    _toread = _hdrlength;
    _format = _read16();            // Type 0, 1, or 2
    int numtracks = _read16();       // Number of tracks
    _division = _read16();          // quarter-note divisions

    while (_toread > 0) 
	_readchar();

    return(numtracks);
}


/* Read a sysex event from the midi file */
MidiEvent *MidiFile::_readsysexevent(unsigned long long time)
{
    MidiEvent *mev = new MidiEvent;
    MDevent *ev = &mev->event;

    ev->msglen = _readvar();

    ev->sysexmsg = (char *)malloc(ev->msglen);
    ev->stamp = time;
    for (int i = 0; i < ev->msglen; i++) 
	ev->sysexmsg[i] = _readchar();

    return mev;
}

MidiEvent *
MidiFile::_readmidievent(int status, int *oldstatus, unsigned long long time)
{
    MidiEvent *mev = new MidiEvent;
    MDevent *ev = &mev->event;
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

    // No associated system exclusive data, so zero this out
    ev->msglen = 0;
    ev->sysexmsg = NULL;

    needed = getMsgLen(status);
  
    // if running status in effect, read mess[2],
    // otherwise read mess[1] and mess[2]
    while (running < needed) {
	char c = _readchar();
	ev->msg[running] = c & 0xFF;
	running++;
    }
    ev->stamp = time;

    return mev;
}


/* Read a meta event from the file */
MidiEvent*
MidiFile::_readmetaevent(unsigned long long time)
{
    MidiEvent *mev = new MidiEvent;
    MDevent *ev = &mev->event;

    ev->msg[0] = 0xff;
    ev->msg[1] = _readchar();
    ev->msglen = _readvar() + 3;	// 3 bytes for metaevent header
    ev->stamp = time;

    if ((ev->sysexmsg = (char *)malloc(ev->msglen)) == NULL) {
	return NULL;
    }

    // Stick on a meta header
    ev->sysexmsg[0] = 0xff;
    ev->sysexmsg[1] = ev->msg[1];
    ev->sysexmsg[2] = (char) ev->msglen - 3;	// Store original length

    for (int i = 3; i < ev->msglen; i++) {
	ev->sysexmsg[i] = _readchar();
    }

    assert(mfIsValidMeta(mev));

    return mev;
}


/* Append the given event onto the end of the list */
void
MidiFile::_append(MidiEvent *ev)
{
    if (_tail == NULL) {
	// This is the first event, so we need to set up the list
	_head = ev;
	ev->prevEvent = NULL;
    }
    else {
	_tail->nextEvent = ev;
	ev->prevEvent = _tail;
    }

    ev->nextEvent = NULL;
    _tail = ev;
    _length++;
}


/* Insert the new event before the given event */
void
MidiFile::_insertBefore(MidiEvent *oldev, MidiEvent *newev)
{
    if (oldev == _head) {
	newev->nextEvent = _head;
	newev->prevEvent = NULL;
	_head->prevEvent = newev;
	_head = newev;
    } else {
	MidiEvent *prev = oldev->prevEvent;
	
	prev->nextEvent  = newev;
	newev->prevEvent = prev;
	newev->nextEvent = oldev;
	oldev->prevEvent = newev;
    }

    _length++;
}

