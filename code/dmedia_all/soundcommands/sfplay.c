/*****************************************************************************
 *
 *     4Dgifts 'playaiff' program ported to IRIS Audio File Library
 *     scott porter, chris pirazzi, gints klimanis, 1991-4
 *
 * notes:
 *     this program plays AIFF, AIFF-C, NeXTSND, wave, raw data
 *     real-time G.722 / G.711 decompression for AIFF-C via audio file 
 *        library
 *     G.722 real-time decompression works best if this program is run
 *         with nondegrading high priority (npri -h), especially for 
 *         stereo data
 *     this program now plays back AIFF-C files which contain audio data 
 *         encoded with Aware MultiRate or MPEG compression algorithms
 *     this program now handles 4-channel AIFF/C files 
 *
 * usage:
 *     sfplay  [options] filename(s)
 *     playaifc [options] filename(s)
 *     playaiff [options] filename(s)
 *
 * command line options:
 *     q = quiet:    suppress file descriptions, warning and error messages
 *     v = verbose:  print additional information
 *     r = rude:     modify global IRIS Audio Processor output rate as 
 *                   necessary in order to play files
 *     p = polite:   don't be rude - see 'nonrude' below
 *
 * defaults:
 *  if invoked as playaiff:
 *	    assumes 'rude' behavior, in order to maintain backward compatibility
 *	    with earlier releases
 *  if invoked as playaifc or sfplay:
 *	    defaults to 'nonrude' behavior
 *
 *    'nonrude' behavior is defined as follows: before modifying global
 *        output sample rate in the IRIS Audio Processor, first check to
 *        whether any other output ports are currently active; if any other
 *        processes have open output ports, don't modify the output rate
 *    
 *    playaifc/playaiff emit one line descriptions for each file during play.
 *    These descriptions are turned off by the '-q' option.
 *
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <audio.h>
#include "audiofile.h"
#include "sf.h"

#ifdef SF_DEBUG
#include <malloc.h>
#endif

#define SHUFFLE_N_LOOP 

extern char *applicationName;
extern long lastAFError;
extern char reportAFError;
extern char sfPrint;
extern float forceTransposeRate;
extern float fileSkipTime;
extern float filePlayTime;

extern void
SFPrint(char *format, ...);

static void
ParseSfPlayCommandLine(int argc, char **argv, char *filePlayList[], int *fileCount,
SoundFile *file, int *loopCount, bool *shuffle, Options *options);
static void
ParsePlayAIFCCommandLine(int argc, char **argv, char *filePlayList[], int *fileCount,
Options *options);

extern char	*optarg;
extern int	optind;

static void SeizeHighExecutionPriority(bool verbose);

char *sfplayApplicationUsage = 
"Usage: sfplay [Options] soundfile(s)\n"
"Options:\n"
"    -help\n"
"    -inputraw [Input Keywords ...] end\n"
"                    files are raw data (note 'end')\n"
"                    'man sfkeywords(1)' for more info\n"
"    -transpose <rate>\n"
"                    transpose all files to play at <rate>\n"
"    -match          rate-convert file to match port rate\n"
"    -quiet          do not print info as files are played\n"
"    -printinfo      print info as files are played\n"
"    -noreporterror  do not report errors\n"
"    -reporterror    report errors\n"
"    -skip <time>    skip <time> seconds into file before playing\n"
"    -dur <time>     play <time> seconds of the file\n"
"    -device	     device to which to send audio\n"
#ifdef SAFE
#ifdef SHUFFLE_N_LOOP
"    -shuffle        randomly reorder file list\n"
"    -loop l         repeat file list l times (-1=forever)\n"
#endif
#endif
"While audio hardware in use by other processes:\n"
"    -rude           change output sampling rate anyway\n"
"    -nice           don't change output sampling rate\n";


char *playaifcApplicationUsage = 
"Usage: playaifc [-hqvrpd] soundfile(s)\n"
"    -h\n"
"    -q		suppress normal info printed\n"
"    -v		print information to stderr (implies NOT quiet)\n"
"    -d		device to which to send audio\n"
"While audio hardware in use by other processes:\n"
"    -r		change output sampling rate anyway\n"
"    -p		don't change output sampling rate\n";



#ifdef SHUFFLE_N_LOOP
static void ShufflePlayList(char *inList[], char *outList[], int count, unsigned int *element);
#endif

void
SetDevice(Options *options, char *name)
{
    /*
     * -device option:
     * See if the given name specifies a valid device.
     */
    options->device = alGetResourceByName(AL_SYSTEM, name, AL_DEVICE_TYPE);
    if (options->device) {
	int itf;

	/*
	 * See if the given name also specifies an interface on
	 * the device. If so, select that interface.
	 */
	itf = alGetResourceByName(AL_SYSTEM, name, AL_INTERFACE_TYPE);
	if (itf) {
	    ALpv pv;

	    pv.param = AL_INTERFACE;
	    pv.value.i = itf;
	    alSetParams(options->device, &pv, 1);
	}
    }
    else {
	fprintf(stderr, "Cannot find device: %s\n", name);
	exit(-1);
    }
}
		

