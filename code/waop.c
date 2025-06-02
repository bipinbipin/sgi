/*  
 *  waop.c 
 *     waop (Wide Audio OutPut) is an application that will take 
 *     several audio input files and play them across several 
 *     audio outputs devices in sample accurate sync. A prerequisite
 *     is that the output devices are locked to a common reference
 *     and are running at the same sample rate.
 *
 *     The application has several command line options. See usage().
 *
 *     A control file may be used to specify the input files and output
 *     devices. Lines starting with '#' are ignored. Input files are
 *     specified with 'infile <filename>' and output devices are
 *     specified with 'output <device>'. Possible device names are
 *     listed in 'apanel -print' or in the device list of apanel.
 *
 *     to compile: cc waop.c -o waop  -laudio -laudiofile
 * 
 *     Written 05-05-97 by Reed Lawson MTS Silicon Graphics Inc.
 */ 

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <bstring.h>
#include <sys/time.h>
#include <audio.h>
#include <dmedia/audiofile.h>


/* Local Defines */
#define MAX_KEYWORD_SIZE        20
#define MAX_IDENTIFIER_SIZE     256
#define MAX_OUTPUTS             20
#define MAX_INPUTFILES		20
#define MAX_OUTPUT_NAME         50
#define MAX_FILE_NAME           256
#define MAX_CHANNELS_PER_DEVICE		8
#define PREROLL_SAMPLES         4000
#define RATE                    44100.0
#define WIDTH                   4
#define WIDTHBITS               24

/* Name the default control file */
#define DEFAULT_CONTROL_FILE    "waop.cmd"

/* Output device constants */
#define AudioQueueSize  (int)RATE
#define FillPoint       (AudioQueueSize*9)/10
#define AudioChunk      (AudioQueueSize * 8) / 10


/* Local Types */
typedef struct
{
    char name[MAX_FILE_NAME];
    AFfilehandle FileHandle;
    char FileIsOpen;
    int NumberOfChannels;
    int NumberOfInputFrames;
    void *buffer;
} Infile_t;

typedef struct
{
    char name[MAX_OUTPUT_NAME];
    ALport port;
    ALconfig config;
    int device;
    int NumberOfChannels;
    void *buffers[MAX_CHANNELS_PER_DEVICE];
    int strides[MAX_CHANNELS_PER_DEVICE];
    long long mscOffset;
} Output_t;

/* Local Globals */
char *myname;
int FileChannelsTotal;
int OutputChannelsTotal;
int Files;
int Outputs;
Infile_t Infile[MAX_INPUTFILES];
Output_t Output[MAX_OUTPUTS];
char verbose;
char looping;


/* Function prototypes */
void usage(void);
char *getMyName(char name[]);
void parseCommandLine(int argc, char* argv[]);
void Initialize(void);
void ProcessInFile(Infile_t *infile);
void WaitForPort(ALport port);
void SpewAudio(void);
void CheckForUnderflow(int ExpectedChange, int port);
void MainLoop(void);

/* Let the code begin! */

main (int argc, char *argv[])
{
    /* Init the 'myname' variable */
    myname = getMyName(argv[0]);

    /* Process all of the command line options */
    parseCommandLine(argc, argv);

    /* Attempt to open all the files and ports. */
    Initialize();
    
    /* Attempt to set a non-degrading priority */
    if(schedctl(NDPRI, 0, NDPHIMIN) < 0)
    {
	printf("%s: Sorry, schedctl had problems:\n", myname);
	perror("");
    }

    /* Feed all that audio file data to the audio ports. */
    MainLoop();
}

