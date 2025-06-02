/* **************************************************************
 *    Chris Pirazzi, Scott Porter, Gints Klimanis, Doug Cook
 *				1991-4
 * ************************************************************** */
#ifndef _sfinclude_H
#define _sfinclude_H

#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include <audio.h>
#include <audiofile.h>
#include <audioutil.h>

#define UNSPECIFIED_FILE_FORMAT	    -1
#define UNSPECIFIED_FILE_FORMAT_VERSION -1

#define UNSPECIFIED_CHANNEL_COUNT   -1
#define UNSPECIFIED_FRAME_COUNT	    -1
#define UNSPECIFIED_SAMPLE_FORMAT   -1
#define UNSPECIFIED_SAMPLE_WIDTH    -1
#define UNSPECIFIED_SAMPLING_RATE   -1

#define UNSPECIFIED_BYTE_ORDER	    -1
#define UNSPECIFIED_COMPRESSION	    -2
#define UNSPECIFIED_FRAME_SIZE	    -1

/* Basic Types and Macros ---------------------------------------- */
/* command line options */
typedef struct {
    char    *applicationName;	    /* tail part of argv[0] */
    int	    applicationID;	   
    char    *applicationUsage;	    /* command line usage string */
    char    verbose;		    /* verbose operation flag */

    char    printFormat;
    char    doNotRudelyChangeSamplingRate;
	char    rawFormatSpecified;
	char    matchPortSampleRate;
    double  fileHeaderSamplingRate;
    int	    device;		    /* device to which to send audio */

/* sampling rate conversion parameters */
    int    transitionBandWidth;    
    int    stopBandAttenuation;    

    char    *matrixFile;            /* NULL==not specified */
} Options;

#define MAX_AWARE_OPTS 5
typedef struct {
    int    layer;
    int    defaultConfiguration;

    int    bitRateTarget;
    int    channelPolicy;
    int    bitRatePolicy;
    double  constantQualityNoiseToMaskRatio;

    char    bitRateTargetSpecified; 
    char    channelPolicySpecified;  
    char    bitRatePolicySpecified; 
    char    constantQualityNoiseToMaskRatioSpecified;   
} Aware;

/* audio track in disk file */
typedef struct {
    int	    dataOffset;
    int	    dataSize;

    int	    byteOrder;
    int	    channelCount;	
    int	    frameCount;		/* total # frames */
    char	    frameSizeSpecified;
    float	    frameSizeInBytes;
    int	     	    sampleFormat;	
    int	 	    sampleWidth;	
    int		    bytesPerSample;
    double	    samplingRate;	

    char	    pcmMapSpecified;
    char	    pcmMapSlopeSpecified;
    char	    pcmMapInterceptSpecified;
    char	    pcmMapMinClipSpecified;
    char	    pcmMapMaxClipSpecified;

    double	    pcmMapSlope;
    double	    pcmMapIntercept;
    double	    pcmMapMinClip;
    double	    pcmMapMaxClip;

/* Compression algorithm specific information */
    int  	    compressionType;	
    char	    *compressionName;	
    Aware	    aware;		/* aware compression parameters */

/* AIFF and AIFF/C specific information */
    int		    markerCount;	
    int 	    *markerIDs;		/* list of marker ids */
    int		    *markerPositions;	/* list of marker positions */
    char	    **markerNames;	/* list of marker name strings */

    char	    aesDataSeen;	
    unsigned char   aesData[24];	/*  */
} Track;

#define LOW    0
#define HIGH   1
#define BASE   2

#define MAX_LOOPS	2
#define LOOP_SUSTAIN	0
#define LOOP_RELEASE	1

/* sampler configuration parameters (some redundancy) */
typedef struct {
    int midiKeyboardMap[3];	    /* keyboard map (LOW, HIGH, BASE)*/
    int midiVelocity[2];	    /* velocity map (LOW, HIGH) */
    int gainInDecibels;	    /* # decibels of playback gain */
    int detuneInCents;		    /* # cents playback detune */

    int loopCount;                
    int loopIDs[MAX_LOOPS];                 
    int loopMode[MAX_LOOPS];               
    int loopStartMarkerID[MAX_LOOPS];      
    int loopEndMarkerID[MAX_LOOPS];       
    int loopStartFrame[MAX_LOOPS];
    int loopEndFrame[MAX_LOOPS];
} Instrument;

