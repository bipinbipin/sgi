///////////////////////////////////////////////////////////////////////////
// File: midifile.h                                                      //
// Silicon Graphics, Inc.                                                //
///////////////////////////////////////////////////////////////////////////

#ifndef _MIDIFILE_H
#define _MIDIFILE_H

// Standard Include Files
#include <stdio.h>
#include <sys/types.h>

// Digital Media Include Files
#include <dmedia/midi.h>

// Specific Include Files
class LinkedList;
class MFtrack;
class MFmeta;
class MFfileEvent;

class MFfile {
  public: 
    MFfile();
    ~MFfile();

    // Methods to access the MIDI Header Information
    int format();
    int ntracks();
    int division();

    // Methods to set the MIDI Header Information. Note that
    // this does not affect the way the class writes the files,
    // so the user is responsible for making sure the information
    // is correct.
    void setformat(int);
    void setdivision(int);

    // Methods to do the actual file I/O
    int                load(char *name);
    int                save(char *name);
    void               rewind();
    void               seek(unsigned long long);
    unsigned long long tell();

    // Methods to manage the tracks within the file
    MFtrack *gettrack(int n);
    void    addtrack(MFtrack *);
    void    deletetrack(int);

    // Methods to access MIDI data as a single stream
    MFfileEvent **nextevent(int *nevents);

    void print();

  private:
    LinkedList *_tracks;

    // MIDI File Header Information:
    //   The length is always "6" and the number of tracks are
    //   determined by the length using the linkedlist utility
    unsigned long _hdrlength;  // length of the header
    int           _format;     // format (0, 1, or 2)
    int           _division;   /// quarter-note divisions


    int toread_;	// number of bytes to read in current chunk 

    // The File Pointer to the file
    FILE *fp_;

    int     readtrack(MFtrack *);
    int     readfilehdr();
    int     readtrackhdr();
    int     readchar();
    int     read32();
    int     read16();
    int     readvar();
    int     readhd(char *,int);
    MDevent *readsysexevent(unsigned long long);
    MFmeta  *readmetaevent(unsigned long long);
    MDevent *readmidievent(int, int *, unsigned long long);

    int           writetrack(int); 
    void          write32(int);
    void          write16(int);
    int           writevar(int);
    int           writetime(int);
    int           writehd(char *looking);
    void          writefilehdr();
    int           writeevent(MFfileEvent *, unsigned long long *time);
    int           writemidievent(MDevent *, unsigned long long *time);
    int           writemetaevent(MFmeta *, unsigned long long *time);
    unsigned long writetrackhdr();
};

#endif