void Initialize(void)
{
    int in, out;
    ALpv pvs[10];
    int size;
    int inchannel, outchannel, ranout;

    /* First see if this version of al has what we need */
    pvs[0].param = AL_VERSION;
    alGetParams(AL_SYSTEM,pvs,1);
    if (pvs[0].sizeOut < 0 || pvs[0].value.i < 6) {
	/* feature not present */
	printf("%s: Sorry, this program needs a more recent version of the audio library.\n", myname);
        exit(-1);
    }

    /* Initialize the input files */
    for(in = 0; in < Files; ++in)
    {
	/* Open all the files */
	Infile[in].FileHandle = afOpenFile(Infile[in].name, "r", NULL);

	/* If there is a problem, die */
	if(Infile[in].FileHandle == AF_NULL_FILEHANDLE)
	{
	    printf("%s: Trouble opening %s.\n", myname, Infile[in].name);
	    exit(-1);
	}

	/* Mark this file as open */
	Infile[in].FileIsOpen = 1;

	/* Get the number of channels in the file */
	Infile[in].NumberOfChannels = afGetChannels(Infile[in].FileHandle, AF_DEFAULT_TRACK);

	/* If we are talkative, report this to the user */
	if(verbose) printf("%s: File %s has %d channels.\n", myname, 
			    Infile[in].name, Infile[in].NumberOfChannels);

	/* Keep a total */
        FileChannelsTotal += Infile[in].NumberOfChannels;

	/* Compute the size of the required buffer in bytes */
	size = AudioChunk * WIDTH * Infile[in].NumberOfChannels;

	/* Get a buffer for a chunk of this audio */
	Infile[in].buffer = malloc(size);

	/* Quit everything if out of memory! */
	if(Infile[in].buffer == NULL)
	{
	    printf("%s: Ran out of memory opening buffer for %s!\n", myname, Infile[in].name);
	    exit(-1);
	}

	/* Set the virtual width and rate of this file. 
	   NOTE: This may cause CPU consumption due to Sample Rate Conversion 
	 */
        afSetVirtualSampleFormat(Infile[in].FileHandle, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, WIDTHBITS);
	afSetVirtualRate(Infile[in].FileHandle, AF_DEFAULT_TRACK, RATE);
    }

    if(verbose) printf("%s: %d input channels total.\n", myname, FileChannelsTotal);

    /* Initialize output devices */
    for(out = 0; out < Outputs; out++)
    {
        Output[out].device  = alGetResourceByName(AL_SYSTEM, Output[out].name, AL_DEVICE_TYPE);

	/* Die if even one output device can't be found. */
        if(Output[out].device == 0) 
        {
            printf("%s: Can't find device: %s \n", myname, Output[out].name);
            exit(-1);
        }

	/* Find out how big it is. */
	pvs[0].param = AL_CHANNELS;
	alGetParams(Output[out].device, pvs, 1);
	if(pvs[0].value.i > MAX_CHANNELS_PER_DEVICE)
	{
	    printf("%s: Too many channels in %s! I quit!\n", myname, Output[out].name);
	    exit(-1);
	}

	/* Save this info for later */
	Output[out].NumberOfChannels = pvs[0].value.i;

	/* If we are talkative, report this to the user */
	if(verbose) printf("%s: Output %s has %d channels.\n", myname, Output[out].name, 
				Output[out].NumberOfChannels);

	/* Keep a running total */
	OutputChannelsTotal += Output[out].NumberOfChannels;
    }

    if(verbose) printf("%s: %d output channels total.\n", myname, OutputChannelsTotal);

    /* 
       Here is where we set up the *buffers[] and strides[] arrays for the output ports
       in preparation to use the alWriteBuffers() library call. We only have to set this
       up once before the main loop is entered. What alWriteBuffers() wants to see is
       an array of pointers, one pointer for each channel of output. So, we are setting
       these pointers to point to the buffers we just allocated for each input file. 
       One obvious caveat is that all input buffers must have a width (byte, word, long)
       that matches the width of the output port as set by alSetWidth() (hence the above
       call to afSetVirtualSampleFormat()).

       The strides[] array provides a way to accommodate buffers that have audio channels
       interleaved. Simply specify what the corresponding interleave is for the buffer
       that each pointer is pointing at (i.e. if the input file is stereo, then the
       interleave is 2).
    */

    in = 0;
    inchannel = 0;
    ranout = 0;

    for(out = 0; out < Outputs; out++)
    {
	for(outchannel = 0; outchannel < Output[out].NumberOfChannels; ++outchannel)
	{
	    if(!ranout)
	    {
		/* Point the "buffers" pointer to the correct starting word for this channel */
		Output[out].buffers[outchannel] = (char *)Infile[in].buffer + (WIDTH*inchannel);

		/* Set the strides according to the interleave of this file */
		Output[out].strides[outchannel] = Infile[in].NumberOfChannels;


		/* When we have used up all the inputs in this file.... */
		if(++inchannel >= Infile[in].NumberOfChannels)
		{
		    /* ...switch to the next input file */
		    inchannel = 0;

		    /* And when we run out of files ... */
		    if(++in >= Files)
		    {
			/* Set a flag that causes zeros to be sent to the
			   unused output channels */
			ranout = 1;
		    }
		}
	    }
	    else /* ranout */
	    {
		/* Cause silence to be output on leftover channels */
		/* Its real nice that you have only to set the pointer to NULL (0)
		   and you do not have to point to a buffer full of zeros. */
		Output[out].buffers[outchannel] = NULL;
		Output[out].strides[outchannel] = 0;
	    }
	}
    }
}