/* "miscellaneous data chunk" in audio file */
typedef struct {
    int    type;                
    int    size;                 
    char    *text;              
} Miscellaneous;

/* describes sound file */
typedef struct {
    int		    fd;			    /* unix file descriptor */
    char	    *name;		    /* file name */
    
    int		    modeCode;		    
    char	    modeString[10];	    /* file open/create mode */
    AFfilehandle    handle;		    
    AFfilesetup	    setUp;		       

    int	    formatID;	
    char	    *formatLabel;	    
    char	    *formatName;	    
    int	   	    formatVersion;	    /* for AIFF-C format only */
    char	    isAIFForC;		    /* is AIFF or AIFF-C ? */
    char	    mayHaveExtras;	    /* more general flag */
    char	    mayHaveMiscs;	    /* more general flag */

    int	    trackCount;		    /* currently only ONE track */
    int	    trackID;		    /* not used */
    Track	    *track;		    /* audio track description */

    int	    instrumentCount;	    /* currently only ONE instrument */
    int	    *instrumentIDs;
    Instrument	    **instruments;	    /* sampler data description */

    int	    miscellaneousCount;	    /* # data chunks */
    int	    *miscellaneousIDs;
    Miscellaneous   **miscellaneous;	    /* data chunks descriptions */

/* sampling rate conversion parameters */
    double  passBandRipple;	    
} SoundFile;

#ifndef _BOOL
typedef char bool;
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define max(a,b) ( ((a)>(b)) ? (a) : (b))
#define min(a,b) ( ((a)<(b)) ? (a) : (b))

#define SRATE_CODEC		(8012.8210513)
#define ERRVAL 1		/* error exiting program */

/* types of print formats */
#define SF_NONE		0
#define SF_SHORT	1
#define	SF_NORMAL	2
#define SF_LONG		3

#define AIFF_TO_AIFC	0
#define AIFC_TO_AIFF	1
#define AIFC_DECOMPRESS 2
#define AIFC_COMPRESS	3
#define AIFC_RESAMPLE	4
#define SF_CONVERT	5

/* Common Soundfile Function Prototypes ------------------------------- */
void SFError(char *format, ...);
void QuietAFError   (long error, const char *description);
void QuietALError   (long error, const char *description, ...);
void LoudAFError    (long error, const char *description);
void LoudALError    (long error, const char *description, ...);
void DefaultAFError (long code, const char *description);
void DefaultALError (long code, const char *description, ...);

char *GetPathTail(char *path);
char *GetPathLead(char *path);
char Numeric	 (char *s);

void PrintFormats	( void );
void PrintCompressions	( void );
bool Empty		( char *string );
bool MatchArg		( char *userString, char *targetString, int minimumMatch );
void DataFormatName	(AFfilehandle handle, int track, char *outBuffer, char printFormat);
void DataFormatExtras	(Track *track, char *out, char printFormat);
char *FindCopyright	( AFfilehandle handle );
void DurationInSeconds	(double seconds, char *out, char printFormat);
void SizeInBytes	(int totalBytes, char *out, char printFormat);
void RateInHertz	(double rate,    char *out, char printFormat);

int ParseKeywords(char **arg, SoundFile *file, char **matrixFile);
bool ReadChannelMatrixFile(char *fileName, int vchans, int fchans, double **ret);

void SetUpOutSoundFile		    (AFfilesetup setUp, SoundFile *inFile, SoundFile *outFile, int applicationID);
void SetUpTrackParameters	    (AFfilesetup setUp, int id, Track *inTrack, SoundFile *outFile, int applicationID);
void SetUpTrackAIFFParameters	    (AFfilesetup setUp, int id, Track *track);
void SetUpInstrumentAIFFParameters  (AFfilesetup setUp, int id, Instrument *data);
void SetUpMiscellaneousData	    (AFfilesetup setUp, int id, Miscellaneous *data);

