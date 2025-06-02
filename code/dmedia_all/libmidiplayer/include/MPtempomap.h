/************************************************************************\
 *	File:		MFtempomap.h					* 
 *									*
 *	Contains a class which manages tempo information for a 		*
 *	particular midifile.  The tempo information allows us 		*
 *	to perform precise time calculations.				*
 *	While this class contains some code to make it work well	*
 *	with SGI's MFtrack class, it can conceivably be used in		*
 *	any application which needs to maintain tempo map info.		*
 *									*
\************************************************************************/

#ifndef MFTEMPOMAP_H
#define MFTEMPOMAP_H

class MidiTrack;
struct _TempoEntry;

class TempoMap {
  public:
    TempoMap(int division);
    ~TempoMap();

    int 		addAllTempos(MidiTrack*);
    int			addTempo(MidiEvent*);
    int			addTempo(long long tick, int usecs);
    int			removeTempo(long long tick);

    int			numTempos(void);
    long long		nthTempoTick(int n);
    long long 		nthTempoTime(int n);
    int			nthTempo(int n);

    int			firstTempo(void);
    int			tempoAtTick(long long tick);
    int			tempoAtTime(long long time);
    
    long long		timeToTicks(long long time);
    long long		ticksToTime(long long ticks);

  private:
    _TempoEntry	       *_tempoMap;
    _TempoEntry        *_defaultEntry;
    int			_numTempos;
    int			_numAllocated;
    int			_division;

    // Private methods
    int			_addTempo(long long tick, int usecs);
    void		_updateTimes(void);
    _TempoEntry*	_entryAtTime(long long time);
    _TempoEntry*	_entryAtTick(long long tick);
};


#endif // MFTEMPOMAP_H
