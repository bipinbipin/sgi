//////////////////////////////////////////////////////////////////////////
// File:         midifile.h                                             //
// Author:       Jordan M. Slott                                        //
//               hordack@esd.sgi.com or hordack@mit.edu                 //
//               Digital Media Systems                                  //
//               Silicon Graphics, Inc.                                 //
//               Based upon an earlier implementation by Archer Sully   //
// Date:         July 28, 1995                                          //
// Description:  A header file for libmidifile. It provides the class   //
//               definitions for the classes MFfile, MFtrack, MFmeta    //
//               and MFfileEvent. In addition it declares prototypes    //
//               for useful functions on MIDI file events.              //
//                                                                      //
// Copyright (c) July 1995, Silicon Graphics, Inc. All Rights Reserved. //
//////////////////////////////////////////////////////////////////////////

#ifndef _MIDIFILE_H
#define _MIDIFILE_H

// Standard Include Files
#include <stdio.h>
#include <dmedia/midi.h>

class MFentry;

///////////////////////////////////////////////////////////////////////////
// MFmeta Class definition                                               //
///////////////////////////////////////////////////////////////////////////

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
};

// Class definition for MFmeta
class MFmeta {
  public:
    MIDImetaEvents     type;              // The type of the Meta Event
    unsigned long      msglen;            // The length of the message in msg
    char               *msg;              // Pointer to message
    unsigned long long stamp;             // 64-bit time stamp
};


//////////////////////////////////////////////////////////////////////////
// MFfileEvent class definition                                         //
//////////////////////////////////////////////////////////////////////////

// The following definitions are used by the MFfileEvent class and are
// returned by the eventType() method.
#define MF_MIDI_EVENT  (10)
#define MF_META_EVENT  (11)

// MFfileEvent class definition
class MFfileEvent {
  public:
    //////////////////////////////////////////////////////////////////////
    // Constructor: MFfileEvent(MDevent *ev);                           //
    //   requires: ev is not NULL.                                      //
    //   modifies: nothing                                              //
    //   effects:  Creates an event of type MF_MIDI_EVENT, and sets the //
    //             pointer to the MIDI event to ev.                     //
    //   returns:  nothing                                              //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent(MDevent *);

    //////////////////////////////////////////////////////////////////////
    // Constructor: MFfileEvent(MFmeta *ev);                            //
    //   requires: ev is not NULL.                                      //
    //   modifies: nothing                                              //
    //   effects:  Creates an event of type MF_META_EVENT, and sets the //
    //             pointer to the Meta event to ev.                     //
    //   returns:  nothing                                              //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent(MFmeta *);

    // Destructor
    ~MFfileEvent();

    //////////////////////////////////////////////////////////////////////
    // unsigned long long eventStamp();                                 //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  nothing                                              //
    //   returns:  The time stamp associated with the event.            //
    //////////////////////////////////////////////////////////////////////
    unsigned long long eventStamp();

    //////////////////////////////////////////////////////////////////////
    // int eventType();                                                 //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  nothing                                              //
    //   returns:  Returns the type of event, either MF_MIDI_EVENT or   //
    //             MF_META_EVENT.                                       //
    //////////////////////////////////////////////////////////////////////
    int                eventType();

    //////////////////////////////////////////////////////////////////////
    // MDevent *midiEvent();                                            //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  Returns a pointer to the event contained in the      //
    //             object.                                              //
    //   returns:  Returns an MDevent pointer to the event. If the      //
    //             event is not of type MF_MIDI_EVENT, this returns     //
    //             NULL.                                                //
    //////////////////////////////////////////////////////////////////////
    MDevent            *midiEvent();

    //////////////////////////////////////////////////////////////////////
    // MFmeta *metaEvent();                                             //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  Returns a pointer to the event contained in the      //
    //             object.                                              //
    //   returns:  Returns an MFmeta pointer to the event. If the event //
    //             is not of type MF_META_EVENT, this returns NULL.     //
    //////////////////////////////////////////////////////////////////////
    MFmeta             *metaEvent();