void PrintTrackParameters	(SoundFile *file, int track, char printFormat);
void PrintTrackAIFFParameters	(Track *data);
void PrintInstrumentAIFFParameters(Instrument *data);
void PrintMiscellaneousData	(Miscellaneous *data);
void PrintSoundFileParameters	(SoundFile *file, char printFormat);
void PrintSoundFileHandle	( AFfilehandle handle );

void BasicMiscellaneous	(Miscellaneous *data);
void BasicInstrument	(Instrument *data);
void BasicTrack		(Track *track);
void BasicSoundFile	(SoundFile *file);

Miscellaneous *NewMiscellaneous	(Miscellaneous	*model);
Instrument *NewInstrument	(Instrument	*model);
Track *NewTrack			(Track		*model);
SoundFile *NewSoundFile		(SoundFile	*model);

int OpenInSoundFile (SoundFile *file);
int OpenOutSoundFile(SoundFile *outFile, SoundFile *modelFile, char *matrixFile /* NULL if none */);

char CopyTrackAudioData(AFfilehandle inHandle, AFfilehandle outHandle, 
			    int id, int framesToTransfer, 
			    double fileHeaderSamplingRate, char verbose);
char CopyTrackAIFFParameters(Track *inTrack, Track *outTrack);

char ReadSoundFileParameters	    (AFfilehandle handle, SoundFile *file);
void ReadTrackParameters	    (AFfilehandle handle, int id, Track *data, int formatID);
char ReadTrackAIFFParameters	    (AFfilehandle handle, int id, Track *data);
void ReadInstrumentAIFFParameters   (AFfilehandle handle, int trackID, int id, Instrument *data);
char ReadMiscellaneousData	    (AFfilehandle handle, int id, Miscellaneous *data);

void WriteSoundFileParameters	    (AFfilehandle handle, SoundFile *file);
void WriteTrackParameters	    (AFfilehandle handle, int id, Track *data);
void WriteTrackAIFFParameters	    (AFfilehandle handle, int id, Track *data);
void WriteInstrumentAIFFParameters  (AFfilehandle handle, int id, Instrument *data);
char WriteMiscellaneousData	    (AFfilehandle handle, int id, Miscellaneous *data);

void CullMiscellaneousAIFFData	(SoundFile *file);
char IsSoundFile		(char *path);
char *StringDuplicate		(char *s);
char CloseSoundFile		(SoundFile *file);
char CleanUpSoundFile		(SoundFile *file);

void PrintError		(Options *options, const char *t);
void CatchAFError	(long code, const char *s);
void InitErrorReporting	(Options *options);
void OnSignalInterrupt	();

int PlaySoundFile(char *audioPortName,  int device,
			AFfilehandle fileHandle, int track, 
			bool background, bool doNotRudelyChangeSamplingRate, 
			bool matchPortSampleRate, int printFormat);

extern char *keywordUsage;	/* keyword options */

char CheckAwareEncoderLicense(SoundFile *file, char verbose);
char CheckAwareDecoderLicense(SoundFile *file, char verbose);

/* Debugging stuff ------------------------------------------------ */

#ifdef SF_DEBUG /* with debugging */

#include <sys/types.h>
#include <malloc.h>
#define MALLOPT() mallopt(M_DEBUG,1)
#include <math.h>
#include <stdlib.h>
#define ZAP() free(malloc((size_t)(random()%5763)))
#define DEBG(statement) { if (getenv("DEBG")||getenv("CHNK")) {statement;} }

struct _AFfilesetup;
struct _AFfilehandle;
typedef struct _AFfilesetup  *filesetup2[2];
typedef struct _AFfilehandle *filehandle2[2];

void printsetup	(struct _AFfilesetup *s);
void diffsetup	(filesetup2 s);
void printhandle(struct _AFfilehandle *h);
void diffhandle	(filehandle2 h);
void showsetup	(struct _AFfilesetup *s);
void showhandle	(struct _AFfilehandle *h);

#else /* no debugging */

#define MALLOPT()
#define ZAP()
#define DEBG(x)

#endif

#endif
