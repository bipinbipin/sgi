/************************************************************************\
 *	File:		MidiPlayer.c++					*
 *									*
 *	Implementation of a simple MIDI playback class.			*
 *									*
 *									*
\************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <bstring.h>
#include <errno.h>
#include <stropts.h>
#include <poll.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/fcntl.h>
#include <dmedia/dmedia.h>
#include <dmedia/midi.h>

#include "MPfile.h"
#include "MPtempomap.h"
#include "MidiPlayer.h"

#define BLOCK_INDEFINITELY	-1000


#if defined(DEBUG)
#define DPRINTF printf
#else
#define DPRINTF 0 && printf 
#endif

// Message types
#define PLAY_MSG		1
#define STOP_MSG		2
#define SEEK_MSG		3
#define SETPLAYRANGE_MSG	4
#define SETLOOPMODE_MSG		5
#define XMITPROGCHANGE_MSG	6
#define SETTEMPO_MSG		7
#define SETINTERFACE_MSG	8

struct MidiPlayerMsg {
    int	type;
    long long time;
    long long time2;
    float fvalue;
};



extern "C" {
    int fcntl (int fildes, int cmd, ... /* arg */);
#ifdef DMKUDZU
    pid_t sproc(void (*entry) (void *), unsigned inh, ...);
#else
    int sproc(void (*entry) (void *), unsigned inh, ...);
#endif
}


/* Constructor
 */
MidiPlayer::MidiPlayer(MidiFile *mfile)
{
    // Initialize some of the private method variables
    _outport	  = NULL;
    _mffile       = mfile;
    _pid          = 0;
    _timeout      = BLOCK_INDEFINITELY;
    _positionTick = 0;
    _durationTime = 0;
    _durationTick = 0;
    _startTime    = 0;
    _mode         = MP_STOPPED;
    _playbackMode = MP_PLAYMODE_STOPATEND;
    _playSelOnly  = False;

    _currEvent    = mfile->firstEvent();
    _tempoMap     = new TempoMap(_mffile->division());
    _tempoMap->addAllTempos(_mffile->getTrack(0));
    
    // Figure out the overall duration of the piece
    _updateDuration();
  
    // Start the player process 
    _startPlayerProcess();

    this->setInterface(NULL);
    this->seekTime(0); 
    this->setPlayRange(0, _durationTime);
    this->setPlaybackMode(MP_PLAYMODE_STOPATEND);
    this->setPlaySelectionOnly(False);
    this->setTempoScale(1.0);
}


/* Destructor
 */
MidiPlayer::~MidiPlayer()
{
    _killPlayerProcess();

    // Should we delete the MIDI file stuff?
}


/*
 * Send messages to the player process.
 */

void 
MidiPlayer::playAt(long long time)
{
    _sendMsg(PLAY_MSG, time);
    while(_mode != MP_PLAYING)
	usleep(1000);
}

void
MidiPlayer::stopAt(long long time)
{
    _sendMsg(STOP_MSG, time);
}


void
MidiPlayer::setPlayRange(long long startTime, long long endTime)
{
    _playRangeStartTick = _tempoMap->timeToTicks(startTime);
    _playRangeEndTick   = _tempoMap->timeToTicks(endTime);

    if (mode() == MP_PLAYING && _playSelOnly) {
	if (positionTick() < _playRangeStartTick) {
	    _sendMsg(SEEK_MSG, _playRangeStartTick);
	}
    }
}


void
MidiPlayer::setPlaySelectionOnly(int value)
{
    _playSelOnly = value;
}


int
MidiPlayer::playSelectionOnly()
{
    return _playSelOnly;
}


/*
 * Move to a specific location in the MIDI file
 */
void
MidiPlayer::seekTime(long long time)
{
    _sendMsg(SEEK_MSG, _tempoMap->timeToTicks(time));
}

void
MidiPlayer::seekTick(long long tick)
{
    _sendMsg(SEEK_MSG, tick);
}


