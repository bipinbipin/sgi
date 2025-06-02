#include <audio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <stdio.h>

main(int argc, char **argv)
{
     int rv;
     ALpv x[4];
     ALpv y[2];
     int i;
     char mname[32];
     
     if (argc > 3 || argc < 2) {
	printf("usage: %s <device>\n",argv[0]);
	exit(-1);
     }
     rv = alGetResourceByName(AL_SYSTEM, argv[1], AL_DEVICE_TYPE);
     
     if (!rv) {
	printf("invalid device\n");
	exit(-1);
     }

	/*
	 * Get the nominal rate of the device, what fraction
	 * of the master clock this is, and which master clock is 
	 * being used.
	 */
	 x[0].param = AL_RATE;
	 x[1].param = AL_RATE_FRACTION_N;
	 x[2].param = AL_RATE_FRACTION_D;
	 x[3].param = AL_MASTER_CLOCK;
	 if (alGetParams(rv,x, 4)<0) {
	    printf("getparams failed: %d\n",oserror());
	 }
	 else {
	    /*
	     * Get the text name of the master clock and its nominal rate.
	     */
	    y[0].param = AL_RATE;
	    y[1].param = AL_LABEL;
	    y[1].value.ptr = mname;
	    y[1].sizeIn = 32;
	    alGetParams(x[3].value.i,y,2);

	    /*
	     * Print it all out
	     */
	    printf("%lf Hz = (MCLK '%s' @ %lf Hz ) * (%d/%d)          ",
		    alFixedToDouble(x[0].value.ll),
		    mname,
		    alFixedToDouble(y[0].value.ll),
		    x[1].value.i,
		    x[2].value.i);
	 }
}
