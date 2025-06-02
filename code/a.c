#include <dmedia/audio.h>
#include <math.h>
#include <stdio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>
#include <stdlib.h>


main(int argc,char **argv)
{
    int         i, j, x;
    int         buf[44100];
    ALconfig    config;
    ALport      audioPort;
    double      samplingRate;
    double      arg, argInc;
    double 	frequency;
    ALpv        pv;


        x = 1;
    
    pv.param = AL_RATE;
    samplingRate = 44100.0;

    config = alNewConfig();
    alSetQueueSize(config, samplingRate);
    alSetChannels(config, AL_MONO);

    /*  Create an audio port using that config. */
    audioPort = alOpenPort("outport", "w", config);
    if (!audioPort) {
        fprintf(stderr, "couldn't open port: %s\n",alGetErrorString(oserror()));        exit(-1);
    }

/* continue infinitly, until ^Z */
/*while (x > 0)
   {
*/
	/* frequency = atof(argv[1]); */
	frequency = random() / 31462793.668256 + 200;

	printf("*************\n");
	printf("* frequency = %f\n",frequency);
	printf("*************\n");

    argInc = frequency*2.0*M_PI/samplingRate;
    arg = M_PI / 2.0;
    for (i = 0; i < samplingRate; i++, arg += argInc) 
        buf[i] = (short) (32767.0*cos(arg));

    for (i = 0; i < 44100; i++)
	printf("%d\t%d\n",i,buf[i]);

    for (i = 0; i < 1; i++)
        alWriteFrames(audioPort, buf, samplingRate); 
/*   }
 */ 
    while (alGetFilled(audioPort) > 0)
        sginap(1);
    alClosePort(audioPort);
    alFreeConfig(config);
}