/* ******************************************************************
 * main()	
 * ****************************************************************** */
int
main( int argc, char **argv)
{
    int		i;
    SoundFile	file;
    Options		options;

#ifdef SHUFFLE_N_LOOP
    struct timeval	    time;
    struct timezone	    timeZone;
    unsigned int	    element;	/* for random number generator */
#endif

    char		**filePlayList;
    bool		shuffle;
    int		fileCount, loopCount;
    int		fileNameIndex;

    MALLOPT();

    /* swap effective & real userIDs.
 * this allows us to become super-user when we wish, but 
 * usually run as "joe user" */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());

    /* extract application name from path */
    applicationName = GetPathTail(argv[0]);

    /* universal application defaults */
    options.doNotRudelyChangeSamplingRate = FALSE;
	options.matchPortSampleRate = FALSE;
    options.rawFormatSpecified = FALSE;
    options.verbose = FALSE;

    /*
     * In the absence of any other information, use the global
     * AL default output device
     */
    options.device = AL_DEFAULT_OUTPUT;

    ALseterrorhandler(0);

    /* 
   change behavior according to context.  
   If launched by typing, print messages and report errors. 
   If launched from within application, do not print messages or report errors.
*/
    if (isatty(0))
    {
	/* report errors by default */
	reportAFError = TRUE;
	afSetErrorHandler(LoudAFError);
	/* print messages by default */
	options.printFormat = SF_SHORT;
    }
    else
    {
	/* do not report errors by default */
	reportAFError = FALSE;
	afSetErrorHandler(QuietAFError);
	/* do not print messages by default */
	options.printFormat = SF_NONE;
    }
    sfPrint = options.printFormat;

    /* Create file play list array, primed for shuffle play mode */
    filePlayList = (char **) malloc(argc*sizeof(char *));
    for (i = 0; i < argc; i++)
	filePlayList[i] = NULL;

    loopCount = 1;
#ifdef SHUFFLE_N_LOOP
    shuffle = FALSE;
#endif

    /* default raw data format */
    BasicSoundFile(&file);

    /*
 * command line parsing for playaifc & playaiff 
 */
    if (!strcmp(applicationName, "playaifc") || !strcmp(applicationName, "playaiff"))
    {
	/* for no command line options, print usage */
	if (1 == argc)
	{
	    fprintf(stderr, playaifcApplicationUsage);
	    exit(1);
	}

	/* Backward compatibility for playaiff:
	    1) will not rudely change sampling rate
	    2) does not print info statement to console by default */
	if ('f' == applicationName[7])
	{
	    options.doNotRudelyChangeSamplingRate = TRUE;
	    options.printFormat = SF_NONE;
	}
	ParsePlayAIFCCommandLine(argc, argv, filePlayList, &fileCount, &options);
    }

    /*
 * command line parsing for sfplay 
 */
    else 
    {
	/* for no command line options, print usage */
	if (argc == 1)
	{
	    fprintf(stderr, sfplayApplicationUsage);
	    exit(ERRVAL);
	}

	ParseSfPlayCommandLine(argc, argv, filePlayList, &fileCount,
	    &file, &loopCount, &shuffle, &options);
    }
    sfPrint = options.printFormat;

