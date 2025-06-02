//////////////////////////////////////////////////////////////////////////
// File: miditrack.h                                                    //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

#ifndef _MIDITRACK_H
#define _MIDITRACK_H

// LinkedList utility
class LinkedList;

// MIDI Include Files
#include <dmedia/midi.h>

#define MF_MIDI_EVENT  (10)
#define MF_META_EVENT  (11)

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
};

class MFmeta {
  public:
    MIDImetaEvents     metatype;
    unsigned long      length;
    char               *msg;
    unsigned long long stamp;
};

class MFfileEvent {
  public:
    MFfileEvent();
    MFfileEvent(int type, MDevent *);
    MFfileEvent(int type, MFmeta *);
    ~MFfileEvent();

    unsigned long long getstamp();

    int     type;

    MDevent *midiev;
    MFmeta  *metaev;
};

class MFtrack {
  public:
    MFtrack();
    ~MFtrack();

    void insertevent(MFfileEvent *);
    void deleteevent(MFfileEvent *);

    void        seektime(unsigned long long);
    MFfileEvent *seekevent(MFfileEvent *);
    MFfileEvent *seekend(void);
    MFfileEvent *seeknthevent(int n);
    MFfileEvent *seekbeginning(void);
    MFfileEvent *seekmetaevent(MIDImetaEvents);
    MFfileEvent *nextmetaevent(MIDImetaEvents);

    MFfileEvent *seekmidievent(int type);
    MFfileEvent *nextmidievent(int type);

    MFfileEvent *nextevent(void);
    MFfileEvent *prevevent(void);
    MFfileEvent *currentevent(void);

    unsigned long long nexttimestamp(int *);
    unsigned long long lasttimestamp(int *);
    unsigned long long firsttimestamp(int *);

    int numberEvents();
    int empty();

    void print();
  private:

    // A linked list which hold the actual events
    LinkedList      *data_;

    // Just a pointer to the current event, to maintain state.
    MFfileEvent    *current_;
};

#endif  // _MIDITRACK_H
