#include "stdio.h"
#include <Xm/Xm.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include "ulocks.h"
#include <sys/types.h>
#include <sys/prctl.h>
#include <stropts.h>
#include <poll.h>
#include "audio.h"
#include <limits.h>
#include <sys/schedctl.h>
#include "audiofile.h"
#include <fcntl.h>

#define NCHANS 4

/*--------------------------------------------------------------------------*/

/*
 * The following is a simple audio application.
 */

char *buttons[] ={ "Start", "Pause", "Stop"};
XtCallbackProc callback_handler();
void SendAudioCommand(char *cmd);
void InitializeAudioProcess(void);
void ProcessAudioCommand(void);
void GetMoreAudioSamples(void);

main(int argc, char **argv)
{
    Widget toplevel, rowcol, w;
    int i;
    toplevel = XtInitialize(argv[0], "AudioDemo", NULL, 0, &argc, argv);
    rowcol=XtCreateManagedWidget("rowcol",xmRowColumnWidgetClass,toplevel,NULL,0);
    for(i=0;i<XtNumber(buttons); i++){
	w=XtCreateManagedWidget(buttons[i],xmPushButtonWidgetClass,rowcol,NULL,0);
        XtAddCallback(w,XmNactivateCallback,(XtCallbackProc)callback_handler,buttons[i]);

    }
    XtRealizeWidget(toplevel);
    InitializeAudioProcess();
    XtMainLoop();
}

XtCallbackProc
callback_handler(widget, client_data, callback_data)
{
    SendAudioCommand((char *)client_data);
    return NULL;
}

/*--------------------------------------------------------------------------*/

/*
 * The following variables are shared between the audio handling
 * process and the main process
 */

usema_t *SendSema, *RcvSema;
char* command;

/*--------------------------------------------------------------------------*/

/*
 * This is how we create the audio handling process.
 */

void AudioProcess(void *);

void
InitializeAudioProcess()
{
    usptr_t *arena;
    static short int signum=9;
    usconfig(CONF_ARENATYPE,US_SHAREDONLY);
    arena=usinit(tmpnam(0));
    SendSema=usnewpollsema(arena, 0);
    RcvSema =usnewsema(arena, 1);
    prctl (PR_SETEXITSIG, (void *)signum);
    sproc(AudioProcess,PR_SALL);
}

/*--------------------------------------------------------------------------*/

/*
 * This is how we send commands to the audio handling process.
 */

void
SendAudioCommand(char *cmd)
{
    /* Send the command */
    command=(char *)cmd;
    usvsema(SendSema);
    /* Wait for response */
    uspsema(RcvSema);
}

/*--------------------------------------------------------------------------*/

/*
 * This is the audio handling process.
 */

ALconfig config;
ALport port;
AFfilehandle file;

int nowplaying = 0;
short sampbuf[28800];
short *soundstart, *soundptr, *soundend;

#define AUDIO_LOW_WATER_MARK   4800
#define AUDIO_HIGH_WATER_MARK 14400

static char filename[] = "soundfile.aifc";

void
AudioProcess(void *v)
{
    struct pollfd PollFD[2];
    int gotsema;

    if (LoadAudioSamples() < 0)
        exit(-1);
    if (OpenAudioPort() < 0)
        exit(-1);

    /* 
     * Set non-degrading process priority for playback process.
     * For this call to succeed, process must have superuser permissions.
     * See the man page for schedctl().
     */
    if (schedctl(NDPRI,0,NDPHIMIN) == -1) {
	printf("%s: %s\n", "schedctl failed", strerror(oserror()));
    }
    
    /*
     * Set up two file descriptors for poll: one associated with our
     * audio port, and one associated with a pollable semaphore. The
     * pollable semaphore indicates when the other process has a command
     * for us to execute.
     */
    PollFD[1].fd=alGetFD(port);
    PollFD[1].events=POLLOUT;
    PollFD[0].fd=usopenpollsema(SendSema, 0777);
    PollFD[0].events=POLLIN;
    gotsema = uspsema(SendSema);
    while(1) {
	alSetFillPoint(port,alGetQueueSize(config)-AUDIO_LOW_WATER_MARK);
	
	/*
	 * block waiting for either a command to come in, or for the audio
	 * port to require more samples. We could also use select() to do
	 * this.
	 */
                if(!gotsema) { /* If we've already got the semaphore, no need to
poll */
                        poll(PollFD,2,-1);
                }
        if(PollFD[0].revents&POLLIN){
            ProcessAudioCommand();
            gotsema = uspsema(SendSema);
            usvsema(RcvSema);
        }
        if((PollFD[1].revents&POLLOUT) || (gotsema)) {
            GetMoreAudioSamples();
        }
    }
}