  private:
    // The type of the event, either MF_MIDI_EVENT or MF_META_EVENT
    int                _type;

    // A pointer to the MIDI event if the type is MF_MIDI_EVENT, otherwise
    // NULL.
    MDevent            *_midiev;

    // A pointer to the META event if the type is MF_META_EVENT, otherwise
    // NULL.
    MFmeta             *_metaev;
};


///////////////////////////////////////////////////////////////////////////
// MFtrack Class abstraction                                             //
///////////////////////////////////////////////////////////////////////////

// Class Definition
class MFtrack {
  public:
    MFtrack();
    ~MFtrack();

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *insertEvent(MFfileEvent *fev);                      // 
    //   requires: nothing                                              //
    //   modifies: _data                                                // 
    //   effects:  Inserts fev in time-sorted order into the track. The //
    //             event's time stamp must be relative to the beginning //
    //             of the track at time 0. If fev is NULL, this method  //
    //             does nothing. This method does not modify the        //
    //             current seek pointer.                                //
    //   returns:  If fev is successfully inserted into the track, then //
    //             this method returns fev, otherwise returns NULL. If  //
    //             fev is NULL, this returns NULL.                      //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *insertEvent(MFfileEvent *fev);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *deleteEvent(MFfileEvent *fev);                      //
    //   requires: nothing                                              //
    //   modifies: _data and _current                                   //
    //   effects:  Deletes fev from the track. If fev is NULL, this     //
    //             method does nothing. If fev is not an event in the   //
    //             track, then this event does nothing. If the current  //
    //             seek pointer is set to the deleted event, then the   //
    //             current seek pointer is set to the event immediately //
    //             following the deleted event, possibly NULL.          //
    //   returns:  If fev is successfully deleted from the track, then  //
    //             this returns fev, otherwise returns NULL. If fev is  //
    //             NULL or if fev is not an event in the track, returns //
    //             NULL.                                                //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *deleteEvent(MFfileEvent *fev);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *seekTime(unsigned long long time);                  //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  Moves the current seek pointer to the event with a   //
    //             time stamp equal to 'time'. The seek pointer is set  //
    //             to the first event of timestamp 'time' if more than  //
    //             one exists. If no event in the track with timestamp  //
    //             time exists, it sets the current seek pointer to     //
    //             the first event with the next largest timestamp. If  //
    //             'time' is past the last event in the track, it will  //
    //             set the current seek pointer to NULL.                //
    //   returns:  Returns the event to which the seek pointer is set.  //
    //             This may be NULL if 'time' is past the end of the    //
    //             track.                                               //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *seekTime(unsigned long long time);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *seekEvent(MFfileEvent *fev);                        //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  If fev is not NULL, then it sets the current seek    //
    //             pointer to the event with the same pointer as fev.   //
    //             If an event with pointer fev is not in the track,    //
    //             then this method does nothing. If fev is NULL, this  //
    //             method does nothing.                                 //
    //   returns:  If fev is a valid event in the track, returns fev.   //
    //             Otherwise if fev is NULL or if fev is not in the     //
    //             track, returns NULL.                                 //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *seekEvent(MFfileEvent *fev);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *seekEnd(void);                                      //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  Sets the current seek pointer to the last event in   //
    //             the track. If the track is empty, sets the seek      //
    //             pointer to NULL.                                     //
    //   returns:  If the track is not empty, returns the last event in //
    //             the track. Otherwise, returns NULL.                  //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *seekEnd(void);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *seekNthEvent(unsigned int n);                       //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  Moves the current seek pointer to the Nth event in   //
    //             the track. The first event is numbered 1. If n is 0, //
    //             this method does not do anything. If n is greater    //
    //             than the number of events, this method does nothing. //
    //   returns:  Returns the Nth event in the track, if n is between  //
    //             1 and the number of total events in the track.       //
    //             Otherwise, returns NULL.                             //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *seekNthEvent(unsigned int n);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *seekBeginning(void);                                //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  Sets the current seek pointer to the first event in  //
    //             the track. If the track is empty, sets the seek      //
    //             pointer to NULL.                                     //
    //   returns:  If the track is not empty, returns the first event   //
    //             in the track. Otherwise, returns NULL.               //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *seekBeginning(void);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *seekMetaEvent(MIDImetaEvents type);                 //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  Sets the current seek pointer to the first meta      //
    //             event in the track of type 'type'. If the track is   //
    //             empty, sets the seek pointer to NULL. If there is    //
    //             no Meta event of type 'type', then this method sets  //
    //             the current seek pointer to NULL.                    //
    //   returns:  If the track is not empty and a meta event of type   //
    //             'type' exists in the track, returns the first meta   //
    //             event of type 'type' in the track. Otherwise,        //
    //             returns NULL.                                        //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *seekMetaEvent(MIDImetaEvents type);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *seekMidiEvent(int type);                            //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  Sets the current seek pointer to the first midi      //
    //             event in the track of type 'type'. If the track is   //
    //             empty, sets the seek pointer to NULL. If there is    //
    //             no Meta event of type 'type', then this method sets  //
    //             the current seek pointer to NULL. Valid 'type'       //
    //             values are: MD_NOTEON, MD_NOTEOFF,                   //
    //             MD_POLYKEYPRESSURE, MD_CONTROLCHANGE,                //
    //             MD_PROGRAMCHANGE, MD_CHANNELPRESSURE,                //
    //             MD_PITCHBENDCHANGE, MD_SYSEX,                        //
    //             MD_TIMECODEQUARTERFRAME, MD_SONGPOSITIONPOINTER,     //
    //             MD_SONGSELECT, MD_TUNEREQUEST, MD_EOX,               //
    //             MD_TIMINGCLOCK, MD_START, MD_CONTINUE, MD_STOP,      //
    //             MD_ACTIVESENSING, or MD_SYSTEMRESET.                 //
    //   returns:  If the track is not empty and a midi event of type   //
    //             'type' exists in the track, returns the first meta   //
    //             event of type 'type' in the track. Otherwise,        //
    //             returns NULL.                                        //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *seekMidiEvent(int type);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *nextMetaEvent(MIDImetaEvents type);                 //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  Moves the current seek pointer to the next meta      //
    //             event of type 'type' following the events after, but //
    //             not including the current seek pointer. If the seek  //
    //             pointer is NULL, this method does nothing. If a      //
    //             meta event of type 'type' does not exist after the   //
    //             current seek pointer, this method does nothing.      //
    //   returns:  Returns the meta event of type 'type' following the  //
    //             current seek pointer. If the seek pointer is NULL or //
    //             if there is no such event, returns NULL.             //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *nextMetaEvent(MIDImetaEvents type);

