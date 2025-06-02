/************************************************************************\
 * 	File:         midifile.h                                        *
 * 									*
 *	Contains the header information for the MidiFile classes	*
 *	used by the MidiPlayer.  While this code is superficially	*
 *	similar to the MF library, it is tuned specifically for the	*
 *	needs of efficient playback.					*
 *                                                                      *
 *     Copyright (c) 1995 Silicon Graphics, Inc. All Rights Reserved.   *
\************************************************************************/

#ifndef _MIDIFILE_H
#define _MIDIFILE_H

// Standard Include Files
#include <stdio.h>
#include <dmedia/midi.h>


// The following MIDImetaEvents enumeration is a listing of all the types
// of possible MIDI File Meta Events.
enum MIDImetaEvents {
    MIDImeta_SeqNumber          = 0x00,
    MIDImeta_Text               = 0x01,
    MIDImeta_Copyright          = 0x02,
    MIDImeta_Name               = 0x03,
    MIDImeta_Instrument         = 0x04,
    MIDImeta_Lyric              = 0x05,
    MIDImeta_Marker             = 0x06,
    MIDImeta_CuePoint           = 0x07,

    MIDImeta_ChannelPrefix      = 0x20,
    MIDImeta_EOT                = 0x2F,
    MIDImeta_SetTempo           = 0x51,
    MIDImeta_SMPTEoffset        = 0x54,
    MIDImeta_TimeSignature      = 0x58,
    MIDImeta_KeySignature       = 0x59,
    MIDImeta_SeqSpecific        = 0x7F,
    MIDImeta_Invalid		= 0x17e,
};


struct MidiEvent {
    MDevent event;
    long long time;			// Relative UST of event

    MidiEvent *nextEvent;		// Next chronological event
    MidiEvent *prevEvent;		// Previous chronological event
    MidiEvent *nextTrackEvent;		// Next event on same track
    MidiEvent *prevTrackEvent;		// Previous event on same track
};

    
class MidiTrack {
  public:
    MidiTrack();
    ~MidiTrack();
    
    void	insertEvent(MidiEvent*);
    void        deleteEvent(MidiEvent*);
    MidiEvent  *lastEvent();
    MidiEvent  *firstEvent();
    MidiEvent  *seekEvent(long long tick);
    int      	numberEvents(void);
    char       *name(void);
    int         empty(void);
    void        print(FILE *fp);

  private:
    // Pointers to a doubly-linked list of events
    MidiEvent      *_head;
    MidiEvent      *_tail;
    int             _length;
    char           *_name;

    void	    _append(MidiEvent *ev);
    void	    _insertBefore(MidiEvent *oldev, MidiEvent *newev);
};


// Class definition
class MidiFile {
  public: 
    MidiFile();
    ~MidiFile();

    // File access methods
    int			load(char *name);
    int			load(int fd);
    int			save(char *name);

    // Basic information about the MIDI file
    int 		format(void);
    int			numberTracks(void);
    int  		division(void);
    int			initialTempo(void);
    void		setFormat(int format);
    void		setDivision(int division);
    const char *	sequenceName(void);

    // Track reference routines
    MidiTrack 		*getTrack(int n);
    void                addTrack(MidiTrack *track);
    void		deleteTrack(int n);

    // Playback support routines
    MidiEvent	       *firstEvent();
    MidiEvent	       *lastEvent();
    MidiEvent 	       *seekTick(long long tick);

    // Miscellaneous routines
    void	        print(FILE *fp);
    void		insertEvent(MidiEvent *);
    MDevent*		getPrerollEvents(int *howmany, long long time);

  private:
    MidiTrack         **_tracks;      // The list of tracks in the file
    int		   	_ntracks;     // The number of tracks in the list
    int			_activeTrack; // In type 2 files, the track to play
    unsigned long       _hdrlength;   // length of the header, always 6
    int		   	_format;      // format (0, 1, or 2)
    int		   	_division;    // ticks per quarter-note
    int		   	_toread;      // to read in current chunk 
    FILE	       *_fp;          // The pointer to the file
    MidiEvent	       *_head;	      // Head of the event list
    MidiEvent	       *_tail;	      // Tail of the event list
    MidiEvent          *_lastEvent;   // Last event inserted (optimization)
    int			_length;      // Number of events in file
    char	       *_sequenceName; // Name of the sequence (if known)
    char	       *_fileName;    // Name of the file we loaded from

    // Internal routines for writing midi files
    int 		_readtrack(MidiTrack *);
    int			_readfilehdr();
    int 		_readtrackhdr();
    int			_readchar();
    int			_read32();
    int 		_read16();
    int			_readvar();
    int			_readhd(char *,int);
    MidiEvent	       *_readsysexevent(unsigned long long);
    MidiEvent          *_readmetaevent(unsigned long long);
    MidiEvent          *_readmidievent(int, int *, unsigned long long);

    // Internal Routines to Write MIDI Disk Files                    
    int			_writetrack(int); 
    void		_write32(int);
    void                _write16(int);
    int                 _writevar(int);
    int                 _writetime(int);
    int                 _writehd(char *looking);
    void                _writefilehdr();
    int                 _writeevent(MidiEvent *, unsigned long long *time);
    int                 _writemidievent(MidiEvent *, unsigned long long *time);
    int                 _writemetaevent(MidiEvent *, unsigned long long *time);
    unsigned long       _writetrackhdr();

    // Miscellaneous support routines
    void		_cleanup(void);
    void		_append(MidiEvent *ev);
    void		_insertBefore(MidiEvent *oldev, MidiEvent *newev);
};


////////////////////////////////////////////////////////////////////////////
// libmidifileutils: Miscellaneous function which are useful for MIDI     //
//    events.                                                             //
////////////////////////////////////////////////////////////////////////////

// Macro definitions for return values
#ifndef True
#define True     (1)
#endif

#ifndef False
#define False    (0)
#endif

int     mfIsSystemCommon(MidiEvent *fev);
int     mfIsSystemRealTime(MidiEvent *fev);
int     mfIsSystemExclusive(MidiEvent *fev);
int     mfIsChannelVoice(MidiEvent *fev);
int     mfIsChannelMode(MidiEvent *fev);
int     mfIsMetaEvent(MidiEvent *ev);
int	mfIsTempoEvent(MidiEvent *ev);
int	mfIsMidiEvent(MidiEvent *ev);
int 	mfIsValidMeta(MidiEvent *ev);
int     mfMessageType(MidiEvent *fev);
int     mfChannelVoiceType(MidiEvent *fev);
int     mfChannelModeType(MidiEvent *fev);
int     mfSystemCommonType(MidiEvent *fev);
int     mfSystemRealTimeType(MidiEvent *fev);
int 	mfMetaType(MidiEvent *fev);
char   *mfEncodeEvent(MidiEvent *fev);
int	mfGetTempo(MidiEvent *ev);

#endif    // _LIBMIDIFILE_H
