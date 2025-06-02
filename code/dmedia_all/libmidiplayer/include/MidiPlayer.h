/************************************************************************\
 * 	File:		MidiPlayer.h					*
 *	Author:		John Kraft (jfk@sgi.com)			*
 *								        *
 *	A class for playing back midifiles.  This class accepts		*
 *	a MFfile description (loaded with the midifile library)		*
 *	and allows a caller to play, stop, seek to a given point,	*
 *	change the playback tempo, etc.					*
 *									*
 *	Copyright (C) 1995, Silicon Graphics Inc. All rights reserved.	*
\************************************************************************/

#ifndef MIDIPLAYER_H
#define MIDIPLAYER_H

#include <dmedia/midi.h>
#include "MPfile.h"

class MidiFile;
class TempoMap;

// Select which types of events should be restored on play.  This
// is selectable, since in some cases users might want to alter
// the synthesizer state manually.
const int MIDI_PREROLL_NONE 		= 0;
const int MIDI_PREROLL_NOTES		= 1;
const int MIDI_PREROLL_CTLRS		= 2;
const int MIDI_PREROLL_PROGRAMCHANGE	= 4;
const int MIDI_PREROLL_ALL		= 0xff;


// The PlayStatus is returned by the mode() method.  
// It indicates the state of the player and can be used
// to determine when playback is complete.

#if !defined(PLAYING) 
#define PLAYING MP_PLAYING
#endif

#if !defined(STOPPED)
#define STOPPED MP_STOPPED
#endif

enum MidiPlayStatus {
	MP_WAITING_TO_START,
	MP_PLAYING,
	MP_STOPPED
};



enum MidiPlaybackMode {
	MP_PLAYMODE_STOPATEND,
	MP_PLAYMODE_LOOP,
	MP_PLAYMODE_SWING,
};


class MidiPlayer {
  public:
    MidiPlayer(MidiFile*);		// Constructor
    ~MidiPlayer();			// Destructor

    // Basic deck-control routines
    void		playAt(long long startTime);
    void		stopAt(long long stopTime);
    void		setPlayRange(long long startTime, long long endTime);
    void		seekTime(long long time);
    void		seekTick(long long tick);
    long long		durationTime(void);
    long long		positionTick(void);
    long long   	positionTime(void);
    MidiPlayStatus  	mode(void);
    void		setPlaybackMode(MidiPlaybackMode mode);
    MidiPlaybackMode	playbackMode();
    void		setPlaySelectionOnly(int value);
    int			playSelectionOnly();

    // Global parameters

    // NOTE: Negative tempo implies playing backwards, which is not yet
    // implemented.
    void	setTempoScale(float tempoScale);
    float	tempoScale(void);
    void	setInterface(char *name);
    void	transmitProgramChanges();
    void	setPrerollOptions(int options);

    // Parameters which may be set on a per-track basis
#if 0 /* These guys aren't quite ready for prime time */
    void	setMute(int track, int value);
    void	setVolume(int track, int level);
    int		isMuted(int track);
    int		volume(int track);
#endif

  private:
    // Private variables relating to the MIDI data
    MidiFile   *_mffile;		// Pointer to MFfile structure
    TempoMap   *_tempoMap;		// Tempo map

    // IPC-related stuff
    int		_fds[2];		// message pipe file descriptors
    int		_pid;			// Process ID of player process
    int		_timeout;		// Timeout value (in microsecs)

    // Various time-keeping variables.
    long long	_positionTick;		// Current play time position
    long long   _durationTime;		// Length of file in nanosecs
    long long	_durationTick;		// Length of file in ticks
    long long	_startTime;		// Time when we hit play
    long long   _playRangeStartTick;	// Start of play range
    long long	_playRangeEndTick;	// End of the play range
    
    MidiPlayStatus	_mode;		// Current playback status
    MidiPlaybackMode	_playbackMode;	// Current playback mode
    int			_playSelOnly;	// Play selection only flag
    float		_tempo;		// Current tempo
    MDport		_outport;	// MIDI output port
    char       	       *_interface;	// Interface name
    MidiEvent          *_currEvent;	// A pointer to the current event

    // Private methods
    int		_startPlayerProcess(void);	
    int		_killPlayerProcess(void);
    void	_updateDuration(void);
    void	_dispatchLoop(void);
    void	_handleMessages(void);
    void	_sendMsg(int, long long value1 = 0, long long value2 = 0);
    void	_sendMsg(int, float);
    void	_transmitEvents(void);
    void	_transmitPreroll(long long tick);
    void	_prerollProgramChanges();
    void	_seekTick(long long tick, int flush_immediately);
    void	_stopPlayback(long long when);
    static void _startup(void *value);
};
    

#endif // MIDIPLAYER_H