    MFfileEvent       *prevMetaEvent(MIDImetaEvents type);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *nextMidiEvent(int type);                            //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  Moves the current seek pointer to the next midi      //
    //             event of type 'type' following the events after, but //
    //             not including the current seek pointer. If the seek  //
    //             pointer is NULL, this method does nothing. If a      //
    //             midi event of type 'type' does not exist after the   //
    //             current seek pointer, this method does nothing.      //
    //   returns:  Returns the midi event of type 'type' following the  //
    //             current seek pointer. If the seek pointer is NULL or //
    //             if there is no such event, returns NULL.             //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *nextMidiEvent(int type);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *nextEvent(void);                                    //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  Moves the current seek pointer to the event          //
    //             following the current event. If the current seek     //
    //             pointer is on the last event, the current seek       //
    //             pointer is set to NULL. If the current seek pointer  //
    //             is NULL, this method does nothing.                   //
    //   returns:  The event following the current one. If the current  //
    //             seek pointer is NULL or if the current seek pointer  //
    //             is on the last event of the track, returns NULL.     //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *nextEvent(void);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *prevEvent(void);                                    //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  Moves the current seek pointer to the event          //
    //             preceeding the current event. If the current seek    //
    //             pointer is on the first event, the current seek      //
    //             pointer is set to NULL. If the current seek pointer  //
    //             is NULL, this method does nothing.                   //
    //   returns:  The event before the current one. If the current     //
    //             seek pointer is NULL or if the current seek pointer  //
    //             is on the first event of the track, returns NULL.    //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *prevEvent(void);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent *currentEvent(void);                                 //
    //   requires: nothing                                              //
    //   modifies: _current                                             //
    //   effects:  nothing                                              //
    //   returns:  Returns the event associated with the current seek   //
    //             pointer. If the track is empty, or if the current    //
    //             seek pointer is NULL, returns NULL.                  //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent       *currentEvent(void);