/*
 * Set the playback mode (STOP-AT-END, LOOP, SWING) to run in
 */

void
MidiPlayer::setPlaybackMode(MidiPlaybackMode mode)
{
    _playbackMode = mode;
}

MidiPlaybackMode
MidiPlayer::playbackMode()
{
    return _playbackMode;
}


/*
 * Return the file's playing time (length) in nanoseconds.  This
 * length isn't changed by tempo changes.
 */
long long
MidiPlayer::durationTime(void)
{
    return _durationTime;
}


/*
 * Return the current position (in nanoseconds) of the player.  This 
 * position can be changed by calling the seekTime
 */
long long
MidiPlayer::positionTime(void)
{
    return _tempoMap->ticksToTime(positionTick());
}


/*
 * Return the current position (in ticks) of the player.
 */
long long
MidiPlayer::positionTick(void)
{
    if (_mode == MP_PLAYING) 
	_positionTick = mdTellNow(_outport);

    return _positionTick;
}


/* 
 * Return information about what the player is currently doing.
 * This allows the caller to figure out whether we're still playing
 * or whether the player is stopped.
 */
MidiPlayStatus
MidiPlayer::mode(void)
{
    return _mode;
}


/*
 * Change the floating point value used to scale tempo.  Setting it
 * to a value < 1.0 makes playback faster.  Setting it to a value > 1.0
 * makes playback slower.
 */
void
MidiPlayer::setTempoScale(float tempo)
{
   _sendMsg(SETTEMPO_MSG, tempo); 
}


/*
 * Return the current tempo scale value.
 */
float
MidiPlayer::tempoScale(void)
{
    return _tempo;
}


/*
 * The the port to which the output should be directed.
 */
void
MidiPlayer::setInterface(char *name)
{
    _sendMsg(SETINTERFACE_MSG, (long long) name);
}


/*
 * In order to simulate the behavior of a multi-track tape recorder
 * more closely, we transmit any "state" needed to start playing notes
 * correctly.  In general, this state consists of any sustained notes
 * which were triggered before the time we're starting at, controller
 * settings, appropriate program changes, etc.  
 *
 * In some cases, however, a user might want not want to restore all of
 * the state.  For example, if a user wants to key in a program change
 * for a particular track by hand, we shouldn't restore program change
 * state (for doing so would undo the user's change from the synth's
 * front panel.  To allow for this, the calling app can set the 
 * preroll options which govern what (if any) state is restored when
 * playback is resumed.  The parameter pass is a mask made up of the
 * following flags:
 *	MIDI_PREROLL_NOTES		Just restart sustained notes
 *	MIDI_PREROLL_CTLRS		Restore controller state
 *	MIDI_PREROLL_PROGRAMCHANGE 	Send any program changes
 *	MIDI_PREROLL_ALL		All possible preroll classes
 * The actual calculation of preroll state is done in preroll.c++.
 */
void
MidiPlayer::setPrerollOptions(int)
{
}


void
MidiPlayer::transmitProgramChanges()
{
    _sendMsg(XMITPROGCHANGE_MSG);
}


/******************** Per-Track Parameters **********************************/



/****************************************************************************
 *	Private method definitions
 ****************************************************************************/

/*
 * Start up the process which actually transmits the MIDI data.  We
 * start it as a separate process so that the current process doesn't
 * need to alter its event dispatch model.
 */
int
MidiPlayer::_startPlayerProcess(void)
{
    // Kill off any existing process
    if (_pid)
	_killPlayerProcess();

    // Set up the communications pipe
    if (pipe(_fds) == -1) 
	return -1;

#if !defined(NO_SPROC)
    // We don't care about the player process's exit status
    signal(SIGCLD, SIG_IGN);
    _pid = sproc(_startup, PR_SALL, (void*) this);
#else
    _pid = getpid();
#endif
    if (_pid < 0) 
	return -1;
    else
	return 0;
}


/*
 * Shut down the player process and close any resources used for
 * communicating between this process and the player process.
 */
