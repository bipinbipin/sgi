#include <audio.h>
#include <sys/hdsp.h>
#include <stdio.h>

/*
 * An example of how to use alGetParamInfo, gains, and vector
 * parameters.
 *
 * This simple program prints out the gain on a given interface.
 * The program also prints information about the supported range
 * of values for gain.
 */
main(int argc, char **argv)
{
     int rv;
     ALpv x[4];
     ALpv y;
     ALparamInfo pi;
     ALfixed gain[8];
     int i;
     char min[30];
     
     if (argc > 3 || argc < 2) {
	printf("usage: %s <device>\n",argv[0]);
	exit(-1);
     }

     /*
      * Get a device associated with the given name. Really
      * gain is an interface parameter, but the device will 
      * treat gain parameters as though they were directed to
      * the currently selected interface on that device.
      */
     rv = alGetResourceByName(AL_SYSTEM, argv[1], AL_DEVICE_TYPE);
     if (!rv) {
	printf("invalid device\n");
	exit(-1);
     }

     /*
      * Now get information about the supported values for 
      * gain.
      */
     alGetParamInfo(rv, AL_GAIN, &pi);

     /*
      * One of the "special" values not described in the normal
      * min->max range is negative infinity. See if this value
      * is supported.
      */
     if (pi.specialVals & AL_NEG_INFINITY_BIT) {
	 sprintf(min, "-inf, %lf", alFixedToDouble(pi.min.ll));
     }
     else {
	 sprintf(min, "%lf", alFixedToDouble(pi.min.ll));
     } 

     /*
      * Print out the gain range.
      */
     printf("min: %s dB; max: %lf dB; min delta: %lf dB\n\n", 
	min,alFixedToDouble(pi.max.ll), alFixedToDouble(pi.minDelta.ll));


     /*
      * Now get the current value of gain
      */
     x[0].param = AL_GAIN;
     x[0].value.ptr = gain;
     x[0].sizeIn = 8;			/* we've provided an 8-channel
					   vector */
     x[1].param = AL_CHANNELS;
     if (alGetParams(rv,x, 2)<0) {
	printf("getparams failed: %d\n",oserror());
     }
     else {
	if (x[0].sizeOut < 0) {
	    printf("AL_GAIN was an unrecognized parameter\n");
	}
	else {
	    printf("(%d channels) gain:\n	",x[0].sizeOut);
	    for (i = 0; i < x[0].sizeOut; i++) {
		printf("%lf dB ", alFixedToDouble(gain[i]));
	    }
	    printf("\n");
	}
     }
}