    //////////////////////////////////////////////////////////////////////
    // unsigned long long currentTimeStamp(int *howmany);               //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  Looks at the current seek pointer and returns the    //
    //             time stamp associated with that event.               // 
    //   returns:  Returns the time stamp associated with the current   //
    //             seek pointer. The variable 'howmany' is set to the   //
    //             number of events which share that same time stamp.   //
    //             If the current seek pointer is NULL, 0 is returned   //
    //             and 'howmany' is set to 0.                           //
    //////////////////////////////////////////////////////////////////////
    unsigned long long currentTimeStamp(int *howmany);

    //////////////////////////////////////////////////////////////////////
    // long long lastTimeStamp(int *howmany);    	                //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  Look at the last event in the track and returns      //
    //             the last time stamp.                                 //
    //   returns:  Returns the time stamp associated with the last      //
    //             event in the track. The variable 'howmany' is set to //
    //             the number of events which share that same time      //
    //             stamp. If the track is empty, 0 is returned and      //
    //             'howmany' is set to 0.                               //
    //////////////////////////////////////////////////////////////////////
    long long lastTimeStamp(int *);

    //////////////////////////////////////////////////////////////////////
    // unsigned long long firstTimeStamp(int *howmany);                 //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  Look at the first event in the track and returns     //
    //             the last time stamp.                                 //
    //   returns:  Returns the time stamp associated with the first     //
    //             event in the track. The variable 'howmany' is set to //
    //             the number of events which share that same time      //
    //             stamp. If the track is empty, 0 is returned and      //
    //             'howmany' is set to 0.                               //
    //////////////////////////////////////////////////////////////////////
    unsigned long long firstTimeStamp(int *);

    //////////////////////////////////////////////////////////////////////
    // int numberEvents(void);                                          //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  nothing                                              //
    //   returns:  Returns the number of events contained in the track. //
    //////////////////////////////////////////////////////////////////////
    int            numberEvents(void);

    //////////////////////////////////////////////////////////////////////
    // int empty(void);                                                 //  
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  nothing                                              //
    //   returns:  Returns 1 if there are one or more events in the     //
    //             track, 0 otherwise.                                  //
    //////////////////////////////////////////////////////////////////////
    int             empty(void);

    //////////////////////////////////////////////////////////////////////
    // void print(void);                                                //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  Prints a summary of the tracks contents to stderr    //
    //   returns:  nothing                                              //
    //////////////////////////////////////////////////////////////////////
    void            print(void);

  private:
    // Pointers to a doubly-linked list of events
    MFentry         *_head;
    MFentry         *_tail;
    MFentry         *_currententry;
    int             _length;

    void            _append(MFfileEvent *data);
    void            _insert(MFfileEvent *newdata);
    MFfileEvent     *_remove(MFfileEvent *data);
    void            _seek(MFfileEvent *data);
    MFfileEvent     *_nth(int n);
    MFfileEvent     *_previous();
    MFfileEvent     *_next();
    MFfileEvent     *_first();
    MFfileEvent     *_last();
    MFfileEvent     *_current();
};



