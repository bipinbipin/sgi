#include <audio.h>
#include <math.h>

#define PI 3.141592653589793

/*
 * This is a very simple 8-channel output program, which
 * outputs different frequencies of sine-waves on each of 8
 * output channels.
 * 
 * The architecture of the AL allows this to run on any device;
 * on devices with fewer than 8 channels, the back channels will
 * be dropped.
 */
main(int argc, char **argv)
{
	double f=0;
	ALconfig c;
	ALport p;
	double phase_inc,phase;
	int i,j;
	int dev = AL_DEFAULT_OUTPUT;	    /* start with default output */
	short buf[16384];		    /* 2048 8-channel sample frames */
	
	/*
	 * set the phase increment so that we complete 20 cycles every
	 * 2048 sample frames, giving a fundamental (@ 48kHz) of:
	 * 
	 *	20 cycles/2048 frames * 48000 frames/sec = 468.75 cycles/sec
	 *
	 */
	phase_inc = 2.0 * PI * 20.0 / 2048.0;
	
	/*
	 * Make 8 channels of sine waves, each of which
	 * is the next harmonic of the fundamental frequency.
	 */
	for (i = 0,j=0; i < 2048; i++,j+=8) {
		buf[j] =   (int)(32767.0 * sin(phase));
		buf[j+1] = (int)(32767.0 * sin(2.0 * phase));
		buf[j+2] = (int)(32767.0 * sin(3.0 * phase));
		buf[j+3] = (int)(32767.0 * sin(4.0 * phase));
		buf[j+4] = (int)(32767.0 * sin(5.0 * phase));
		buf[j+5] = (int)(32767.0 * sin(6.0 * phase));
		buf[j+6] = (int)(32767.0 * sin(7.0 * phase));
		buf[j+7] = (int)(32767.0 * sin(8.0 * phase));
		phase += phase_inc;
	}

	if (argc > 2) {
	    printf("usage: %s [device]\n",argv[0]);
	    exit(-1);
	}
	else if (argc == 2) {
	    /*
	     * First, let "dev" be the resource given by the first argument.
	     * If the first argument happens to be the name of an interface,
	     * this will find the device that has that interface. 
	     * 
	     * This works because alGetResourceByName searches the resource hierarchy
	     * for the named item, then looks for the closest resource of
	     * the given type which can get to the resource it finds. Since
	     * we've specified AL_DEVICE_TYPE, it will try to find a device 
	     * which is, or can get to, the resource we specified.
	     */
	    
	    dev = alGetResourceByName(AL_SYSTEM, argv[1], AL_DEVICE_TYPE);
	    if (!dev) {
		printf("invalid device %s\n", argv[1]);
		exit(-1);
	    }
	}

	c = alNewConfig();
	alSetQueueSize(c,10000);		/* 10,000 sample frames */
	alSetChannels(c,8);			/* 8 channels per sample frame */
	alSetDevice(c, dev);			/* connect to the requested device,
						   or AL_DEFAULT_OUTPUT,  if none
						   given */
	p = alOpenPort("8chsine", "w", c);
	
	if (!p) {
	    printf("openport error %s\n", alGetErrorString(oserror()));
	    exit(-1);
	}
	
	while (1) {
	    alWriteFrames(p,buf,2048);
	}
}
