/* ***************************************************************************
 *
 * 4Dgifts recordaiff program now ported to the audio file library
 *
 * - when program name is" "recordaiff" 
 *					 AIFF files created by default
 *                                             (for backward compatability)
 * - when program name is: "recordaifc", "sfrecord" it 
 *                                         AIFF-C files created by default
 * - sfrecord shares defaults w/playaifc
 *
 *		    scott porter, gints klimanis, doug scott
 *			    1992-6
 * ***************************************************************************
 */
#ident "$Revision: 1.24 $"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <limits.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <dmedia/audio.h>
#include <dmedia/audiofile.h>
#include <dmedia/audioutil.h>

#include "al.h"
#include "sf.h"

extern void
SFPrint(char *format, ...);

static char recordaiff[] = "recordaiff"; /* original program name */

typedef struct
{
    char    *applicationUsage;
    char    verbose;		    /* verbose operation flag */
    char    printFormat;
    char    doNotRudelyChangeSamplingRate;
    double  fileHeaderSamplingRate;

    double  framesPerSecond;
    long    bitsPerSample;
    long    bytesPerSample;
    long    samplesPerFrame;
    long    timeLimitSpecified;
    double  timeLimit;
    long    compressionType;
    long    fileFormat;
    
    int	    device;		    /* audio device from which to get audio */

    Aware   awareOptions;	    /* aware compression parameters */
} Parameters;

/* local subroutines */
static void DefaultParameters(Parameters *);
static void ParseCommandLine(int argc, char **argv, Parameters *);
static void RecordAudioToFile(ALport, AFfilehandle, Parameters *);

/* globals */
extern char *applicationName;
static char *recordFileName;
static char *fileNameTail;
extern char reportAFError;
extern char sfPrint;

extern int caughtSignalInterrupt;
extern char awareCompressionOptions[];
extern char compressionAlgorithms[];

/*
 * For IRIX 6.2, we won't document the new file formats (wave, next, raw)
 * compression schemes (mpegL1, mpegL2), etc supported by 'recordaifc'.
 *
 * Also, we won't document the alternate 'sfrecord' command-line
 * invocation.
 */
#ifdef NOTYET
static char recordAIFCApplicationUsage[] = 
"Usage: recordaifc [options] filename\n\
Options:\n\
    -help\n\
    -verbose\n\
    -n nchannels    1, 2, 4, 8\n\
    -s samplefmt    8, 16, 24\n\
    -r rate         hertz (44100 48000 etc)\n\
    -t time         seconds\n\
    -f filefmt      aiff, aifc, next, wave, raw\n\
    -d device       (e.g. DefaultIn, AnalogIn, AESIn, ADATIn ...)\n\
    -c compression  algorithm\n";
#else
static char recordAIFCApplicationUsage[] = 
"Usage: recordaifc [options] filename\n\
Options:\n\
    -help\n\
    -verbose\n\
    -n nchannels    1, 2, 4, 8\n\
    -s samplefmt    8, 16, 24\n\
    -r rate         hertz\n\
    -t time         seconds\n\
    -f filefmt      aiff, aifc\n\
    -d device       (e.g. DefaultIn, AnalogIn, AESIn, ADATIn ...)\n\
    -c compression  algorithm\n";
#endif

static char sfRecordApplicationUsage[] = 
"Usage: sfrecord [options] filename\n\
Options:\n\
    -help\n\
    -verbose\n\
    -quiet\n\
    -channels     1, 2, 4\n\
    -width        8, 16, 24\n\
    -rate         hertz\n\
    -time         seconds\n\
    -fileformat   aiff, aifc, next, wave, raw\n\
    -device       (e.g. DefaultIn, AnalogIn, AESIn, ADATIn ...)\n\
    -compression  algorithm\n";

static AFfilehandle OpenAudioOutFile(Parameters *parameters, 
                                     char *recordFileName,
                                     char **fileNameTail);
static ALport InitAudioPort(Parameters *parameters);
static char CheckParameters(Parameters *parameters);

/* ******************************************************************
 * main()	
 * ****************************************************************** */
int
main(int argc, char **argv)
{
    Parameters	    parameters;
    ALport		    audioInputPort;
    AFfilehandle	    handle;

    /* swap effective & real userIDs.  Thus, we may be super-user when 
 * required, but usually run as "joe user" */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());

    /* extract application name from path */
    applicationName = GetPathTail(argv[0]);
    DefaultParameters(&parameters);

    /* Backward compatibility code for recordaifc & recordaiff */
    if ((!strcmp(applicationName, "recordaifc"))||(!strcmp(applicationName, "recordaiff")))
	parameters.applicationUsage = recordAIFCApplicationUsage;
	/* sfrecord */
    else
	parameters.applicationUsage = sfRecordApplicationUsage;

    /* change behavior according to context.  If launched by
       typing, print messages.  If launched from within application, do not
       print messages */
    if (isatty(0))
    {
	reportAFError = TRUE;
	parameters.printFormat = SF_SHORT;
    }
    else
    {
	reportAFError = FALSE;
	parameters.printFormat = SF_NONE;
    }

    /* parse command line and check parameters for compatible values */
    ParseCommandLine(argc, argv, &parameters);
    sfPrint = parameters.printFormat;
    if (!CheckParameters(&parameters))
	exit(1);

    /* disable AF error handler if -quiet flag set */
    if (!reportAFError)
	afSetErrorHandler(QuietAFError);
    else
	afSetErrorHandler(DefaultAFError);

    /* set signal interrupt handler (for example, Control-C breaks) */
    caughtSignalInterrupt = FALSE;
    sigset(SIGINT, OnSignalInterrupt);

    /* initialize audio port */
    audioInputPort = InitAudioPort(&parameters);
    if (!audioInputPort)
    {
	char string[120];
	sprintf(string, "Couldn't open audio port: %s\n", alGetErrorString(oserror()));
	SFError(string);
	exit(1);
    }

    /* initialize audio file */
    handle = OpenAudioOutFile(&parameters, recordFileName, &fileNameTail);
    if (handle == AF_NULL_FILEHANDLE)
    {
	SFError("Failed to open audio file '%s'", recordFileName);
	exit(1);
    }

    /* record audio to file */
    RecordAudioToFile(audioInputPort, handle, &parameters);

    /* Clean house: close audio file and audio port */
    if (afCloseFile(handle) < 0)
    {
	SFError("Failed to update and close '%s'", recordFileName);
	exit(1);
    }
    alClosePort(audioInputPort);

    return 0;
} /* ---- end main() ---- */