//////////////////////////////////////////////////////////////////////////////
// MFfile Class abstraction                                                 //
//////////////////////////////////////////////////////////////////////////////

// Class definition
class MFfile {
  public: 
    MFfile();
    ~MFfile();

    //////////////////////////////////////////////////////////////////////
    // int format(void);                                                //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  nothing                                              //
    //   returns:  Returns the format of the MIDI File, either 0, 1 or  //
    //             2.                                                   //
    //////////////////////////////////////////////////////////////////////
    int                format(void);

    //////////////////////////////////////////////////////////////////////
    // int numberTracks(void);                                          //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  nothing                                              //
    //   returns:  Returns the number of tracks contained within the    //
    //             file.                                                //
    //////////////////////////////////////////////////////////////////////
    int                numberTracks(void);

    //////////////////////////////////////////////////////////////////////
    // int division(void);                                              //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  nothing                                              //
    //   returns:  Returns the division, the ticks per MIDI quarter-    //
    //             note in the file.                                    //
    //////////////////////////////////////////////////////////////////////
    int                division(void);

    //////////////////////////////////////////////////////////////////////
    // void setFormat(int format);                                      //
    //   requires: nothing                                              //
    //   modifies: _format                                              //
    //   effects:  Sets the format to 'format.' If format is not either //
    //             0, 1, or 2, this method does nothing.                //
    //   returns:  nothing                                              //
    //////////////////////////////////////////////////////////////////////
    void               setFormat(int format);

    //////////////////////////////////////////////////////////////////////
    // void setDivision(int division);                                  //
    //   requires: nothing                                              //
    //   modifies: _division                                            //
    //   effects:  Sets the division of the MIDI file object            //
    //   returns:  nothing                                              //
    //////////////////////////////////////////////////////////////////////
    void               setDivision(int division);

    //////////////////////////////////////////////////////////////////////
    // int load(char *name);                                            //
    //   requires: nothing                                              //
    //   modifies: just about everything!                               //
    //   effects:  Load the file 'name' into memory. If name is NULL,   //
    //             if name does not exist, or if name is not a valid    //
    //             MIDI File, then this method does nothing.            //
    //   returns:  Returns 0 if the file was successfully loaded.       //
    //             Otherwise, returns -1.                               //
    //////////////////////////////////////////////////////////////////////
    int                load(char *name);

    //////////////////////////////////////////////////////////////////////
    // int load(int fd);                                                //
    //   requires: nothing                                              //
    //   modifies: just about everything!                               //
    //   effects:  Loads the file corresponding to the open file with   //
    //             the fd. If the fd is not a valid MIDI file, this     //
    //             method does nothing.                                 //
    //   returns:  Returns 0 if the file was successfully loaded.       //
    //             Otherwise, returns -1.                               //
    //////////////////////////////////////////////////////////////////////
    int                 load(int fd);

    //////////////////////////////////////////////////////////////////////
    // int save(char *name);                                            //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  Saves the data contained within the object to name.  //
    //   returns:  Returns 0 if the file was saved successfully.        //
    //             Returns -1 otherwise.                                //
    //////////////////////////////////////////////////////////////////////
    int                save(char *name);

    //////////////////////////////////////////////////////////////////////
    // void rewind(void);                                               //
    //   requires: nothing                                              //
    //   modifies: The tracks contained within the file                 //
    //   effects:  Sets the seek pointers of all the tracks within the  //
    //             to the beginning of each track respectively.         //
    //   returns:  nothing                                              //
    //////////////////////////////////////////////////////////////////////
    void               rewind(void);

    //////////////////////////////////////////////////////////////////////
    // void seek(unsigned long long time);                              //
    //   requires: nothing                                              //
    //   modifies: The tracks contained within the file                 //
    //   effects:  Performs a seekTime(time) on all of the tracks       //
    //             within the file.                                     //
    //   returns:  nothing                                              //
    //////////////////////////////////////////////////////////////////////
    void               seek(unsigned long long time);