/*
  This code opens the required audio ports, and syncs them up in preparation for calls
  to alWriteBuffers(). It uses the Unadjusted System Time (UST) and Media Stream Count
  (MSC) infrastructure to accomplish audio sample accurate sync.
  Basically, the sequence of events is like this:

  1) Prime the pump. Send a bunch of zeros out the port. This gets the queues out of
     underflow.
  2) Snap a picture of the queue positions and port positions.
  3) Compute the relative MSC offsets amongst the ports.
  4) Stuff zeros in the queues to align them such that the next audio frames
     written to these queues will all exit the system at the same time (sample).
*/

void StartOutputPorts(void)
{
    int out;
    long long msc[MAX_OUTPUTS], ust[MAX_OUTPUTS];
    long long mscQueuePositions[MAX_OUTPUTS];
    long long PositionError[MAX_OUTPUTS];
    long long QueueOffset[MAX_OUTPUTS];
    double Phase;
    int FramesOff;
    double nanosec_per_frame = (1000000000.0)/(double)RATE;
    long long lowest = 0;

    char name[20];
    strcpy(name, "outport ");

    /* Create a 'ALconfig' for each output port. Setup the information in each config. */
    for(out = 0; out < Outputs; ++out)
    {
        Output[out].config = alNewConfig();
        alSetQueueSize(Output[out].config, AudioQueueSize);
	alSetChannels(Output[out].config, Output[out].NumberOfChannels);
        alSetDevice(Output[out].config, Output[out].device);
	alSetWidth(Output[out].config, AL_SAMPLE_24);
    }

    /* Attempt to open up all these ports */
    for(out = 0; out < Outputs; ++out)
    {
	/* Create a unique name for each port */
        name[7] = out + '0';

	/* Attempt to open the port  as writable */
        Output[out].port = alOpenPort(name, "w", Output[out].config);

	/* Die if the port does not open. Attempt to spell out the problem. */
        if(Output[out].port == NULL)
        {
            printf("%s: Error opening %s: %s\n", myname, Output[out].name,
                   alGetErrorString(oserror()));
            exit(-1);
        }
    }

    /* Prime the pump by sending zeros to all output ports. This gets the hardware
       cranking so that calls to alGetFrameNumber() and alGetFrameTime() will
       return meaningful data. */
    for(out = 0; out < Outputs; ++out)
    {
        alZeroFrames(Output[out].port, PREROLL_SAMPLES);
    }

    /* alGetFrameNumber() obtains for us the audio queue position. The value
       returned tells us the serial number (MSC) that will be assigned to the 
       next audio frame written to the queue (audio port) with alWriteBuffers().
    */
    for(out = 0; out < Outputs; ++out)
    {
        alGetFrameNumber(Output[out].port, &mscQueuePositions[out]);
    }

    /* Obtain the UST/MSC atomic pair for each port. From this we can establish a 
       port to port MSC offset that will never change as long as the ports are
       open and referenced to the same clock and running at the same sampling rate.
    */
    for(out = 0; out < Outputs; ++out)
    {
        alGetFrameTime(Output[out].port, &msc[out], &ust[out]);
    }

    /* Here is where we calculate these MSC offests. We use the first port
       as a reference for the rests. Naturally then, Output[0].mscOffset
       will be zero. */
    for(out = 0; out < Outputs; ++out)
    {
        /* Calculate this one's phase relative to the reference in percent. 
	   This tells us how well aligned each port is to a fraction of a 
	   sample. The value obtained here may be full samples ahead or
	   behind. 
	*/
        Phase = ((double)(ust[out] - ust[0]) / (nanosec_per_frame)) * 100;

	/* In theory, the USTs we obtain for each port from alGetFrameTime()
	   may be wildly different from the reference, so we must be prepared 
	   to adjust the MSC we obtained to align with the reference. So we 
	   round up or down to the nearest sample to find the best samples to 
	   associate between this port and the reference port.  Note that if 
	   Phase is between -50 and 50, the MSC we obtained need not be 
	   adjusted. 
        */
        FramesOff = 0;
        if(Phase >  50) FramesOff = (Phase + 50)/100;
        if(Phase < -50) FramesOff = (Phase - 50)/100;

        /* Adjust this msc to where it was when the reference one was read. */
        msc[out] -= FramesOff;

	/* Here is where we store the port's MSC offset with respect to
	the reference (port 0). This is used as the basis for output sync
	for the remainder of the applications life. */
        Output[out].mscOffset = msc[0] - msc[out];
    }

    /* Now, we deal with the queue's offsets. Like the port's offset
       each output queue should have a very similar offset. To the extent
       they are not the same, we have to adjust them to make them the same. */
       
    /* Calculate all the queue offsets for these output ports using the data
       obtained from alGetFrameNumber(). */
    for(out = 0; out < Outputs; ++out)
    {
        /* Calculate all of the MSC's offsets with respect to the first one */
        QueueOffset[out] = mscQueuePositions[0] - mscQueuePositions[out];
    }

    /* Find out how far out of sync each queue is by using the port offsets 
       computed in the above phasing exercise. This tells us how
       much extra we must stuff in the output queue to make up */
    for(out = 0; out < Outputs; ++out)
    {
        PositionError[out] = QueueOffset[out] - Output[out].mscOffset;
    }

    /* Make all the errors positive by adding the inverse of the most negative one. 
       This is necessary because we can only add zeros to the queues, we cannot remove
       them. */

    /* So, first go and find the lowest one */
    for(out = 0; out < Outputs; ++out)
    {
        if(PositionError[out] < lowest) lowest = PositionError[out];
    }

    /* Make problems into opportunities :-) */
    lowest = -lowest;

    /* Make all the position errors positive. (and the lowest one zero) */
    for(out = 0; out < Outputs; ++out)
    {
        PositionError[out] += lowest;
    }

    /* Now we fix the problem by stuffing zeros in the ports that are behind */
    for(out = 0; out < Outputs; ++out)
    {
	/* Of course, there is always the possibility that the error is more than
	   the amount we stuffed at the beginning */
        if(PositionError[out] > PREROLL_SAMPLES)
        {
            printf("%s: Internal error! Insufficient preroll %lld\n", myname, PositionError[out]);
        }
        else if(PositionError[out])
        {
	    /* Apply the position compensation for exact sample alignment */
            alZeroFrames(Output[out].port, PositionError[out]);
        }
    }
    /* Output ports are up, running and synced */
}

