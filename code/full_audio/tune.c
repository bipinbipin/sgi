#include <dmedia/audio.h>
#include <math.h>
#include <stdio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>
#include <stdlib.h>


main(int argc,char **argv)
{
    int i, j, x;
    short       buf[60000];
    ALconfig    config;
    ALport      audioPort;
    double      samplingRate;
    double      arg, argInc;
    double      amplitude, frequency, phase;
    ALpv        pv;

	if (argc != 2) {
        printf("usage: %s <frequency>\n",argv[0]);
        exit(-1);
     }

        x = 1;
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
while (x > 0){
frequency = atof(argv[1]);
/* frequency = rand() / 31.4; */
j = (int) (frequency);
printf("%d\n",j);
    /* generate a second of sine wave into buf */
    argInc = frequency*2.0*M_PI/samplingRate;
    arg = M_PI / 2.0;
    for (i = 0; i < samplingRate; i++, arg += argInc) 
        buf[i] = (short) (32767.0*cos(arg));

    /* write 5 seconds to audio hardware */
    for (i = 0; i < 1; i++)
        alWriteFrames(audioPort, buf, samplingRate);
}
    /* 
     * When we close the port, the contents of the queue will be 
     * thrown away. So we wait until the queue has drained until
     * we close the port.
     */
    while (alGetFilled(audioPort) > 0)
        sginap(1);

    alClosePort(audioPort);
    alFreeConfig(config);
}