    //////////////////////////////////////////////////////////////////////
    // unsigned long long tell(void);                                   //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  nothing                                              //
    //   returns:  Returns the "position" in the file. It returns the   //
    //             lowest time stamp of all the current seek pointers   //
    //             in each track.                                       //
    //////////////////////////////////////////////////////////////////////
    unsigned long long tell();

    //////////////////////////////////////////////////////////////////////
    // MFtrack *getTrack(int n);                                        //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  nothing                                              //
    //   returns:  Returns the nth track in the file. The first track   //
    //             is numbered 0. If n is greater than the number of    //
    //             tracks minus one, this method returns NULL.          //
    //////////////////////////////////////////////////////////////////////
    MFtrack            *getTrack(int n);

    //////////////////////////////////////////////////////////////////////
    // void addTrack(MFtrack *track);                                   //
    //   requires: nothing                                              //
    //   modifies: _tracks                                              //
    //   effects:  Adds 'track' to the end of the list of tracks. If    //
    //             'track' is NULL, this method does nothing.           //
    //   returns:  nothing                                              //
    //////////////////////////////////////////////////////////////////////
    void               addTrack(MFtrack *track);

    //////////////////////////////////////////////////////////////////////
    // void deleteTrack(int n);                                         //
    //   requires: nothing                                              //
    //   modifies: _tracks                                              //
    //   effects:  Removes the Nth track from the file. The first track //
    //             is numbered 0. If n is greater than the number of    //
    //             tracks minus one, this method does nothing.          //
    //   returns:  nothing                                              //
    //////////////////////////////////////////////////////////////////////
    void               deleteTrack(int n);

    //////////////////////////////////////////////////////////////////////
    // MFfileEvent **nextEvent(int *nevents);                           //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  nothing                                              //
    //   returns:  Looks at all of the tracks in the file and finds the //
    //             lowest time stamp associated with the seek pointers  //
    //             for each track. If returns an array of 'nevents'     //
    //             pointers to MFfileEvents of all the events from all  //
    //             tracks with this next lowest time stamp.             //
    //////////////////////////////////////////////////////////////////////
    MFfileEvent        **nextEvent(int *nevents);
    MFfileEvent        *nextEvent();

    //////////////////////////////////////////////////////////////////////
    // void print(void);                                                //
    //   requires: nothing                                              //
    //   modifies: nothing                                              //
    //   effects:  Prints a summary of the file to stderr.              //
    //   returns:  nothing                                              //
    //////////////////////////////////////////////////////////////////////
    void               print(void);

  private:
    MFtrack            **_tracks;    // The list of tracks in the file
    int                _ntracks;     // The number of tracks in the list

    unsigned long      _hdrlength;   // length of the header, always 6
    int                _format;      // format (0, 1, or 2)
    int                _division;    // ticks per quarter-note

    int                _toread;	     // to read in current chunk 
    FILE               *_fp;         // The pointer to the file


    /////////////////////////////////////////////////////////////////////
    // Internal Routines to Read MIDI Disk Files                       //
    /////////////////////////////////////////////////////////////////////
    int                readtrack(MFtrack *);
    int                readfilehdr();
    int                readtrackhdr();
    int                readchar();
    int                read32();
    int                read16();
    int                readvar();
    int                readhd(char *,int);
    MDevent            *readsysexevent(unsigned long long);
    MFmeta             *readmetaevent(unsigned long long);
    MDevent            *readmidievent(int, int *, unsigned long long);

