/* compile with cc -o sound sound.c -laudio -lm */

#include <dmedia/audio.h>
#include <math.h>
#include <stdio.h>

#define LATENCY_FACTOR 100

main()
{
    int		latency;
    int 	i,j;
    short	buf[60000];
    ALconfig    config;
    ALport	audioPort;
    double	samplingRate;
    double	arg, argInc;
    double	amplitude, frequency, phase;
    ALpv	pv;

    pv.param = AL_RATE;
    if (alGetParams(AL_DEFAULT_OUTPUT,&pv,1) < 0) {
	printf("alGetParams failed: %s\n",alGetErrorString(oserror()));
	exit(-1);
    }
    samplingRate = alFixedToDouble(pv.value.ll);
    if (samplingRate < 0) {
	samplingRate = 44100.0;
    }

    config = alNewConfig();
    latency = samplingRate / LATENCY_FACTOR;
    alSetQueueSize(config, latency);
    alSetChannels(config, AL_MONO);

    audioPort = alOpenPort("outport", "w", config);
    if (!audioPort) {
	fprintf(stderr, "couldn't open port: %s\n",alGetErrorString(oserror()));
	exit(-1);
    }

    frequency = 440.0;
    phase = 2.0;
    amplitude = 32000;
 
 for (j = 0; j < LATENCY_FACTOR; j++)
    {
    argInc = frequency*phase*M_PI/samplingRate;
    arg = M_PI / phase;
    
    frequency = frequency - 1;
    phase = phase - 0.1;
    /* amplitude = amplitude + 100; */
 
    for (i = 0; i < latency; i++, arg += argInc) 
        buf[i] = (short) (32767.0*cos(arg));
	
    alWriteFrames(audioPort, buf, latency);
    }
 
    while (alGetFilled(audioPort) > 0)
	sginap(1);

    alClosePort(audioPort);
    alFreeConfig(config);
}