#ifdef SHUFFLE_N_LOOP
    /* seed random # generator w/system clock */
    if (shuffle && fileCount > 1)
    {
	gettimeofday(&time, &timeZone);
	element = time.tv_usec;
    }
#endif

    /* 
 * ---------------- sound file list 'loopCount' times
 */
    SeizeHighExecutionPriority(options.verbose);
    /* if loopCount negative, loop infinitely */
    for (i = 0; i != loopCount; i++)
    {
	/* print loop count */
	if (options.printFormat != SF_NONE && loopCount != 1)
	{
	    if (loopCount > 0)
		SFPrint("Loop %d/%d", i+1, loopCount);
	    else
		SFPrint("Loop %d", i+1);
	}

#ifdef SHUFFLE_N_LOOP
	/* shuffle play list each loop iteration */
	if (shuffle && fileCount > 1)
	    ShufflePlayList(filePlayList, filePlayList, fileCount, &element);
#endif

	/* play complete list once */
	for (fileNameIndex = 0; filePlayList[fileNameIndex] ; fileNameIndex++)
	{
	    file.name = filePlayList[fileNameIndex];

	    strcpy(file.modeString, "r");
	    file.modeCode = O_RDONLY;
	    if (OpenInSoundFile(&file))
	    {
		file.trackCount = afGetTrackIDs(file.handle, NULL);
		afGetTrackIDs(file.handle, &file.trackID);
		ReadTrackParameters(file.handle, file.trackID, file.track, file.formatID);
		if (options.printFormat != SF_NONE)
		    PrintSoundFileParameters(&file, options.printFormat);

		PlaySoundFile(applicationName, options.device, file.handle, file.trackID, 
		    FALSE, options.doNotRudelyChangeSamplingRate,
			options.matchPortSampleRate,
		    options.printFormat);
		if(!options.rawFormatSpecified) {  /* don't reset if -inputraw used */
			CleanUpSoundFile(&file);
			BasicSoundFile(&file);
		}
		else {
			CloseSoundFile(&file);
		}
	    }

	    else if (AF_BAD_OPEN == lastAFError)
		SFError("error opening file %s", file.name);
	}
    }

    ZAP();

    return 0;
} /*---- end main() ---- */

/* **********************************************************************
 * ParsePlayAIFCCommandLine:	parse input command line for user options  
 * ********************************************************************** */
static void
ParsePlayAIFCCommandLine(int argc, char **argv, char *filePlayList[], 
int *fileCount, Options *options)
{
    int		c;
    extern char	*optarg;
    extern int	optind;
    int		fd;
    char		*fileName;

    /* extract options "-qvrph" */
    while ((c = getopt(argc, argv, "qvrphd:")) != -1)
    {
	switch(c)
	{
	case 'q':
	    options->printFormat = SF_NONE;
	    break;
	case 'v':
	    options->verbose = TRUE;
	    break;
	case 'r':
	    options->doNotRudelyChangeSamplingRate = FALSE;
	    break;
	case 'p':
	    options->doNotRudelyChangeSamplingRate = TRUE;
	    break;
	case 'd': 
	    SetDevice(options, optarg);
	    break;
	case 'h':
	default:
	    fprintf(stderr, playaifcApplicationUsage);
	    exit(1);
	    break;
	}
    }

    /* 
   for playaif? we sort of munge together the printing and
   error settings
*/
    if (options->verbose)
	options->printFormat = SF_SHORT;

    if (SF_NONE == options->printFormat)
    {
	reportAFError = FALSE;
	afSetErrorHandler(QuietAFError);
    }
    else 
    {
	reportAFError = TRUE;
	afSetErrorHandler(DefaultAFError);
    }
    sfPrint = options->printFormat;

    /* if no options left, no files found */
    if (argc - optind < 1)
    {
	SFError("Well, supply some audio files !!!");
	exit(1);
    }

    /* 
 * create file play list 
 */
    *fileCount = 0;
    while (optind < argc)
    {
	fileName = argv[optind];

	/* obtain unix file descriptor for input file */
	if ((fd = open(fileName, O_RDONLY)) < 0)
	{
	    SFError("failed to open file '%s' %s", fileName, strerror(errno));
	}

	/* test file descriptor to see whether we can attach an audio
 *    file handle to it 
 */
	else if (afIdentifyFD(fd) < 0)
	{
	    /* if error messages are not to be supressed, print message */
	    if (options->printFormat != SF_NONE)
		SFError("'%s' not an AIFF-C or AIFF file", fileName);
	}

	/* attach audio file handle to file descriptor */
	else if (AF_NULL_FILEHANDLE == afOpenFD(fd, "r", AF_NULL_FILESETUP))
	{
	    if (options->printFormat != SF_NONE && reportAFError)
		SFError("failed to open file '%s'", fileName);
	}

	/* append filenames to file list */
	else 
	{
	    filePlayList[(*fileCount)++] = argv[optind];
	}

	optind++;
    }
}   /* ---- end ParsePlayAIFCCommandLine() ---- */

