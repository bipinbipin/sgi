/*
 * concert_a.c - Generate a pure 441 Hz tone for 5 seconds.
 *
 * Concert A is usually pitched somewhere between 440 Hz (USA) 
 *    and 442 Hz (Europe).
 */
#include <dmedia/audio.h>
#include <math.h>
#include <stdio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>

main()
{
    int	i;
    short	buf[60000];
    ALconfig    config;
    ALport	audioPort;
    double	samplingRate;
    double	arg, argInc;
    double	amplitude, freakinsee, phase;
    ALpv	pv;



    /*
     * Get the nominal output sample rate for the default output device.
     */
    pv.param = AL_RATE;
    if (alGetParams(AL_DEFAULT_OUTPUT,&pv,1) < 0) {
	printf("alGetParams failed: %s\n",alGetErrorString(oserror()));
	exit(-1);
    }
    samplingRate = alFixedToDouble(pv.value.ll);
    if (samplingRate < 0) {
	/*
	 * system couldn't tell us the sample rate: assume 44100
	 */
	samplingRate = 44100.0;
    }

    /*
     * Create a config which specifies a monophonic port
     * with 1 seconds' worth of space in its queue.
     */
    config = alNewConfig();
    alSetQueueSize(config, samplingRate);
    alSetChannels(config, AL_MONO);

    /*
     * Create an audio port using that config.
     */
    audioPort = alOpenPort("outport", "w", config);
    if (!audioPort) {
	fprintf(stderr, "couldn't open port: %s\n",alGetErrorString(oserror()));
	exit(-1);
    }

   
freakinsee = 88;



for (i = 0; i < 8; i++)
   { 

    argInc = freakinsee*2.0*M_PI/samplingRate;
    arg = M_PI / 2.0;
    for (i = 0; i < samplingRate; i++, arg += (argInc + (argInc/(argInc+4))),freakinsee++)
       { buf[i] = (32767.0*cos(arg)) + (i/freakinsee) ;
}

alWriteFrames(audioPort, buf, samplingRate);

    }






while (alGetFilled(audioPort) > 0)
	sginap(1);

    alClosePort(audioPort);
    alFreeConfig(config);
}

