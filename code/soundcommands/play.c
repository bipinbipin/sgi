/*****************************************************************************
 *
 *   routine to play sound file 
 *	    
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <signal.h>

#include <dmedia/dmedia.h>

#include <audio.h>
#include "audiofile.h"
#include "al.h"
#include "sf.h"

float forceTransposeRate = 0.0;
float fileSkipTime = 0.0;
float filePlayTime = 0.0;

double _16_to_2matrix[] = {
 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0,
 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0
};

double _16_to_4matrix[] = {
 0.4, 0.0, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0,
 0.0, 0.4, 0.0, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0, 0.4, 0.0, 0.0,
 0.0, 0.0, 0.4, 0.0, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0, 0.4, 0.0,
 0.0, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0, 0.4, 0.0, 0.0, 0.0, 0.4
};

/* ******************************************************************
 * PrepareAudioFileAndHardware   	
 * ****************************************************************** */
bool 
PrepareAudioFileAndHardware( AFfilehandle fileHandle,
                             int id, 
                             ALconfig alConfiguration,
			     int device,
                             bool doNotRudelyChangeSamplingRate,
			     bool matchPortSampleRate,
                             int printFormat )
{
    int	sampleFormat, sampleWidth;
    int	channelCount;
    double	playSamplingRate;
    long	currentHardwareRate;
    bool	audioHardwareInUse;
    ALport	p;

    afSetVirtualByteOrder(fileHandle, id, AF_BYTEORDER_BIGENDIAN);

    /* attempt to set audio hardware output sampling rate to file rate */
    playSamplingRate = afGetRate(fileHandle, id);

    /* if user wants to force rate to another rate (does transp.) */

    if(forceTransposeRate != 0.0)
	playSamplingRate = forceTransposeRate;

    if (playSamplingRate <= 0)
    {
	SFError("Bogus sampling rate: %g Hertz\n", playSamplingRate);
	return (FALSE);
    }

    /* get sample rate. returns negative value if unable to obtain */
    currentHardwareRate = (long) GetAudioHardwareRate(device);

    if(matchPortSampleRate)
	playSamplingRate = currentHardwareRate;

    /* if necessary, change audio hardware output sampling rate  */

    if (currentHardwareRate != playSamplingRate)
    {
	audioHardwareInUse = AudioOutputHardwareInUse(device);
	/* if no one else is using audio output hardware, change sampling rate */
	if ((!audioHardwareInUse) || (!doNotRudelyChangeSamplingRate))
	{
	    ALpv buf[2];
	    buf[0].param = AL_RATE;
	    buf[0].value.ll = alDoubleToFixed((double)playSamplingRate);
	    buf[1].param = AL_MASTER_CLOCK;
	    buf[1].value.i = AL_CRYSTAL_MCLK_TYPE;
	    alSetParams(device, buf, 2);
	}

	/* hardware will use nearest supported rate.  */
	/* warn about unsupported rates (outside the Audio Driver rate set) */
	currentHardwareRate = GetAudioHardwareRate(device);
	if ((currentHardwareRate > 0)&&
	    (currentHardwareRate != (long) playSamplingRate))
	{
	    if (audioHardwareInUse)
		SFError("Audio Hardware in Use. Playing at %d Hz, not file rate %g Hz", 
		    currentHardwareRate, playSamplingRate);
	    else
		SFError("File rate %g Hz unsupported. Playing at %d Hz", 
		    playSamplingRate, currentHardwareRate);
	}
    }

    afSetVirtualRate(fileHandle, id, playSamplingRate);

    channelCount = afGetChannels(fileHandle, id);
    
    /*
     * try to see if we can open a port with the specified
     * number of channels.
     */
    alSetChannels( alConfiguration, channelCount );
    alSetDevice(alConfiguration, device);
    if (p = alOpenPort("sfplay-check", "w", alConfiguration)) {
	alClosePort(p);
	afSetVirtualChannels( fileHandle, id, channelCount );
    }
    else if (channelCount > 4) {
	/* we couldn't open a port. Try 4-ch if that makes sense. All versions
	   of AL support 4ch, so no need to check */
	SFError("warning: mixing down from %d to 4 channels",
		channelCount);
	alSetChannels( alConfiguration, 4 );
	afSetVirtualChannels( fileHandle, id, 4 );
	/* provide a mixdown matrix if no more than 16 inputs */
	if(channelCount <= 16)
		afSetChannelMatrix( fileHandle, id, _16_to_4matrix );
    }
    else {
	/* we couldn't open a port. We'll try stereo */
	SFError("warning: mixing down from %d-channel to stereo", channelCount);
	alSetChannels( alConfiguration, 2 );
	afSetVirtualChannels( fileHandle, id, 2 );
	/* provide a mixdown matrix if no more than 16 inputs */
	if(channelCount <= 16)
		afSetChannelMatrix( fileHandle, id, _16_to_2matrix );
	
    }

    /* set sample format */
    afGetSampleFormat(fileHandle, id, &sampleFormat, &sampleWidth);
    switch ( sampleFormat )
    {
    case AF_SAMPFMT_UNSIGNED:
    case AF_SAMPFMT_TWOSCOMP:
	switch ((sampleWidth-1)/8 + 1)
	{
	case 1:
	    ALsetwidth( alConfiguration, AL_SAMPLE_8 );
	    afSetVirtualSampleFormat( fileHandle, id, AF_SAMPFMT_TWOSCOMP, 8 );
	    break;
	case 2:
	    ALsetwidth( alConfiguration, AL_SAMPLE_16 );
	    afSetVirtualSampleFormat( fileHandle, id, AF_SAMPFMT_TWOSCOMP, 16 );
	    break;
	case 3:
	case 4:
	    ALsetwidth( alConfiguration, AL_SAMPLE_24 );
	    afSetVirtualSampleFormat( fileHandle, id, AF_SAMPFMT_TWOSCOMP, 24 );
	    break;

	default:
	    SFError("program plays 1-32 bit integer sound data only");
	    return(FALSE);
	}
	break;

    case AF_SAMPFMT_FLOAT:
    case AF_SAMPFMT_DOUBLE:
	ALsetwidth( alConfiguration, AL_SAMPLE_24 );
	afSetVirtualSampleFormat( fileHandle, id, AF_SAMPFMT_TWOSCOMP, 24 );
	break;
    }

    return (TRUE);
} /* ---- end PrepareAudioFileAndHardware() ---- */