/* **********************************************************************
 * ParseSfPlayCommandLine:	parse input command line for user options  
 * ********************************************************************** */
static void
ParseSfPlayCommandLine(int argc, char **argv, char *filePlayList[], int *fileCount,
SoundFile *file, int *loopCount, 
bool *shuffle, Options *options)
{
    int	    i;
    int	    keyWordsFound;

    *fileCount = 0;
    for (i = 1; i < argc; i++)
    {
	/* check for -help */
	if (MatchArg(argv[i], "-help", 2))
	{
	    fprintf(stderr, sfplayApplicationUsage);
	    exit(ERRVAL);
	}

	/* check for -inputraw */
	else if (MatchArg(argv[i], "-inputraw", 2))
	{
	    if (++i >= argc)
	    {
		fprintf(stderr, sfplayApplicationUsage);
		exit(ERRVAL);
	    }

	    file->formatID = AF_FILE_RAWDATA;
	    keyWordsFound = ParseKeywords(&argv[i], file, NULL);
	    if (0 >= keyWordsFound)
		exit(ERRVAL);
	    i += keyWordsFound;
		options->rawFormatSpecified = TRUE;
	}

	/* check for -nice */
	else if (MatchArg(argv[i], "-nice", 3))
	    options->doNotRudelyChangeSamplingRate = TRUE;

	    /* check for -printinfo */
	else if (MatchArg(argv[i], "-printinfo", 2))
	    options->printFormat = SF_SHORT;

	    /* check for -quiet */
	else if (MatchArg(argv[i], "-quiet", 2))
	    options->printFormat = SF_NONE;

	    /* check for -noreporterror  */
	else if (MatchArg(argv[i], "-noreporterror", 3))
	{
	    afSetErrorHandler(QuietAFError);
	    reportAFError = FALSE;
	}

	/* check for -reporterror  */
	else if (MatchArg(argv[i], "-reporterror", 3))
	{
	    afSetErrorHandler(LoudAFError);
	    reportAFError = TRUE;
	}
	
	/* check for -device  */
	else if (MatchArg(argv[i], "-device", 1))
	{
	    if (++i >= argc)
	    {
		fprintf(stderr, sfplayApplicationUsage);
		exit(ERRVAL);
	    }
	    SetDevice(options, argv[i]);
	}

	/* check for -rude */
	else if (MatchArg(argv[i], "-rude", 3))
	    options->doNotRudelyChangeSamplingRate = FALSE;

	/* check for -transpose */
	else if (MatchArg(argv[i], "-transpose", 2))
	{
	    if (++i >= argc)
	    {
		fprintf(stderr, sfplayApplicationUsage);
		exit(ERRVAL);
	    }
	    forceTransposeRate = atof(argv[i]);
	    options->doNotRudelyChangeSamplingRate = FALSE; /* also rude */
	}
	/* check for -match */
	else if (MatchArg(argv[i], "-match", 3)) {
	    options->matchPortSampleRate = TRUE;
	    options->doNotRudelyChangeSamplingRate = TRUE; /* also polite */
	}
	else if (MatchArg(argv[i], "-skip", 2)) {
	    if (++i >= argc) {
		fprintf(stderr, sfplayApplicationUsage);
		exit(ERRVAL);
	    }
	    fileSkipTime = atof(argv[i]);
	}
	else if (MatchArg(argv[i], "-dur", 3)) {
	    if (++i >= argc) {
		fprintf(stderr, sfplayApplicationUsage);
		exit(ERRVAL);
	    }
	    filePlayTime = atof(argv[i]);
	}


#ifdef SHUFFLE_N_LOOP
	/* check for -loop */
else if (MatchArg(argv[i], "-loop", 2))
{
    i++;
    if (i >= argc)
    {
	fprintf(stderr, sfplayApplicationUsage);
	exit(ERRVAL);
    }

    *loopCount = atoi(argv[i]);
    if (0 == *loopCount)
    {
	SFError("Invalid loop count %s: %s", argv[i-1], argv[i]);
	exit(ERRVAL);
    }
}

/* check for -shuffle */
else if (MatchArg(argv[i], "-shuffle", 2))
*shuffle = TRUE;
#endif

/* check for invalid options */
else if ('-' == argv[i][0])
{
    fprintf(stderr, sfplayApplicationUsage);
    exit(ERRVAL);
}

/* append filename to file list */
else
{
    filePlayList[(*fileCount)++] = argv[i];
}
    }

    sfPrint = options->printFormat;

    /* for no files found, print message and exit */
    if (0 == *fileCount)
    {
	fprintf(stderr, "Well, supply some audio files !!!\n");
	fprintf(stderr, sfplayApplicationUsage);
	exit(ERRVAL);
    }
}	/* ---- end ParseSfPlayCommandLine() ---- */