int 
OpenAudioPort()
{
    ALpv pv[2];
    int x;

    /*
     * Get the sample-rate and number of current users of the default
     * output device.
     */
    pv[0].param = AL_RATE;
    pv[1].param = AL_PORT_COUNT;
    if (alGetParams(AL_DEFAULT_OUTPUT, pv, 2) < 0) {
	fprintf(stderr, "alGetParams failed: %s\n", alGetErrorString(oserror()));
	return(-1);
    }

    /*
     * Now,  if we're not at 44.1 kHz,  set ourselves to 44.1kHz.
     * But only change the sample rate if no one else is using the output
     * device -- otherwise we may screw up the other application.
     */
    if (alFixedToDouble(pv[0].value.ll) != 44100.0) {
        if (pv[1].value.i > 0) {
	    /* Someone else is using this device at a different rate: leave it alone */
	    fprintf(stderr, "Someone else is using the default output at a different rate\n");
            return(-1);
	}
        else {
	    /*
	     * Set the sample rate. Note how we also set the master clock
	     * to be of crystal (internal) type. Otherwise, if some other
	     * master clock is currently in use,  we may get unexpected results.
	     */
	    pv[0].param = AL_RATE;
	    pv[0].value.ll = alDoubleToFixed(44100.0);
	    pv[1].param = AL_MASTER_CLOCK;
	    pv[1].value.i = AL_CRYSTAL_MCLK_TYPE;
            alSetParams(AL_DEFAULT_OUTPUT, pv, 2); 
        }
    } 
    
    /*
     * Open an NCHANS-channel,  16-bit audio port.
     * Note that the default value for NCHANS is 4.
     */
    config=alNewConfig();
    if (!config) {
	fprintf(stderr, "alNewConfig failed: %s\n", alGetErrorString(oserror()));
	return -1;
    }
    alSetChannels(config,NCHANS);
    alSetWidth(config,AL_SAMPLE_16);
    port=alOpenPort("demoprog","w",config);
    if (!port) {
	fprintf(stderr, "alOpenPort failed: %s\n", alGetErrorString(oserror()));
	return -1;
    }
    return 0;
}

int 
LoadAudioSamples()
{
    int fd;
    int nsamps, nframes;
    double rate;

    fd = open(filename, O_RDONLY);

    if (fd > 0) 
        file = afOpenFD(fd, "r", 0);
    else {
        printf("couldn't open file %s\n", filename);
	return -1;
    }

    /*
     * We do an afGetRate(file, AF_DEFAULT_TRACK) to
     * see what the rate of this file is, and holler if it's not 44100.

     * However,  we could also enable automatic rate conversion in the
     * Audio File Library. This would use the call
     * 
     * afSetVirtualRate(file, AF_DEFAULT_TRACK, 44100.0);
     * 
     * Note that the rate conversion burns some CPU if the file's rate
     * is not already 44100,  and that the default rate-conversion
     * algorithm is the highest-quality, slowest one. See afSetConversionParams
     * for more information on how to change the rate-conversion algorithm.
     */
    if (afGetRate(file, AF_DEFAULT_TRACK) != 44100) {
	printf("File is not sampled at 44.1kHz!\n");
	return -1;
    }

    nframes = afGetFrameCount(file, AF_DEFAULT_TRACK);
    nsamps = NCHANS * nframes;

    afSetVirtualChannels(file, AF_DEFAULT_TRACK, NCHANS);

    soundstart = (short *)malloc(nsamps * sizeof(short));
    soundend = soundstart + nsamps;
    soundptr = soundstart;

    /*
     * pin the buffer into memory. This is part of ensuring
     * that we get real-time performance, since it prevents 
     * the buffer from becoming paged out if we're short on memory.
     * In practice, we need to pin the code and other data,  too.
     */
    mpin(soundstart, nsamps*sizeof(short));
    
    /*
     * read the entire file into memory. In general,  we don't
     * want to do this; some files may be quite long.
     */
    afReadFrames(file, AF_DEFAULT_TRACK, soundstart, nframes);
    return(0);
}

void
ProcessAudioCommand(void)
{
    if (!strcmp(command, "Start")) 
        nowplaying = 1;
    else if (!strcmp(command, "Pause"))
        nowplaying = 0;
    else if (!strcmp(command, "Stop")) {
        nowplaying = 0;
        soundptr = soundstart;
    }
}

#define MIN(a,b) ((a)<(b)?(a):(b))

void
GetMoreAudioSamples(void)
{
    int nframes,n;
    int nframesgot = 0;
    int framestilend;

    /*
     * Figure out how many frames we need to fill
     * to bring the port to the high-water mark we've
     * established.
     */
    nframes=AUDIO_HIGH_WATER_MARK-alGetFilled(port);
    
    /*
     * Loop the audio buffer over and over until we've
     * filled the port to the high-water mark.
     */
    if(nframes>0) {
        if (nowplaying) {
            while (nframesgot < nframes) {
		/*
		 * Figure out how many frames we can play.
		 * This is the minimum of the number we want to play
		 * and the number of frames to the end of the sound buffer.
		 */
		framestilend = (soundend-soundptr)/NCHANS;
                n = MIN(nframes, framestilend);
		alWriteFrames(port,soundptr,n);
                nframesgot+=n;
		
		/*
		 * advance the pointer
		 */
                if (n >= framestilend) {
                    soundptr = soundstart;
                }
                else {
                    soundptr += (n * NCHANS);
		}
            }
        } 
        else {
	    /* if not playing, put out zeros. */
	    alZeroFrames(port, nframes);
	}
    }
}