int
MidiPlayer::_killPlayerProcess(void)
{
    kill(_pid, SIGKILL);
    _pid = 0;
    close(_fds[0]);
    close(_fds[1]);
    return 0;
}


/* called when the player process first starts up. */
void
MidiPlayer::_startup(void *value)
{
    MidiPlayer *player = (MidiPlayer*) value;
    player->_dispatchLoop();
    exit(0);
}


/*
 * Determine the length of the composition in time and 
 * store it in the duration variable.
 */
void
MidiPlayer::_updateDuration(void)
{
    // Search backwords from the end of the file for the first non-meta
    // event.
    MidiEvent *ev = _mffile->lastEvent();
    while (ev != NULL && mfIsMetaEvent(ev))
	ev = ev->prevEvent;

    if (ev == NULL) {
	_durationTick = 0;
	_durationTime = 0;
    } else {
	_durationTick = ev->event.stamp;
	_durationTime = _tempoMap->ticksToTime(_durationTick);
    }
}

/*
 * dispatches events and invokes the transmit call 
 * periodically when necessary. 
 */

#define MAX(_x, _y) (((_x) > (_y)) ? (_x) : (_y))

void
MidiPlayer::_dispatchLoop()
{
    struct pollfd fds[1];

    fds[0].fd 	   = _fds[0];
    fds[0].events  = POLLIN;
    fds[0].revents = 0;

    for(;;) {
	// Set up for the poll call
	DPRINTF("Calling poll, _timeout = %d\n", _timeout);
	int retval = poll(fds, 1, _timeout/1000);
	if (retval == -1) {
	    perror("MidiPlayer::_dispatch: Unexpected error from poll");
	    return;
	} 
	else if (retval == 0) {
	    // A timeout occurred
	    if (_mode == MP_PLAYING)
		_transmitEvents();
	}
	else {
	    // We need to handle input on one of the file descriptors
	    if (fds[0].revents == POLLIN) {
		_handleMessages();
	    } else {
//		printf("Spurious return from poll\n");
	    }
	}
    }
}


/*
 * Handle messages sent to us by the controlling process
 */
void
MidiPlayer::_handleMessages(void)
{
    char *name;
    MidiPlayerMsg msg;
    long long positionTick;

    if (read(_fds[0], &msg, sizeof(msg)) != sizeof(msg)) {
	fprintf(stderr, "MidiPlayer::_handleEvents: bogus message size");
	return;
    }

    switch (msg.type) {
      case PLAY_MSG:
	DPRINTF("Play msg received.\n");
	if (_mode != MP_PLAYING) {

	    // Constrain ourselves to the specified play range
	    if (_playSelOnly) {
		if (_positionTick < _playRangeStartTick ||
		    _positionTick >= _playRangeEndTick)
		    _positionTick = _playRangeStartTick;
	    }

	    mdSetTemposcale(_outport, _tempo);
	    _mode = MP_PLAYING;
	    _seekTick(_positionTick, True);
	    _transmitEvents();
	}
	break;

      case STOP_MSG:
	DPRINTF("Stop msg received\n");
	_stopPlayback(mdTellNow(_outport));
	break;

      case SEEK_MSG:
	_positionTick = msg.time;
	if (_mode == MP_PLAYING) {

	    // Constrain ourselves to the specified play range
            if (_playSelOnly) {
                if (_positionTick < _playRangeStartTick ||
                    _positionTick >= _playRangeEndTick)
                    _positionTick = _playRangeStartTick;
            }

	    _seekTick(_positionTick, True);
	}
	break;

      case XMITPROGCHANGE_MSG:
	_prerollProgramChanges();
	break;
	
      case SETPLAYRANGE_MSG:
	break;

      case SETTEMPO_MSG:
	_tempo = msg.fvalue;
	
	if (_outport != NULL)
	    mdSetTemposcale(_outport, _tempo);
	break;

      case SETINTERFACE_MSG:

	// Close the old port, if necessary
	positionTick = this->positionTick();
	if (_outport != NULL) {
	    mdPause(_outport);
	    mdPanic(_outport);    
	    mdClosePort(_outport);
	}

	name = (char*) msg.time;
	_outport = mdOpenOutPort(name);

	// Put the port into relative tick mode, since this is what the midifile
	// library provides us.
	mdSetStampMode(_outport, MD_RELATIVETICKS);

	// Set the division
	mdSetDivision(_outport, _mffile->division());
	mdSetTempo(_outport, _mffile->initialTempo());
	mdSetTemposcale(_outport, _tempo);	
	fcntl(mdGetFd(_outport), F_SETFL, FNONBLOCK);

	// Reposition ourselves for playback
	_seekTick(positionTick, True);
	break;	
    
      default:
	fprintf(stderr, "MidiPlayer::_handleEvents: unrecognized msg %d\n",
		msg.type);
    }
}

	    
/*
 * Send a message to the player process
 */