void MainLoop(void)
{
    int in, out;
    char SomeAreOpen;
    static int notFirstTime;

    /* The only way out is to run out of input data and not be in loop mode */
    while(1)
    {
	/* Clear the flag that indicates there is one or more files still open */
	SomeAreOpen = 0;

	/* For each open file, get a buffer full of audio data */
	for(in = 0; in < Files; in++)
	{
	    /* If the file has been played out and subsequently closed, skip it */
	    if(!Infile[in].FileIsOpen) continue;

	    /* Ah! At least one is still playing */
	    SomeAreOpen = 1;

	    /* Process the input file (get some audio) */
            ProcessInFile(&Infile[in]);
        }

	/* See if any files are still open. Quit if done. */
	if(!SomeAreOpen) break;


	/* Open the audio ports if this is the first time through */
	if(!notFirstTime)
	{
	    StartOutputPorts();
	}
	notFirstTime = 1;

	/* Wait for the first port to get to the low water mark */
	WaitForPort(Output[0].port);

        /* Write the audio frames */
	SpewAudio();

    }
}

void ProcessInFile(Infile_t *infile)
{
    int FramesRead;
    
    /* Read a frame of stuff */
    FramesRead = afReadFrames(infile->FileHandle, AF_DEFAULT_TRACK, infile->buffer, AudioChunk);

    /* if the file is empty, loop it or close it */
    if(FramesRead != AudioChunk)
    {
	/* Zero the unwritten part of the buffer */
	char *wheretostart = (char *)infile->buffer + FramesRead * WIDTH * infile->NumberOfChannels;
	int bytestoclear = (AudioChunk - FramesRead) * WIDTH * infile->NumberOfChannels;
	bzero(wheretostart, bytestoclear);
	if(looping)
	{
	    /* move fileptr to beginning of sample data */
	    afSeekFrame(infile->FileHandle, AF_DEFAULT_TRACK, 0);
	}
	else
	{
	    /* Close the file */
	    afCloseFile(infile->FileHandle);
	    infile->FileIsOpen = 0;
	}
    }
    
    /* Report how many frames were written */
    infile->NumberOfInputFrames = FramesRead;
}

