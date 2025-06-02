#include <audio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>
#include <stdlib.h>

/*
 * Set a crystal-based sample rate on a device.
 */
main(int argc, char **argv)
{
     int fd;
     int rv;
     double rate;
     ALpv x[2];
     
     if (argc != 3) {
	printf("usage: %s <device> <rate>\n",argv[0]);
	exit(-1);
     }
     rv = alGetResourceByName(AL_SYSTEM,argv[1],AL_DEVICE_TYPE);
     if (!rv) {
	printf("invalid device\n");
	exit(-1);
     }

     /* convert argv[2] to double-precision FP */
     rate = atof(argv[2]);

     /*
      * Now set the nominal rate to the given number, and 
      * set AL_MASTER_CLOCK to be AL_CRYSTAL_MCLK_TYPE.
      */
     x[0].param = AL_RATE;
     x[0].value.ll = alDoubleToFixed(rate);
     x[1].param = AL_MASTER_CLOCK;
     x[1].value.i = AL_CRYSTAL_MCLK_TYPE;

     if (alSetParams(rv,x, 2)<0) {
	printf("setparams failed: %s\n",alGetErrorString(oserror()));
     }
     if (x[0].sizeOut < 0) {
	/*
	 * Not all devices will allow setting of AL_RATE (for example, digital 
	 * inputs run only at the frequency of the external device).  Check
	 * to see if the rate was accepted.
	 */
	printf("AL_RATE was not accepted on the given resource\n");
     }

     if (alGetParams(rv,x, 1)<0) {
	printf("getparams failed: %s\n",alGetErrorString(oserror()));
     }
     printf("rate is now %lf\n",alFixedToDouble(x[0].value.ll));
}
