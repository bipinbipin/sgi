#include <dmedia/audio.h>
#include <math.h>
#include <stdio.h>

main()
{
        int     i, j, f;
        short           buf[60000];
        ALconfig        config;
        ALport          audioPort;
        double          samplingRate;
        double          arg, argInc;
        double          amplitude, frequency, phase;
        ALpv            pv;

        pv.param = AL_RATE;
        if (alGetParams(AL_DEFAULT_OUTPUT,&pv,1) < 0) {
                printf("alGetParams failed: %s\n",alGetErrorString(oserror()));
                exit(-1);  }
        samplingRate = alFixedToDouble(pv.value.ll);
        if (samplingRate < 0) {  samplingRate = 44100.0;  }
        config = alNewConfig();
        alSetQueueSize(config, samplingRate);
        alSetChannels(config, AL_MONO);
        audioPort = alOpenPort("outport", "w", config);
        if (!audioPort) {  fprintf(stderr, "couldn't open port; %s\n",alGetErrorString(oserror()));
        exit(-1);  }

        j = 3;
        frequency = 43.5; 
        argInc = frequency*2.0*M_PI/samplingRate;
        arg = M_PI / 2.0;

while (j < 60)
{
        
        for (i = 0; i < samplingRate; i++, arg += argInc)
              { 
                 buf[i] = (int)  (32767.0*cos((4*arg*(1-arg)*(cos(arg))))); 
                 argInc = frequency*2.0*M_PI/samplingRate;
                 
              }

        alWriteFrames(audioPort, buf, samplingRate);
        j++;

        f = (int) (frequency);
        if (2 < f < 40 ) {
        frequency = frequency * 2;    }
        
        if (f > 40) {
        frequency = frequency / 2;    } 
/*      if (f <= 1) {
        frequency = frequency * 2;    } */      
        f = (int) (frequency);
        
        printf("devil> frequency = %d\n",f);

}

        while (alGetFilled(audioPort) > 0)
                sginap(1);
        alClosePort(audioPort);
        alFreeConfig(config);
} 