/* Just block (give control to unix) until the output port is ready for data */
void WaitForPort(ALport port)
{
    fd_set writefds;
    int fd, ndfs;
    int rv;

    /* Set the fill point to 90% of the queue size */
    alSetFillPoint(port, FillPoint);

    /* Set up for the select() call*/
    fd = alGetFD(port);
    ndfs = fd + 1;
    FD_ZERO(&writefds);
    FD_SET(fd, &writefds);

    /* Block until the fill point is reached. */
    rv = select(ndfs, NULL, &writefds, NULL, NULL);
    if(rv < 0)
    {
        printf("%s: select failed.\n", myname);
        perror("");
    }
}

void SpewAudio(void)
{
    int out;

    /* For each output, call alWriteBuffers() to send the ports data out.
       Then check to see if the MSC for the port actually moved that much
       since last time. If not, there must have been an underflow */

    for(out = 0; out < Outputs; out++)
    {
	alWriteBuffers(Output[out].port, Output[out].buffers, Output[out].strides, AudioChunk);
	CheckForUnderflow(AudioChunk, out);
    }
}

void CheckForUnderflow(int ExpectedChange, int port)
{
    long long Position, delta;
    static long long LastPosition[MAX_OUTPUTS];
    static char notFirstTime[MAX_OUTPUTS];
    char *flow;

    /* Get the latest queue position */
    alGetFrameNumber(Output[port].port, &Position);

    /* Inhibit the first call for going wrong */
    if(notFirstTime[port])
    {
	/* Delta better be zero or we blew it */
        delta = ExpectedChange - (Position - LastPosition[port]);
        if(delta)
        {
            if(delta < 0)
            {
                flow = "under";
                delta = - delta;
            }
            else
            {
		/* It should never be an over flow. Never */
                flow = "over";
            }
	    /* Report the underflow, or impossible overflow */
            printf("%s: Detected %sflow on port %d of %lld.\n", myname, flow, port, delta);
        }
    }

    /* Keep the history of the last position */
    LastPosition[port] = Position;

    /* Record that LastPosition[port] is now valid */
    notFirstTime[port] = 1;
}


