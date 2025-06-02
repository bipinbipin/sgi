#include <audio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>
#include <stdlib.h>


/*
 * This brief example sets an AES (digital input)-based sample rate
 * on a named audio device. 
 *
 * Note that not all audio devices support AES-related sample rates.
 *
 * The salient thing to understand from this example is the manner 
 * in which the AL separates the notion of the "master clock," or
 * the timebase from which a sample-rate is generated, and the sample
 * rate itself. The master clock can be an internal crystal 
 * of type AL_CRYSTAL_MCLK_TYPE, an AES-derived timebase of 
 * type AL_AES_MCLK_TYPE, an ADAT-derived timebase of AL_ADAT_MCLK_TYPE,
 * or a video-related timebase of type AL_VIDEO_MCLK_TYPE.
 * If you're not sure which of these you need, use AL_CRYSTAL_MCLK_TYPE.
 * The others are for precise synchronization with external devices.
 * 
 * When setting a sample-rate, it's a good idea to 
 * always specify both the master clock and the sample rate itself,
 * expressed either as a rate in Hz (using the AL_RATE parameter), or
 * as a fraction of the master clock rate (using the AL_RATE_FRACTION_N
 * and AL_RATE_FRACTION_D pair, as we have done here).
 *
 * This example is equivalent to setting an old-style "AL_RATE_AES_1" sample rate.
 */
main(int argc, char **argv)
{
     int fd;
     int rv;
     double rate;
     int mclk;
     ALpv x[5];
     
     if (argc != 2) {
	printf("usage: %s <device>\n",argv[0]);
	exit(-1);
     }

     /*
      * Get the resource associated with the given device name.
      */
     rv = alGetResourceByName(AL_SYSTEM,argv[1],AL_DEVICE_TYPE);
     if (!rv) {
	printf("invalid device\n");
	exit(-1);
     }

     /*
      * Now set the master clock to be of AES type and set the rate to be (1/1)
      * times the AES input frequency.
      * 
      * Note that we are setting the master clock by *type* here. 
      * AL_MASTER_CLOCK accepts a resource. When the audio system sees a type 
      * specified instead, it will find an appropriate master clock of the
      * given type, if available, and use that. When we read back the value
      * of AL_MASTER_CLOCK after setting it, it will be a specific master 
      * clock resource.
      */
     x[0].param = AL_MASTER_CLOCK;
     x[0].value.i = AL_AES_MCLK_TYPE;
     x[1].param = AL_RATE_FRACTION_N;
     x[1].value.i = 1;
     x[2].param = AL_RATE_FRACTION_D;
     x[2].value.i = 1;
     if (alSetParams(rv,x, 3)<0) {
	printf("setparams failed: %s\n",alGetErrorString(oserror()));
     }

     /*
      * Now get the values for the same parameters, to see what actually
      * got set.
      */
     if (alGetParams(rv,x, 3)<0) {
	printf("getparams failed: %s\n",alGetErrorString(oserror()));
     }
     printf("rate set to mclk %d * (%d/%d)\n",x[0].value.i, x[1].value.i,x[2].value.i);
}