    /////////////////////////////////////////////////////////////////////
    // Internal Routines to Write MIDI Disk Files                      //
    /////////////////////////////////////////////////////////////////////
    int                writetrack(int); 
    void               write32(int);
    void               write16(int);
    int                writevar(int);
    int                writetime(int);
    int                writehd(char *looking);
    void               writefilehdr();
    int                writeevent(MFfileEvent *, unsigned long long *time);
    int                writemidievent(MDevent *, unsigned long long *time);
    int                writemetaevent(MFmeta *, unsigned long long *time);
    unsigned long      writetrackhdr();
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

// Function Predeclarations

////////////////////////////////////////////////////////////////////////////
// int mfIsSystemCommon(MFfileEvent *fev);                                //
//    Returns True if fev is a MIDI System Common event, otherwise False. //
////////////////////////////////////////////////////////////////////////////
int      mfIsSystemCommon(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfIsSystemRealTime(MFfileEvent *fev);                              //
//    Returns True if fev is a MIDI System RealTime event, otherwise      //
//    False.                                                              //
////////////////////////////////////////////////////////////////////////////
int      mfIsSystemRealTime(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfIsSystemExclusive(MFfileEvent *fev);                             //
//    Returns True if fev is a MIDI System Exclusive event, otherwise     //
//    False.                                                              //
////////////////////////////////////////////////////////////////////////////
int      mfIsSystemExclusive(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfIsChannelVoice(MFfileEvent *fev);                                //
//    Returns True if fev is a MIDI Channel Voice event, otherwise False. //
////////////////////////////////////////////////////////////////////////////
int      mfIsChannelVoice(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfIsChannelMode(MFfileEvent *fev);                                 //
//    Returns True if fev is a MIDI Channel Mode event, otherwise False.  //
////////////////////////////////////////////////////////////////////////////
int      mfIsChannelMode(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfIsMeta(MFfileEvent *fev);                                        //
//    Returns True if fev is a Meta event, otherwise False.               //
////////////////////////////////////////////////////////////////////////////
int      mfIsMeta(MFfileEvent *ev);

////////////////////////////////////////////////////////////////////////////
// int mfMessageType(MFfileEvent *fev);                                   //
//    Returns the type of event this is, either: MD_CHANNELVOICE,         //
//    MD_CHANNELMODESELECT, MD_SYSTEMCOMMON, MD_SYSTEMREALTIME,           //
//    MD_SYSEX, or MD_META.                                               //
////////////////////////////////////////////////////////////////////////////
int      mfMessageType(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfChannelVoiceType(MFfileEvent *fev);                              //
//    Returns the type of Channel Voice message fev is. If fev is not a   //
//    Channel Voice message, returns -1.                                  //
////////////////////////////////////////////////////////////////////////////
int      mfChannelVoiceType(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfChannelModeType(MFfileEvent *fev);                               //
//    Returns the type of Channel Mode message fev is. If fev is not a    //
//    Channel Mode message, returns -1.                                   //
////////////////////////////////////////////////////////////////////////////
int      mfChannelModeType(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfSystemCommonType(MFfileEvent *fev);                              //
//    Returns the type of System Common message fev is. If fev is not a   //
//    System Common message, returns -1.                                  //
////////////////////////////////////////////////////////////////////////////
int      mfSystemCommonType(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfSystemRealTimeType(MFfileEvent *fev);                            //
//    Returns the type of System RealTime message fev is. If fev is not a //
//    System RealTime message, returns -1.                                //
////////////////////////////////////////////////////////////////////////////
int      mfSystemRealTimeType(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfMetaType(MFfileEvent *fev);                                      //
//    Returns the type of Meta Message fev is. If fev is not a Meta       //
//    message, returns -1.                                                //
////////////////////////////////////////////////////////////////////////////
int      mfMetaType(MFfileEvent *fev);

////////////////////////////////////////////////////////////////////////////
// int mfEncodeEvent(MFfileEvent *fev);                                   //
//    Returns a string which represents a string representation of fev.   //
////////////////////////////////////////////////////////////////////////////
char     *mfEncodeEvent(MFfileEvent *fev);

#endif    // _LIBMIDIFILE_H