void LogInputFile(char *Filename)
{
    if(Files >= MAX_INPUTFILES)
    {
	printf("%s: Too many input files!\n", myname);
    }

    /* Store the name in the structure */
    strncpy(Infile[Files].name, Filename, MAX_FILE_NAME);

    /* Truncate very long names */
    Infile[Files].name[MAX_FILE_NAME-1] = '\0';

    /* Increment counter */
    Files++;
}

void LogOutput(char *Devicename)
{
    if(Outputs > MAX_OUTPUTS)
    {
	printf("%s: Too many output devices!\n", myname);
    }

    /* Store the name in the structure */
    strncpy(Output[Outputs].name, Devicename, MAX_OUTPUT_NAME);

    /* Truncate very long names */
    Output[Outputs].name[MAX_OUTPUT_NAME-1] = '\0';

    /* Increment counter */
    Outputs++;
}


void ProcessControlFile(char *Filename)
{
    FILE *fd;
    char keyword[MAX_KEYWORD_SIZE];
    char identifier[MAX_IDENTIFIER_SIZE];
    char junk[MAX_KEYWORD_SIZE];
    int c;

    fd = fopen(Filename, "r");
    if(fd == NULL) 
    {
	printf("%s: Could not open %s \n", myname, Filename);
	return;
    }

    while(fscanf(fd, "%20s%256s", keyword, identifier, junk) != EOF)
    {
        if(strcmp("infile", keyword) == 0)
	{
	    LogInputFile(identifier);
	}
	else if(strcmp("output", keyword) == 0)
	{
	    LogOutput(identifier);
	}
	else if(keyword[0] == '#') ;/* ignore comments */
        else printf("%s: Unknown keyword %s in file %s\n", myname, keyword, Filename);

	/* Flush unwanted characters from the end of the line */
	do  c = fgetc(fd);  while((c != '\n') && (c != EOF));
    }

    /* Close the file */
    fclose(fd);
}

void parseCommandLine(int argc, char* argv[])
{
    int c;

    /* Parse command line arguments */
    while((c=getopt(argc,argv,"hi:o:f:lv")) != -1)
    {
        switch(c)
        {
        case 'h':
            usage();
            exit(0);
            break;

        case 'i':
            LogInputFile(optarg);
            break;

        case 'o':
            LogOutput(optarg);
            break;

        case 'f':
	    ProcessControlFile(optarg);
            break;

        case 'l':
            looping = 1;
            break;

        case 'v':
            verbose = 1;
            break;
        }
    }

    if(!Files && !Outputs)
    {
	/* Try to open the default control file */
	ProcessControlFile(DEFAULT_CONTROL_FILE);
    }

    if(!Files && !Outputs)
    {
        usage();
        exit(0);
    }
}

char *getMyName(char name[])
{
    char *p = name;

    /* find the end of the string */
    while(*p != '\0') p++;

    /* Back up 'till a '/' is found */
    while(p > name)
    {
	if(*(p-1) == '/') break;
	--p;
    }
    return(p);
}

void usage(void)
{

    printf(\
"usage: %s -i <input file> [-i <more input files> ...] -o <output device> [-o <more output devices> ... ] \n"
"    Other options:\n"
"        -f <control file>     use control file to get input file names and output device names\n"
"                              Each line in the file is of the form <keyword> <identifier>\n"
"                              Where 'keyword' is either 'infile' or 'output'. Identifier is the\n"
"                              name of the input file or the name of the output device.\n"
"        -l (ell)              loop mode.\n"
"        -v                    verbose mode\n"
"        -h                    this help message\n"
, myname);

}