void
MidiPlayer::_sendMsg(int type, long long time1, long long time2)
{
    MidiPlayerMsg msg;

    msg.type  = type;
    msg.time  = time1;
    msg.time2 = time2;

    write(_fds[1], &msg, sizeof(msg));

#if NO_SPROC
    _dispatchLoop();
#endif
}

void
MidiPlayer::_sendMsg(int type, float value)
{
    MidiPlayerMsg msg;

    msg.type   = type;
    msg.fvalue = value;

    write(_fds[1], &msg, sizeof(msg));

#if NO_SPROC
    _dispatchLoop();
#endif
}


/*
 * transmit events
 */

void
MidiPlayer::_transmitEvents(void)
{
    unsigned long long now;
   
    DPRINTF("In transmit events, currEvent %x ...\n", _currEvent);

    // Handle the end of file situation
    if (_currEvent == NULL) {
	if (mdTellNow(_outport) > _durationTick) {
	    switch (_playbackMode) {
	      case MP_PLAYMODE_STOPATEND:
		// We're done, shut down
		_stopPlayback(_durationTick);
		return;

	      case MP_PLAYMODE_LOOP:
		if (_playSelOnly) 
		    _seekTick(_playRangeStartTick, False);
		else
		    _seekTick(0, False);
		break;

	      case MP_PLAYMODE_SWING:
		// 
		break;
	    }
	} else {
	    // We need to wait awhile longer
	    _timeout = 100000;
	    return;
	}
    }

    dmGetUST(&now);

    // I'd like to know if we're late
    long long stamp = _tempoMap->ticksToTime(_currEvent->event.stamp);

//    long long time  = _startTime + stamp;
//    if (now > time)
//	printf("WARNING! Late transmitting event (now %lld, event %lld)\n",
//	       now, time);

    // Otherwise, queue up some events for transmission
    long long lookaheadamt = (long long) (50000000.0 * _tempo); 
    long long endStamp     = _tempoMap->timeToTicks(stamp + lookaheadamt);

    while (_currEvent && _currEvent->event.stamp < endStamp) {

        // Check for the end of the play range
	if (_playSelOnly && (_playRangeEndTick < _currEvent->event.stamp)) {
	    switch (_playbackMode) {
	      case MP_PLAYMODE_STOPATEND:
		_stopPlayback(_playRangeEndTick);
		return;

	      case MP_PLAYMODE_LOOP:
		_timeout = 0;
		_seekTick(_playRangeStartTick, False);
		return;

	      case MP_PLAYMODE_SWING:
	        // Not yet supported
		break;  
	    }
        }

	// Skip meta events
	if (!mfIsMidiEvent(_currEvent)) {
	    _currEvent = _currEvent->nextEvent;
	    continue;
	}
	
	DPRINTF("Sending event w/ stamp %lld\n", _currEvent->event.stamp);
	int retval = mdSend(_outport, &_currEvent->event, 1);
	if (retval == -1 && errno == EWOULDBLOCK) {
	    break;
	} else {
	    _currEvent = _currEvent->nextEvent;
	}
    }

    long long future;

    // Get rid of any trailing META events
    while (_currEvent && !mfIsMidiEvent(_currEvent))
	_currEvent = _currEvent->nextEvent;

    if (_currEvent == NULL) {
	// We'll wait for the last event to get out at any event, so
	// just hang out till then
	future = 100000;
    } else {
	// I need to figure out how much time lies between the current 
	future = (_tempoMap->ticksToTime(_currEvent->event.stamp) -
	    _tempoMap->ticksToTime(mdTellNow(_outport))) / 1000;
    }

    // We want to wake up 20 ms before the event needs to go out
    if (future > 20000)
	future -= 20000;

    if (future < 0)
	future = 0;

    _timeout = future;
}