/* **********************************************************************
 * SeizeHighExecutionPriority:	  set non-degrading priority, use high priority 
 *				    if running as root
 * ********************************************************************** */
static void
SeizeHighExecutionPriority(bool verbose)
{
    /* 
 * set non-degrading priority
 * use high priority if running as root
 * would be better to use DEADLINE scheduling, but, hey, it's a start
 */

    /* swap permissions so that we become root for a moment. */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());

#ifdef THIS_IS_BROKEN_REVISIT_AFTER_5_1
    /* we are root, use real exploitation */
    if (schedctl(NDPRI, 0, NDPNORMMAX)!=-1)
    {
	schedctl(SLICE, 0, CLK_TCK);
    }
    else
    {
	if (verbose)
	    SFPrint("Run as root for higher process priority");

	/* find lowest user non-degrading priority, ...
     * this value is one more than ndpri_hilim which can be set with 
     * systune(3), but cannot be determined by a system call, hence
     * the for() loop.
     */
	for (i = NDPLOMAX+1; schedctl(NDPRI, 0, i) != -1; i--) ;
    }
#else
    if (schedctl(NDPRI, 0, NDPNORMMAX)< 0)
    {
	if (verbose)
	    SFPrint("Run as root for higher process priority");
    }
#endif

    /* swap permissions back, now we're just "joe user" */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());
}	/* ---- end SeizeHighExecutionPriority() ---- */

#ifdef SHUFFLE_N_LOOP
/* ******************************************************************
 *  ShufflePlayList:	randomly reorder play list
 *			(limited to 32767 files)
 * ****************************************************************** */
static void
ShufflePlayList(char *inList[], char *outList[], int count, unsigned int *element)
{
    int		i, j;
    char		**tmpList;
    int		choiceIndex;
    unsigned int	value;
    double		scale;

    /* use temporary memory because in-place operation destoys input list */
    /* allocate memory and create copy of input list */
    tmpList = (char **) malloc(count*sizeof(char *));
    if (NULL == tmpList)
    {
	SFError("ShufflePlayList(): out of memory for new list");
	return;
    }
    /* copy input list into scratch memory */
    for (i = 0; i < count; i++)
	tmpList[i] = inList[i];

    /* 
 * randomly reorder sound file list
 */
    scale = (double) count-1;
    for (i = 0; i < count; i++)
    {
	/* generate random # in range [0 .. count-1] */
	/* linear congruential # generation */
	for (j = 0; j < count; j++)
	{
	    *element = (*element)*1103515245 + 12345;
	    value = ((*element)>>16)&0x7FFF;
	    choiceIndex = (int) (scale*((double) value)*(1.0/32767.0));
	    if (tmpList[choiceIndex] != NULL)
		break;
	}
	outList[i] = tmpList[choiceIndex];
	tmpList[choiceIndex] = NULL;
    }

    free(tmpList);
} /* ------ end ShufflePlayList() ---- */
#endif

