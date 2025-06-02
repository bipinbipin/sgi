#include <dmedia/audio.h>
#include <math.h>
#include <stdio.h>

main()
{
	int	i;
	short		buf[60000];
	ALconfig 	config;
	ALport		audioPort;
	double		samplingRate;
	double		arg, argInc;
	double		amplitude, frequency, phase;
	ALpv		pv;

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
	frequency = 1;
	argInc = frequency*2.0*M_PI/samplingRate;
	arg = M_PI / 2.0;

	for (i = 0; i < samplingRate; i++,frequency++, arg += argInc)
	{	buf[i] = (short) (32767.0*cos(arg));
		argInc = frequency*2.0*M_PI/samplingRate;    }

	for (i = 0; i < 3; i++)
		alWriteFrames(audioPort, buf, samplingRate);

	while (alGetFilled(audioPort) > 0)
		sginap(1);
	alClosePort(audioPort);
	alFreeConfig(config);
}	
	