/* ******************************************************************
 * DefaultParameters()	
 * ****************************************************************** */
static void 
DefaultParameters(Parameters *p)
{
    p->verbose = FALSE;
    p->printFormat = SF_NORMAL;

    /* default file format:  AIFF for sfrecord and recordaiff, AIFC for recordaifc */
    if (!strcmp(applicationName, "recordaifc"))
	p->fileFormat = AF_FILE_AIFFC;
    else
	p->fileFormat = AF_FILE_AIFF;

    p->timeLimitSpecified = 0;
    p->timeLimit = 0;

    /* intialize to stereo (2 channel), 16-bit, no compression,
	get hardware input sampling rate */
    p->samplesPerFrame = 2;
    p->bitsPerSample = 16;
    p->bytesPerSample = 2;
    p->compressionType = AF_COMPRESSION_NONE;
    p->device = AL_DEFAULT_INPUT;
    /*
     * We don't set the default rate here; we'll get that from the hardware
     * rate after we decide what the real input device is and 
     * try to set any rate given by the user.
     */
} /* ---- end DefaultParameters() ---- */

void
SetDevice(Parameters *options, char *name)
{
    /*
     * -device option:
     * See if the given name specifies a valid device.
     */
    options->device = alGetResourceByName(AL_SYSTEM,
	name, AL_DEVICE_TYPE);
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
 * ParseCommandLine()	
 * ****************************************************************** */
static void
ParseCommandLine(int argc, char **argv, Parameters *parameters)
{
    int	i;
    char	*s;
    char	fileFormatSeen	    = FALSE;
    char	rateSeen	    = FALSE;
    char	widthSeen	    = FALSE;
    char	channelsSeen	    = FALSE;
    char	timeSeen	    = FALSE;
    char	compressionSeen	    = FALSE;
    double t;
    int	someInt;

    recordFileName = (char *)0;

    /* requires at least application name and file name */
    if (argc < 2)
    {
	fprintf(stderr, "%s\n%s\n%s\n", 
	    parameters->applicationUsage, 
	    compressionAlgorithms, awareCompressionOptions);
	exit(1);
    }

    /* set up Aware parameters */
    parameters->awareOptions.bitRateTargetSpecified = FALSE;
    parameters->awareOptions.channelPolicySpecified = FALSE;
    parameters->awareOptions.bitRatePolicySpecified = FALSE;
    parameters->awareOptions.constantQualityNoiseToMaskRatioSpecified = FALSE;

    /* parse command line options */
    for (i = 1; i < argc; i++)
    {
	s = argv[i];

	/* file format option (-f, -filefmt, -fileformat) */
	if (MatchArg(argv[i], "-fileformat", 2))
	{
	    fileFormatSeen++;
	    if (++i >= argc)
	    {
		SFError("Well, supply file format: aiff, aifc, next, wave, raw.");
		exit(ERRVAL);
	    }
	    s = argv[i];
	    if	(!strcasecmp(s, "aiff"))
		parameters->fileFormat = AF_FILE_AIFF;
	    else if ((!strcasecmp(s, "aifc"))||(!strcasecmp(s, "aiff-c"))||
	        (!strcasecmp(s, "aiffc")))
		parameters->fileFormat = AF_FILE_AIFFC;
	    else if	((!strcasecmp(s, "next"))||(!strcasecmp(s, "snd")))
		parameters->fileFormat = AF_FILE_NEXTSND;
	    else if	(!strcasecmp(s, "raw"))
		parameters->fileFormat = AF_FILE_RAWDATA;
	    else if	(!strcasecmp(s, "wave"))
		parameters->fileFormat = AF_FILE_WAVE;
	    else
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	}

	/* channels option (-n, -nchannels for backward compatibility) */
	else if (MatchArg(argv[i], "-nchannels", 2)
	    || (!strcmp(argv[i], "-channels")))
	{
	    channelsSeen++;
	    if (++i >= argc)
	    {
		SFError("Well, supply channels!");
		exit(ERRVAL);
	    }
	    s = argv[i];
	    parameters->samplesPerFrame = atoi(s);
	    if (parameters->samplesPerFrame <= 0)
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	    /*
	     * if parameters->samplesPerFrame is positive, 
	     * we'll try to open a port & audio file using this
	     * value,  and complain then if that fails. This way
	     * we let the AL and Audio File Library decide what
	     * #channels are supported.
	     */
	}

	/* -rate record sampling rate option */
	else if (MatchArg(argv[i], "-rate", 2))
	{
	    rateSeen++;
	    if (++i >= argc)
	    {
		SFError("Please supply a sample rate.");
		exit(ERRVAL);
	    }
	    s = argv[i];
	    parameters->framesPerSecond = atof(s);
	    if ((!isdigit(s[0]))||(parameters->framesPerSecond <= 0))
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	}
	
	/* -device device option */
	else if (MatchArg(argv[i], "-device", 1))
	{
	    if (++i >= argc)
	    {
		SFError("No device given.");
		exit(ERRVAL);
	    }
	    SetDevice(parameters, argv[i]);
	}

	/* -width option (used to be -samplefmt) (-w, -width, -s, -samplefmt) */
	else if (MatchArg(argv[i], "-width", 2)||
	    MatchArg(argv[i], "-samplefmt", 2))
	{
	    widthSeen++;
	    if (++i >= argc)
	    {
		SFError("Well, supply sample width!");
		exit(ERRVAL);
	    }
	    s = argv[i];
	    if (!isdigit(s[0]))
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	    someInt = atoi(s);
	    if ((someInt != 8)&&(someInt != 16)&&(someInt != 24))
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }

	    /* convert bits to bytes */
	    parameters->bitsPerSample = someInt;
	    if	(someInt == 8)
		parameters->bytesPerSample = 1;
	    else if (someInt == 16)
		parameters->bytesPerSample = 2;
	    else if (someInt == 24)
		parameters->bytesPerSample = 4;
	}

	/* -compression option (-c, -compress, -compression) */
	else if (MatchArg(argv[i], "-compression", 2))
	{
	    compressionSeen++;
	    if (++i >= argc)
	    {
		SFError("Well, supply compression type:\
		    \nalaw, ulaw, g722, g726, g728, gsm, mpeg1L1, mpeg1L2, awmultirate, awlossless");
		exit(ERRVAL);
	    }
	    s = argv[i];

	    if	(!strcmp(s, "alaw"))
		parameters->compressionType = AF_COMPRESSION_G711_ALAW;
	    else if (!strcmp(s, "ulaw"))
		parameters->compressionType = AF_COMPRESSION_G711_ULAW;
	    else if	(!strcmp(s, "g722"))
		parameters->compressionType = AF_COMPRESSION_G722;
	    else if	(!strcmp(s, "g726"))
		parameters->compressionType = AF_COMPRESSION_G726;
	    else if	(!strcmp(s, "g728"))
		parameters->compressionType = AF_COMPRESSION_G728;
	    else if	(!strcmp(s, "gsm"))
		parameters->compressionType = AF_COMPRESSION_GSM;
	    else if	(!strcmp(s, "dvi"))
		parameters->compressionType = AF_COMPRESSION_DVI_AUDIO;

	    else if (!strcmp(s, "mpeg1L1") || !strcmp(s, "awmpeg1L1"))
	    {
		parameters->compressionType = AF_COMPRESSION_MPEG1;
		parameters->awareOptions.layer = AF_MPEG_LAYER_I;
		parameters->awareOptions.defaultConfiguration 
		    = AF_COMPRESSION_DEFAULT_MPEG1_LAYERI;
	    }
	    else if (!strcmp(s, "mpeg1L2") || !strcmp(s, "awmpeg1L2"))
	    {
		parameters->compressionType = AF_COMPRESSION_MPEG1;
		parameters->awareOptions.layer = AF_MPEG_LAYER_II;
		parameters->awareOptions.defaultConfiguration 
		    = AF_COMPRESSION_DEFAULT_MPEG1_LAYERII;
	    }
	    else if ((!strcmp(s, "awmultirate"))||(!strcmp(s, "awmulti")))
	    {
		parameters->compressionType = AF_COMPRESSION_AWARE_MULTIRATE;
		parameters->awareOptions.layer = AF_MPEG_LAYER_I; /* whatever */
		parameters->awareOptions.defaultConfiguration
		    = AF_COMPRESSION_AWARE_DEFAULT_MULTIRATE;
	    }
	    else if ((!strcmp(s, "awlossless"))||(!strcmp(s, "awlsls")))
	    {
		parameters->compressionType = AF_COMPRESSION_AWARE_MULTIRATE;
		parameters->awareOptions.layer = AF_MPEG_LAYER_I; /* whatever */
		parameters->awareOptions.defaultConfiguration 
		    = AF_COMPRESSION_AWARE_DEFAULT_LOSSLESS;
	    }

	    else if (!strcmp(s, "none"))
	    {
		parameters->compressionType = AF_COMPRESSION_NONE;
	    }
	    else
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	}

	/* -time:   length of recording session */
	else if (MatchArg(argv[i], "-time", 2))
	{
	    timeSeen++;
	    if (++i >= argc)
	    {
		SFError("Well, supply record time!");
		exit(ERRVAL);
	    }
	    s = argv[i];
	    if (!isdigit(s[0])&&(s[0] != '.'))
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	    parameters->timeLimit = (double) atof(s);
	    if (parameters->timeLimit <= 0)
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	    parameters->timeLimitSpecified = 1;
	}

	/* -verbose */
	else if (MatchArg(argv[i], "-verbose", 2))
	{
	    parameters->verbose = TRUE;
	}

	/* -reportError */
	else if (MatchArg(argv[i], "-reporterror", 3))
	{
	    reportAFError = TRUE;
	}

	/* help */
	else if (MatchArg(argv[i], "-help", 2))
	{
	    fprintf(stderr, "%s%s\n", parameters->applicationUsage, 
	        compressionAlgorithms);
	    exit(1);
	}

	/* -quiet */
	else if (MatchArg(argv[i], "-quiet", 2))
	{
	    parameters->printFormat = SF_NONE;
	}

	/* bit rate target (-aw_targetbitrate, -aw_targ) */
	else if (MatchArg(argv[i], "-aw_targ", 8))
	{
	    int b;

	    if (++i >= argc)
	    {
		SFError("Well, supply bit rate target!");
		exit(ERRVAL);
	    }
	    s = argv[i];
	    b = atoi(s);
	    switch (b)
	    {
	    case 32000:  /* I, II */
	    case 48000:  /*    II */
	    case 56000:  /*    II */
	    case 64000:  /* I, II */
	    case 96000:  /* I, II */
	    case 112000: /*    II */
	    case 128000: /* I, II */
	    case 160000: /* I, II */
	    case 192000: /* I, II */
	    case 224000: /* I, II */
	    case 256000: /* I, II */
	    case 228000: /* I     */
	    case 320000: /* I, II */
	    case 352000: /* I     */
	    case 384000: /* I, II */
	    case 416000: /* I     */
	    case 448000: /* I     */
		parameters->awareOptions.bitRateTarget = b;
		break;
	    default:
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	    parameters->awareOptions.bitRateTargetSpecified = TRUE;
	}

	/* channel policy (-aw_channelpolicy, -aw_chanpol) */
	else if (MatchArg(argv[i], "-aw_chan", 8))
	{
	    if (++i >= argc)
	    {
		SFError("Well, supply channel policy!");
		exit(ERRVAL);
	    }
	    s = argv[i];
	    if (!strcmp(s, "stereo"))
		parameters->awareOptions.channelPolicy = AF_AWARE_STEREO;
	    else if ((!strcmp(s, "independent"))||(!strcmp(s, "indep")))
		parameters->awareOptions.channelPolicy = AF_AWARE_INDEPENDENT;
	    else if (!strcmp(s, "joint"))
		parameters->awareOptions.channelPolicy = AF_AWARE_JOINT_STEREO;
	    else 
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	    parameters->awareOptions.channelPolicySpecified = TRUE;
	}

	/* bit rate policy (-aw_bitratepolicy, -aw_bitpol) */
	else if (MatchArg(argv[i], "-aw_bitp", 8))
	{
	    if (++i >= argc)
	    {
		SFError("Well, supply bit rate policy!");
		exit(ERRVAL);
	    }
	    s = argv[i];
	    if (!strcmp(s, "fixed"))
		parameters->awareOptions.bitRatePolicy = AF_AWARE_FIXED_RATE;
	    else if ((!strcmp(s, "constquality"))||(!strcmp(s, "constqual")))
		parameters->awareOptions.bitRatePolicy = AF_AWARE_CONST_QUAL;
	    else if (!strcmp(s, "lossless"))
		parameters->awareOptions.bitRatePolicy = AF_AWARE_LOSSLESS;
	    else 
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	    parameters->awareOptions.bitRatePolicySpecified = TRUE;
	}

	/* constant quality NoiseToMask Ratio (-aw_noisetomaskratio, -aw_nmr)*/
	else if (MatchArg(argv[i], "-aw_n", 5))
	{
	    double noiseToMaskRatio;

	    if (++i >= argc)
	    {
		SFError("Well, supply noise to mask ratio!");
		exit(ERRVAL);
	    }
	    s = argv[i];
	    noiseToMaskRatio = atof(s);
	    if ((noiseToMaskRatio < -13)|| (noiseToMaskRatio > 13))
	    {
		SFError("Invalid %s: '%s'", argv[i-1], s);
		exit(1);
	    }
	    parameters->awareOptions.constantQualityNoiseToMaskRatio = noiseToMaskRatio;
	    parameters->awareOptions.constantQualityNoiseToMaskRatioSpecified = TRUE;
	}

	/* unrecognized option: print help message */
	else if (argv[i][0] == '-')
	{
	    fprintf(stderr, "Invalid option: '%s'\n", s);
	    fprintf(stderr, "%s\n", parameters->applicationUsage);
	    exit(1);
	}

	/* assume we've found the filename: break out of the parser loop */
	else
	{
	    recordFileName = argv[i];
	    break;
	}
    }

    /* no filename found */
    if (recordFileName == (char *)0)
    {
	fprintf(stderr, "No file name specified.\n");
	fprintf(stderr, "%s\n", parameters->applicationUsage);
	exit(1);
    }

	/* check that DVI uses WAVE format */

	if (parameters->compressionType == AF_COMPRESSION_DVI_AUDIO &&
		parameters->fileFormat != AF_FILE_WAVE)
	{	
	fprintf(stderr, "DVI ADPCM may only be used with wave file format.\n");
	exit(1);
	}

    /* verbose implies not quiet */
    if (parameters->verbose)
	parameters->printFormat = SF_NORMAL;

    /* set hardware to nearest capable rate. Then, query for that rate */
    if (rateSeen)
    {
	ALpv pv[2];
	pv[0].param = AL_RATE;
	pv[0].value.ll = alDoubleToFixed((double)parameters->framesPerSecond);
	pv[1].param = AL_MASTER_CLOCK;
	pv[1].value.i = AL_CRYSTAL_MCLK_TYPE;
	alSetParams(parameters->device, pv, 2);
    }
    t = GetAudioHardwareRate(parameters->device);
    if (t <= 0) {
	parameters->framesPerSecond = 48000.0;
	fprintf(stderr, "Unable to determine input rate -- defaulting to 48000.0\n");
    }
    else {
	parameters->framesPerSecond = t;
    }
} /* ---- end ParseCommandLine() ---- */

/* ******************************************************************
 * RecordAudioToFile()	
 * ****************************************************************** */
static void
RecordAudioToFile(ALport audioInputPort, AFfilehandle handle,
                  Parameters *parameters)
{
    int	i;
    int	sampleCount, frameCount, frameLimit;
    char	*buffer;
    int	bytesPerBuffer, framesPerBuffer, samplesPerFrame;
    int	bytesPerSample;
    double	framesPerSecond, secondsPerFrame, secondsPerBuffer, secondCount;
    int	numBuffers;
    int	leftOverSamples, leftOverFrames;
    long	totalSampleBytes, totalFrames;
    char	compressionNameString[20], fileFormatNameString[20];
    char    byteUnitsString[100], durationString[100];
    char	done;

    samplesPerFrame   = parameters->samplesPerFrame;
    framesPerSecond   = parameters->framesPerSecond;
    bytesPerSample    = parameters->bytesPerSample;
    secondsPerFrame   = 1.0/framesPerSecond;
    framesPerBuffer   = 0.5*framesPerSecond;
    bytesPerBuffer    = framesPerBuffer*samplesPerFrame*bytesPerSample;
    secondsPerBuffer  = secondsPerFrame*framesPerBuffer;
    buffer = (char *) malloc(bytesPerBuffer);
    if (buffer == NULL)
    {
	SFError("Out of memory!");
	exit(-1);
    }

    if (parameters->printFormat != SF_NONE)
    {
	/* create compression name string */
	switch (parameters->compressionType)
	{
	case AF_COMPRESSION_NONE:
	    compressionNameString[0] = '\0';
	    break;
	case AF_COMPRESSION_G722:
	    strcpy(compressionNameString, "--> G.722");
	    break;
	case AF_COMPRESSION_G726:
	    strcpy(compressionNameString, "--> G.726");
	    break;
	case AF_COMPRESSION_G728:
	    strcpy(compressionNameString, "--> G.728");
	    break;
	case AF_COMPRESSION_GSM:
	    strcpy(compressionNameString, "--> GSM");
	    break;
	case AF_COMPRESSION_DVI_AUDIO:
	    strcpy(compressionNameString, "--> DVI");
	    break;
	case AF_COMPRESSION_G711_ALAW:
	    strcpy(compressionNameString, "--> A-law");
	    break;
	case AF_COMPRESSION_G711_ULAW:
	    strcpy(compressionNameString, "--> u-law");
	    break;
	case AF_COMPRESSION_MPEG1:
	    if (AF_MPEG_LAYER_I == parameters->awareOptions.layer)
		strcpy(compressionNameString, "--> MPEG1 Layer I");
	    else
		strcpy(compressionNameString, "--> MPEG1 Layer II");
	    break;
	case AF_COMPRESSION_AWARE_MULTIRATE:
	    if (AF_COMPRESSION_AWARE_DEFAULT_MULTIRATE == 
	        parameters->awareOptions.defaultConfiguration)
		strcpy(compressionNameString, "--> Aware MultiRate");
	    else
		strcpy(compressionNameString, "--> Aware LossLess");
	    break;
	default:
	    SFError("Invalid compression type: %d", parameters->compressionType);
	    break;
	}

	/* create file format name string */
	switch (parameters->fileFormat)
	{
	default:
	case AF_FILE_AIFF:
	    strcpy(fileFormatNameString, "aiff");
	    break;
	case AF_FILE_AIFFC:
	    strcpy(fileFormatNameString, "aifc");
	    break;
	case AF_FILE_RAWDATA:
	    strcpy(fileFormatNameString, "raw");
	    break;
	case AF_FILE_SOUNDESIGNER1:
	    strcpy(fileFormatNameString, "sd1");
	    break;
	case AF_FILE_SOUNDESIGNER2:
	    strcpy(fileFormatNameString, "sd2");
	    break;
	case AF_FILE_NEXTSND:
	    strcpy(fileFormatNameString, "next");
	    break;
	case AF_FILE_WAVE:
	    strcpy(fileFormatNameString, "wave");
	    break;
	}
    }

#ifdef THIS_IS_BROKEN_REVISIT_AFTER_5_1
    /* 
 * set non-degrading priority
 * use high priority if running as root
 * would be better to use DEADLINE scheduling, but, hey, it's a start
 */
    /* we are root, use real exploitation */
    if (schedctl(NDPRI, 0, NDPNORMMAX) != -1)
	schedctl(SLICE, 0, CLK_TCK);
    else
    {
	if ((schedctl(NDPRI, 0, NDPNORMMAX)< 0)&&(parameters->verbose))
	    SFPrint("Run as root for higher process priority");

	/* find lowest user non-degrading priority, ...
     * this value is one more than ndpri_hilim which can be set w/
     * systune(3), but cannot be determined by a system call, hence
     * the for() loop.
     */
	for (i = NDPLOMAX+1; schedctl(NDPRI, 0, i) != -1; i--) ;
    }
#else
    if ((schedctl(NDPRI, 0, NDPNORMMAX)< 0)&&(parameters->verbose))
	SFPrint("Run as root for higher process priority");
#endif

    /* 
 * -------------------- user has not specified time limit 
 */
    done = FALSE;
    if (!parameters->timeLimitSpecified)
    {
	totalSampleBytes = 0;
	totalFrames      = 0;
	secondCount      = 0;

	if (parameters->printFormat != SF_NONE)
	{
	    SFPrint("%s: '%s' %g Hz %d-channel %d-bit %s %s",
	        applicationName,
	        fileNameTail,
	        framesPerSecond,
	        samplesPerFrame,
	        parameters->bitsPerSample,
	        compressionNameString,
	        fileFormatNameString);
	}

	while (!done)
	{
	    alReadFrames(audioInputPort, buffer, framesPerBuffer);

#ifdef SAFE
	    /* swap bytes */
	    for (i = 0; i < framesPerBuffer*samplesPerFrame*2; i+=2)
	    {
		tmp = buffer[i];
		buffer[i] = buffer[i+1];
		buffer[i+1] = tmp;
	    }
#endif
	    frameCount = afWriteFrames(handle, AF_DEFAULT_TRACK, 
	                               buffer, framesPerBuffer);
	    totalFrames += frameCount;
	    if (frameCount != framesPerBuffer)
	    {
		SFError("Wrote only %d of %d frames to %s",
		    frameCount, framesPerBuffer, recordFileName);
		secondCount      += totalFrames * secondsPerFrame;
		totalSampleBytes += frameCount * samplesPerFrame * bytesPerSample;
		done = TRUE;
	    }
	    else 
	    {
		totalSampleBytes += bytesPerBuffer;
		secondCount	     += secondsPerBuffer;
	    }

	    if (parameters->verbose)
	    {
		DurationInSeconds(secondCount, durationString, SF_NORMAL);
		SizeInBytes(totalSampleBytes, byteUnitsString, SF_NORMAL);

		SFError("%d sample frames %s %s",
		    totalFrames, durationString, byteUnitsString);
	    }

	    if (caughtSignalInterrupt)
		done = TRUE;
	}

	/* drain audio data that has accumulated in audio port 
         * since last time we checked */
	sampleCount = ALgetfilled(audioInputPort);
	frameCount = sampleCount / samplesPerFrame;
	numBuffers = frameCount / framesPerBuffer;
	leftOverFrames = frameCount - numBuffers*framesPerBuffer;
	leftOverSamples  = leftOverFrames * samplesPerFrame;

	if ((parameters->printFormat != SF_NONE) && (numBuffers > 0))
	    SFPrint("Draining audio input port ...");

	while (numBuffers)
	{
	    alReadFrames(audioInputPort, buffer, framesPerBuffer);
	    frameCount = afWriteFrames(handle, AF_DEFAULT_TRACK, 
	                               buffer, framesPerBuffer);
	    totalFrames      += frameCount;
	    if (frameCount != framesPerBuffer)
	    {
		SFError("Wrote only %d of %d frames to %s",
		    frameCount, framesPerBuffer, recordFileName);
		secondCount      += totalFrames * secondsPerFrame;
		totalSampleBytes += frameCount * samplesPerFrame * bytesPerSample;
		numBuffers = 0;
	    }
	    else 
	    {
		totalSampleBytes += bytesPerBuffer;
		secondCount	     += secondsPerBuffer;
		numBuffers--;
	    }
	    if (parameters->verbose)
	    {
		DurationInSeconds(secondCount, durationString, SF_NORMAL);
		SizeInBytes(totalSampleBytes, byteUnitsString, SF_NORMAL);

		SFPrint("%d sample frames %s %s",
		    totalFrames, durationString, byteUnitsString);
	    }
	}

	ALreadsamps(audioInputPort, buffer, leftOverSamples);
	frameCount = afWriteFrames(handle, AF_DEFAULT_TRACK, 
	                           buffer, leftOverFrames);
	totalFrames      += frameCount;
	if (frameCount != leftOverFrames)
	{
	    SFError("Wrote only %d of %d frames to %s",
	        frameCount, leftOverFrames, recordFileName);
	    secondCount      += totalFrames * secondsPerFrame;
	    totalSampleBytes += frameCount * samplesPerFrame * bytesPerSample;
	}
	else 
	{
	    totalSampleBytes += (leftOverFrames*bytesPerSample*samplesPerFrame);
	}

	if (parameters->verbose)
	{
	    secondCount += leftOverFrames * secondsPerFrame;
	    DurationInSeconds(secondCount, durationString, SF_NORMAL);
	    SizeInBytes(totalSampleBytes, byteUnitsString, SF_NORMAL);

	    SFPrint("%d sample frames %s %s",
	        totalFrames, durationString, byteUnitsString);
	}

    }

    /* 
 * -------------------- user has specified a time limit 
 */
    else 
    {
	totalSampleBytes = 0;
	totalFrames      = 0;
	secondCount      = 0;

	/* translate time (in seconds) to number of sample frames */
	frameLimit      = (int)(parameters->timeLimit * framesPerSecond);
	numBuffers      = frameLimit / framesPerBuffer;
	leftOverFrames  = frameLimit - (numBuffers * framesPerBuffer);
	leftOverSamples = leftOverFrames * samplesPerFrame;
	secondCount     = 0;

	if (parameters->printFormat != SF_NONE)
	{
	    SFPrint("%s: '%s' %6.3f sec %7.1f Hz %d-channel %d-bit %s %s",
	        applicationName,
	        fileNameTail,
	        parameters->timeLimit,
	        framesPerSecond,
	        samplesPerFrame,
	        parameters->bitsPerSample,
	        compressionNameString,
	        fileFormatNameString);
	}

	/*
     * write blocks of samples
     */
	for (i = 0; (i < numBuffers)&&(!done); i++)
	{
	    alReadFrames(audioInputPort, buffer, framesPerBuffer);
	    frameCount = afWriteFrames(handle, AF_DEFAULT_TRACK, 
	                               buffer, framesPerBuffer);
	    totalFrames     += frameCount;
	    if (frameCount != framesPerBuffer)
	    {
		SFError("Wrote only %d of %d samples to %s",
		    frameCount, framesPerBuffer, recordFileName);

		secondCount     += totalFrames * secondsPerFrame;
		done++;
	    }
	    else 
	    {
		totalSampleBytes += bytesPerBuffer;
		secondCount      += secondsPerBuffer;
	    }

	    if (parameters->verbose)
	    {
		DurationInSeconds(secondCount, durationString, SF_NORMAL);
		SizeInBytes(totalSampleBytes, byteUnitsString, SF_NORMAL);

		SFPrint("%d sample frames %s %s",
		    totalFrames, durationString, byteUnitsString);
	    }

	    if (caughtSignalInterrupt)
		done = TRUE;
	}

	/*
     * write leftover samples
     */
	if (!done)
	{
	    ALreadsamps(audioInputPort, buffer, leftOverSamples);
	    frameCount = afWriteFrames(handle, AF_DEFAULT_TRACK, 
	                               buffer, leftOverFrames);
	    if (frameCount != leftOverFrames)
	    {
		SFError("Wrote only %d of %d frames to %s.",
		    frameCount, leftOverFrames, recordFileName);
	    }
	    totalFrames      += frameCount;
	    totalSampleBytes += frameCount*samplesPerFrame*bytesPerSample;
	    if (parameters->verbose)
	    {
		secondCount += leftOverFrames * secondsPerFrame;
		DurationInSeconds(secondCount, durationString, SF_NORMAL);
		SizeInBytes(totalSampleBytes, byteUnitsString, SF_NORMAL);

		SFPrint("%d sample frames %s %s",
		    totalFrames, durationString, byteUnitsString);
	    }
	}
    }
} /* ---- end RecordAudioToFile() ---- */

/* ******************************************************************
 * OpenAudioOutFile() 
 * ****************************************************************** */
static AFfilehandle
OpenAudioOutFile(Parameters *parameters, char *recordFileName,
                 char **fileNameTail)
{
    int		fd;
    AFfilehandle	handle;
    AFfilesetup	audioFileSetup;
    int	    counter;
    AUpvlist    pvlist;

    /* obtain unix file descriptor for input file */
    fd = open(recordFileName, O_CREAT|O_TRUNC|O_WRONLY, 0644);
    if (fd < 0)
    {
	SFError("Failed to create file '%s'", recordFileName);
	perror("");
	return (NULL);
    }
    *fileNameTail = GetPathTail(recordFileName);

    /* set file parameters */
    audioFileSetup  = afNewFileSetup();

    afInitFileFormat(audioFileSetup, parameters->fileFormat);

    /* WAVE file format uses little endian byte order */
    if (parameters->fileFormat == AF_FILE_WAVE)
	afInitByteOrder(audioFileSetup, AF_DEFAULT_TRACK, AF_BYTEORDER_LITTLEENDIAN);

    /* initialize compression parameters */
    if ((parameters->compressionType == AF_COMPRESSION_AWARE_MULTIRATE) ||
        (parameters->compressionType == AF_COMPRESSION_MPEG1))
    {
	pvlist = AUpvnew(MAX_AWARE_OPTS);
	counter = 0;
	if (parameters->compressionType == AF_COMPRESSION_MPEG1)
	{
	    AUpvsetparam(pvlist, counter, AF_AWARE_PARAM_LAYER);
	    AUpvsetvaltype(pvlist, counter, AU_PVTYPE_LONG);
	    AUpvsetval(pvlist, counter, &parameters->awareOptions.layer);
	    counter++;
	}
	if (parameters->awareOptions.bitRateTargetSpecified)
	{
	    AUpvsetparam(pvlist, counter, AF_AWARE_PARAM_BITRATE_TARGET);
	    AUpvsetvaltype(pvlist, counter, AU_PVTYPE_LONG);
	    AUpvsetval(pvlist, counter, &parameters->awareOptions.bitRateTarget);
	    counter++;
	}
	if (parameters->awareOptions.channelPolicySpecified)
	{
	    AUpvsetparam(pvlist, counter, AF_AWARE_PARAM_CHANNEL_POLICY);
	    AUpvsetvaltype(pvlist, counter, AU_PVTYPE_LONG);
	    AUpvsetval(pvlist, counter, &parameters->awareOptions.channelPolicy);
	    counter++;
	}
	if (parameters->awareOptions.bitRatePolicySpecified)
	{
	    AUpvsetparam(pvlist, counter, AF_AWARE_PARAM_BITRATE_POLICY);
	    AUpvsetvaltype(pvlist, counter, AU_PVTYPE_LONG);
	    AUpvsetval(pvlist, counter, &parameters->awareOptions.bitRatePolicy);
	    counter++;
	}
	if (parameters->awareOptions.constantQualityNoiseToMaskRatioSpecified)
	{
	    AUpvsetparam(pvlist,counter,AF_AWARE_PARAM_CONST_QUAL_NMR);
	    AUpvsetvaltype(pvlist,counter,AU_PVTYPE_DOUBLE);
	    AUpvsetval(pvlist,counter,&parameters->awareOptions.constantQualityNoiseToMaskRatio);
	    counter++;
	}
	afInitCompressionParams(audioFileSetup,
	    AF_DEFAULT_TRACK,
	    parameters->awareOptions.defaultConfiguration,
	    pvlist,
	    counter);
    }
    else
    {
	afInitCompression(audioFileSetup, AF_DEFAULT_TRACK,
	    parameters->compressionType);
    }

    afInitChannels(audioFileSetup, AF_DEFAULT_TRACK, parameters->samplesPerFrame);
    afInitSampleFormat(audioFileSetup, AF_DEFAULT_TRACK, 
                       AF_SAMPFMT_TWOSCOMP, parameters->bitsPerSample);
    afInitRate(audioFileSetup, AF_DEFAULT_TRACK, parameters->framesPerSecond);

    afInitAESChannelDataTo(audioFileSetup, AF_DEFAULT_TRACK, 0);
    afInitMarkIDs(audioFileSetup, AF_DEFAULT_TRACK, NULL, 0);
    afInitInstIDs(audioFileSetup, NULL, 0);
    afInitMiscIDs(audioFileSetup, NULL, 0);

    /* open audio file w/file set up */
    handle = afOpenFD(fd, "w", audioFileSetup);
    if (handle != NULL)
    {
	/* this here because default are not clearly documented */
	afSetVirtualByteOrder(handle, AF_DEFAULT_TRACK, AF_BYTEORDER_BIGENDIAN);
    }
    afFreeFileSetup(audioFileSetup);
    return (handle);
} /* ---- end OpenAudioOutFile() ---- */

/* ******************************************************************
 * InitAudioPort(): configure and open audio port	
 * ****************************************************************** */
static ALport
InitAudioPort(Parameters *parameters)
{
    ALport	    port;
    ALconfig	    portConfiguration;

    portConfiguration = alNewConfig();
    if (!portConfiguration) {
	return 0;	/* config failed */
    }
    alSetChannels(portConfiguration, parameters->samplesPerFrame);
    alSetSampFmt(portConfiguration, AL_SAMPFMT_TWOSCOMP);
    alSetWidth(portConfiguration, parameters->bytesPerSample);
    alSetDevice(portConfiguration, parameters->device);
    port = alOpenPort(applicationName, "r", portConfiguration);
    alFreeConfig(portConfiguration);

    return (port);
} /* ---- end InitAudioPort() ---- */

/* ******************************************************************
 * CheckParameters() 
 * ****************************************************************** */
static char
CheckParameters(Parameters *parameters)
{
    int	errorValue;

    /* ensure compression is used only with appropriate formats */
    if ((AF_COMPRESSION_NONE != parameters->compressionType)&&
        (AF_FILE_NEXTSND != parameters->fileFormat)&&
        (AF_FILE_WAVE != parameters->fileFormat)&&
        (AF_FILE_RAWDATA != parameters->fileFormat)&&
        (AF_FILE_AIFFC != parameters->fileFormat))
    {
#ifdef NOTYET /* don't advertise raw data files right now */
	SFError("Audio data compression supported in AIFF-C and raw data files only");
#else
	SFError("Audio data compression not supported in this file format");
#endif
	return (FALSE);
    }

    /* compression requires 16 bit samples */
    if ((16 != parameters->bitsPerSample)&&
        (AF_COMPRESSION_NONE != parameters->compressionType))
    {
	SFError("Compression requires 16-bit sample data");
	return (FALSE);
    }
    
    /*
 * check license for Aware audio compression encoder
 */
    switch (parameters->compressionType)
    {
    case AF_COMPRESSION_AWARE_MULTIRATE:
	if (parameters->printFormat != SF_NONE)
	    SFPrint("Checking license for Aware MultiRate encoder ...");

	AUchecklicense(AU_LICENSE_AWARE_MULTIRATE_ENCODER, &errorValue, 
	    (char **)0);
	if (parameters->printFormat != SF_NONE)
	    SFError("Done");

	if (errorValue != AU_LICENSE_OK)
	{
	    if (parameters->printFormat != SF_NONE)
		SFError("License for Aware MultiRate encoder not available");

	    return (FALSE);
	}
	break;
    default:
	break;
    }

    return (TRUE);
} /* ---- end CheckParameters() ---- */