/*
 * Seek to a specified time in the file, transmitting all
 * the messages needed to silence the previously playing notes
 * set up for the new play location.
 */
void
MidiPlayer::_seekTick(long long tick, int flush_immediately)
{
    if (flush_immediately || _currEvent == NULL) {
	mdPause(_outport);
    } else {
	// Wait for the events that are already queued to go out
	long long stamp = _currEvent->prevEvent->event.stamp;

	while (mdTellNow(_outport) < stamp) 
	    usleep(1000);
    }

    // Turn off any lingering NOTE ON's.  This is a hack; we should really
    // keep track of what note on's have been transmitted but not turned
    // off ourselves, as per the midi spec.
    mdPanic(_outport);

    // Move to the proper position in the sequence
    _currEvent = _mffile->seekTick(tick);
	
    _positionTick = tick;

    // Readjust the timeline so that the new position is equivalent to now
    dmGetUST((unsigned long long *) &_startTime);
    mdSetStartPoint(_outport, _startTime, _positionTick);

    // Transmit preroll events
    if (_mode == MP_PLAYING) {
        _transmitPreroll(_positionTick);

	// We want to reenter the transmission code immediately, so
	// set the timeout to 0
	_timeout = 0;
    }
}


/*
 * Stop playback.  This can be only be called by the player thread.
 */
void
MidiPlayer::_stopPlayback(long long when)
{
    assert(getpid() == _pid);

    _mode    = MP_STOPPED;
    _timeout = BLOCK_INDEFINITELY;
    _positionTick = when;

    mdPause(_outport);
    mdPanic(_outport);
}


/*
 * Transmit the preroll events needed to properly restore the MIDI
 * state at this point in time.  The idea here is that MIDI is an
 * extremely stateful protocol, and in order for stuff to sound correct
 * when played back we need to restore the state whenever we've indexed
 * randomly in a midi file.  The events returned by getPrerollEvents
 * always have a timestamp of 0, so they should get transmitted immediately
 * no matter what we've set the timeline to with mdSetStartPoint.
 */
void
MidiPlayer::_transmitPreroll(long long tick)
{
    int howmany = 0;
    MDevent *events = _mffile->getPrerollEvents(&howmany, tick);

    mdSend(_outport, events, howmany);
    free(events);
}


/*
 * The MIDI Synth takes a while to do the initial load of a patch,
 * so in order to keep the sound from breaking up during the initial
 * portion of the sound we pretransmit the first set of program
 * changes on all sixteen channels.
 */
void
MidiPlayer::_prerollProgramChanges()
{
    MidiEvent *ev = _currEvent;
    int channels  = 0x0;

    while (ev != NULL && channels != 0xffff) {

	if (mfChannelVoiceType(ev) == MD_PROGRAMCHANGE) {
	    int chan = ev->event.msg[0] & MD_CHANNELMASK;
	    if ((channels & (1 << chan)) == 0) {
		// No event has been transmitted for this channel
		MDevent event = ev->event;
		event.stamp = _currEvent->event.stamp;
#if DEBUG
		char buf[80];
		mdPrintEvent(buf, &event, 1);
		DPRINTF("Prerolling %s\n", buf);
#endif
		mdSend(_outport, &event, 1);
		
		channels |= (1 << chan);
	    }
	}

	ev = ev->nextEvent;
    }
}
