/*
 * alInit.c: Does libaudio (audio library) initializations)
 *
 * This sets up an AL channel based on the audio track in the movie file.
 * It attempts to set the audio sample clock, but won't if there is anyone
 * else using audio.
 * 
 * Silicon Graphics Inc., June 1994
 */

#include "dmplay.h"

static void setAudioFrameRate (void);
static int AudioOutputHardwareInUse(void);
static long GetAudioHardwareOutputRate(void);
static long GetAudioHardwareInputRate(void);

/*
 * initialize audio hardware output port
 *
 * The audio port is initialized by first creating an audio config
 * structure with all of the appropriate parameters and then opening
 * a new audio channel with that configuration.
 *
 */
void alInit (void)
{
    ALconfig   config;
    int sampleWidth;    
    
    if (!audio.isTrack)
	return;

    /*
     * attempt to set audio hardware output sampling rate to movie rate
     */
    setAudioFrameRate();

    if ((config = ALnewconfig()) == NULL) {
	fprintf (stderr, "Unable to initialise an ALconfig structure\n");
	exit (1);
    }

    if (ALsetqueuesize(config, audio.queueSize) == -1) {
	fprintf (stderr, "Unable to set Queue size for the audio port.\n");
	exit (1);
    }

    if (audio.sampleWidth <= 8)
	sampleWidth = AL_SAMPLE_8;
    else if (audio.sampleWidth <= 16)
	sampleWidth = AL_SAMPLE_16;
    else
	sampleWidth = AL_SAMPLE_24;

    if (ALsetwidth(config, sampleWidth)) {
	fprintf (stderr, "Unable to set audio sample width.\n");
	exit(1);
    }
    
    if (ALsetchannels(config, audio.channelCount)) {
	fprintf (stderr, "Unable to set number of audio channels\n.");
	exit(1);
    }

    if ((audio.outPort = ALopenport("dmplay", "w", config)) == NULL) {
	fprintf(stderr,"Sorry, audio output is not available.\n");
	exit(1);
    }    

}

void setAudioFrameRate (void)
{
    int audioHardwareInUse;
    long currentHardwareRate;
    long buffer[2];
    
    /*
     * Temporarily turn off the AL error handler
     */
    ALerrfunc oldErrorHandler = ALseterrorhandler(NULL);

    currentHardwareRate = GetAudioHardwareOutputRate();

    if (currentHardwareRate != audio.frameRate) {

	audioHardwareInUse = AudioOutputHardwareInUse();

	/*
	 * if no one else is using audio output hardware, change sampling rate
	 */
	if (!(audioHardwareInUse = AudioOutputHardwareInUse()) ||
	    options.forceSampleRate) {
	    buffer[0] = AL_OUTPUT_RATE;
	    buffer[1] = (long) audio.frameRate;
	    ALsetparams(AL_DEFAULT_DEVICE, buffer, 2);
	    currentHardwareRate = GetAudioHardwareOutputRate();
	}

	/*
	 * hardware will use the nearest supported rate.
	 * Warn about unsupported rates (outside the Audio Driver 
	 * rate set)
	 */
	if ( currentHardwareRate != AL_RATE_UNDEFINED &&
	     currentHardwareRate != (long) audio.frameRate ) {
	    if (audioHardwareInUse) {
		fprintf (stderr,"Audio Hardware in use. Playing at ");
		fprintf (stderr,"%d Hz, not movie rate %g Hz\n", 
			 currentHardwareRate, audio.frameRate);
		fprintf (stderr, "Shutting off audio\n");
		/*
		 * we won't play the audio track because we
		 * can't change the output sample rate while
		 * another audio app is already running
		 */
		audio.isTrack = 0; 
	    } else {
		fprintf (stderr, "File rate XXX Hz unsupported. ");
		fprintf (stderr, "Playing at XXX2 Hz\n");
		      /* audio.frameRate, currentHardwareRate); */
	    }
	}
    }

    ALseterrorhandler(oldErrorHandler);
}

/*
 * GetAudioHardwareOutputRate
 *
 * output rate is in Hertz
 */
static long 
GetAudioHardwareOutputRate(void)
{
    long	buffer[4];

    buffer[0] = AL_OUTPUT_RATE;
    buffer[2] = AL_DIGITAL_INPUT_RATE;

    ALgetparams(AL_DEFAULT_DEVICE, buffer, 4);

    if (buffer[1] > 0) 
	return buffer[1];
    else
    if (buffer[1] == AL_RATE_INPUTRATE)
	return GetAudioHardwareInputRate();
    else
    if (ALgetdefault(AL_DEFAULT_DEVICE, AL_DIGITAL_INPUT_RATE) >= 0) 
	return buffer[3];
    else
	return AL_RATE_UNDEFINED;
}

/*
 * AudioOutputHardwareInUse - any output ports open or monitor on?  
 */
int
AudioOutputHardwareInUse(void)
{
    long	buffer[4];

    buffer[0] = AL_OUTPUT_COUNT;
    buffer[2] = AL_MONITOR_CTL;

    ALgetparams(AL_DEFAULT_DEVICE, buffer, 4);

    /* no open audio output ports and monitor off */
    if (buffer[1] == 0 && AL_MONITOR_OFF == buffer[3])
	return FALSE;

    return TRUE;
}

/*
 * GetAudioHardwareInputRate:    acquire audio hardware input sampling rate
 */
long 
GetAudioHardwareInputRate(void)
{
    long buffer[6];

    /* acquire state variables of audio hardware */
    buffer[0] = AL_INPUT_RATE;
    buffer[2] = AL_INPUT_SOURCE;
    buffer[4] = AL_DIGITAL_INPUT_RATE;

    ALgetparams(AL_DEFAULT_DEVICE, buffer, 6);

    /*
     * for input sources microphone or line and input rate not AES word clock
     */
    if (buffer[3] != AL_INPUT_DIGITAL && buffer[1] > 0)
	return buffer[1];
    /*
     * for input rate AES word clock and machine has ability
     * to read digital input sampling rate, return AL_DIGITAL_INPUT_RATE
     */
    else
    if (ALgetdefault(AL_DEFAULT_DEVICE, AL_DIGITAL_INPUT_RATE) >= 0) 
	return buffer[5];
    /*
     * could not determine sampling rate
     */
    else
	return AL_RATE_UNDEFINED;
}