/* ******************************************************************
 * PlaySoundFile   	
 * ****************************************************************** */
int 
PlaySoundFile(char *audioPortName,  int device, AFfilehandle fileHandle, int id, 
bool background, bool doNotRudelyChangeSamplingRate,
bool matchPortSampleRate, int printFormat)
/* audioPortName	name of port, cool to use application name
*/
{
    int	    processID;
    ALconfig    alConfiguration;
    ALport	    audioPort;
    int	    frames;		    /* frames per chunk */
    int	    frame;
    AFframecount frameOffset;
    AFframecount totalFrames = 9999999999;
    AFframecount playedFrames = 0;
    double sampleRate;
    void	    *sampleBuffer;

    alConfiguration = alNewConfig();
    if (!PrepareAudioFileAndHardware(fileHandle, id, alConfiguration, 
        device, doNotRudelyChangeSamplingRate, matchPortSampleRate, printFormat))
    {
	if (alConfiguration)
	    alFreeConfig(alConfiguration);
	return (-1);
    }

    sampleRate = afGetRate(fileHandle, id);

    /* we're now in child process if this is background play */
    /* our chunk size is 1/2 second of sample data */
    frames = (int) (0.5 * sampleRate);

    /* if user has specified a duration (non-zero play time), calculate
       frame count total.
    */

    if (filePlayTime > 0.0)
	totalFrames = filePlayTime * sampleRate;

    /* make ring buffer large enough to hold 1 sec of converted samples */
    alSetQueueSize( alConfiguration, 2*frames );

    /* check to see if audio port open.  fork makes it tought to detect if
    forked process was able to open an audioport.  However,  there is
    a possiblity that the forked process may not be able to open a new
    port due to a race condition which results from competing with
    other applications opening audio ports between the oprt available
    check and its subsequent open in the forked process.  */
    audioPort = alOpenPort( audioPortName, "w", alConfiguration );
    if ( !audioPort )
    {
	char string[120];
	
	sprintf(string, "failed to open audio port: %s", alGetErrorString(oserror()));
	SFError(string);
	return (-2);
    }
    alClosePort(audioPort);

    /* make refill happen at 1/4 sec */
    alSetFillPoint( audioPort, frames/2 );

    /* move fileptr to beginning of desired sample data (default is 0) */
    frameOffset = (double) fileSkipTime * sampleRate;
    afSeekFrame(fileHandle, AF_DEFAULT_TRACK, frameOffset);

    /* If this is background play, fork off here.
 Parent returns back immediately, while child allocates buffers, plays, 
 and exits when done.

 Note that in the background, the child can NOT call Error()--its 
 only choice on error is to exit.

 Also we make some nonsense calls to disable SIGINT handler of parent,
 which generally does some parent-specific thing.
*/
    if (background)
    {
#ifdef NOTDEF
	void  (*istat)(int, ...);
#endif /*NOTDEF*/
	void  (*istat)();

	istat = sigset( SIGINT, SIG_DFL );
	if (processID = fork())
	{			/* parent process */
	    sigset( SIGINT, istat );
	    return (processID);
	}
    }

    audioPort = alOpenPort( audioPortName, "w", alConfiguration );
    sampleBuffer = malloc( frames * afGetVirtualFrameSize(fileHandle, id, TRUE) );
    /* do it ! */
    while ((frame = afReadFrames(fileHandle, id, sampleBuffer, frames)) > 0) {
	ZAP();
	alWriteFrames(audioPort, sampleBuffer, frame);
	if (frame != frames)
	    break;
	playedFrames += frame;
	if(playedFrames >= totalFrames)
	    break;
    }

    if(frame < 0) {
	char detail[DM_MAX_ERROR_DETAIL];
	int errnum;
	dmGetError(&errnum, detail);
	fprintf(stderr, "%s [errnum %d]\n", detail, errnum);
    }

    free(sampleBuffer);

    /* poll output audio port for empty sample buffer */
    while (alGetFilled(audioPort) > 0)
	sginap(1);
    alClosePort( audioPort );
    if (alConfiguration)
	alFreeConfig(alConfiguration);

    if (background)
	exit(0);

    return (0);
} /* ---- end PlaySoundFile() ---- */
