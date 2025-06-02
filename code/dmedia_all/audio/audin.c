#include <audio.h>
#include <stdio.h>
#include <getopt.h>
#include <malloc.h>
#include <stdlib.h>

#define FORCE_RATE 1
#define SET_RATE   2

/*
 * Audin
 *
 * A small program to deliver audio data on standard out.
 * Audin will provide interleaved data
 *
 * Audin takes several arguments:
 *	-c	number of channels. Can be any number supported
 *		by the audio library; it's not hard-coded. Defaults
 *		to 2.
 *	-d	name of audio device. Defaults to DefaultIn.
 *	-f	format. Can be f, d, 8, 16, 24, or 32. 24 and 32 both
 *		    provide 24-bit data sign-extended to 32 bits. 'f'
 *		    and 'd' correspond to 'float' and 'double,' 
 *		    respectively. 
 *	-r|s	sample-rate. Sets the hardware sample rate, in Hz.
 *		(-s is the same as -r, but forces the sample rate to be
 *		set even if there are other users)
 * 	-b	buffer size, in seconds.
 */

main(int argc, char **argv)

{
    extern char *optarg;
    extern int optind;
    int errflg = 0;
    int ch;
    int nchans = 2;
    int dev = AL_DEFAULT_INPUT;
    int itf;
    int wsize_in;
    int wsize = AL_SAMPLE_16;
    int sampfmt = AL_SAMPFMT_TWOSCOMP;
    int set_rate = 0;
    int bytes_per_word=2;
    int bufbytes;
    int bufframes;
    double rate;
    ALconfig c;
    ALport   p;
    ALpv     pv;
    void *buf;
    double buf_secs = 0.25;
    
    while ((ch = getopt(argc, argv, "c:d:f:r:s:b:")) != -1)
	switch (ch) {
	    case 'c':
		nchans = atoi(optarg);
		if (nchans <= 0) {
		    fprintf(stderr, "invalid number of channels; must be >= 0\n");
		    exit(-1);
		}
		break;
		
	    case 'd':

		/*
		 * Get a device,  and optionally select an interface.
		 */
		dev = alGetResourceByName(AL_SYSTEM, optarg, AL_DEVICE_TYPE);
		if (!dev) {
		    fprintf(stderr, "invalid device %s\n", optarg);
		    exit(-1);
		}
		
		if (itf = alGetResourceByName(AL_SYSTEM, optarg, AL_INTERFACE_TYPE)) {
		    ALpv p;
		    
		    p.param = AL_INTERFACE;
		    p.value.i = itf;
		    if (alSetParams(dev, &p, 1) < 0 || p.sizeOut < 0) {
			fprintf(stderr,"set interface failed\n");
		    }
		}
		break;
	    
	    case 'f' :
		if (isdigit(*optarg)) {
		    /*
		     * Get a wordsize.
		     */
		    wsize_in = atoi(optarg);
		    switch(wsize_in) {
			case 8:
			    wsize = AL_SAMPLE_8;
			    bytes_per_word = 1;
			    break;
			case 16:
			    wsize = AL_SAMPLE_16;
			    bytes_per_word = 2;
			    break;
			case 24:
			case 32:
			    wsize = AL_SAMPLE_24;
			    bytes_per_word = 4;
			    break;
			default:
			    fprintf(stderr, "invalid wordsize %d\n", wsize_in);
			    exit(-1);
		    }	
		}
		else {
		    if (*optarg == 'f') {
			sampfmt = AL_SAMPFMT_FLOAT;
			bytes_per_word = 4;
		    }
		    else if (*optarg == 'd') {
			sampfmt = AL_SAMPFMT_DOUBLE;
			bytes_per_word = 8;
		    }
		    else {
			fprintf(stderr, "invalid format %s\n", optarg);
			exit(-1);
		    }
		}
		break;
	    case 'r':
	    case 's':
		rate = atof(optarg);
		if (rate == 0.0) {
		    fprintf(stderr, "invalid rate '%s', ignoring\n", optarg);
		}
		else {
		    set_rate = (ch == 's') ? FORCE_RATE : SET_RATE ;
		}
		break;
	    case 'b':
		buf_secs = atof(optarg);
		if (buf_secs <= 0.0) {
		    fprintf(stderr, "invalid bufsize '%s', ignoring\n", optarg);
		    buf_secs = 0.25;
		}
		break;

	    case '?':
		errflg++;
	}
	if (errflg) {
	    fprintf(stderr, "usage: %s [-f format (8|16|24|32|f|d)] [-d device] [-c nchannels] [-b buf_secs] [-s|r sample_rate]\n");
	    exit (-1);
	}

	/*
	 * If a rate was specified, and no one else is using the
	 * device, attempt to set the sample rate for the device. 
	 * Right now we do not support setting other than
	 * a crystal-based (internal) master-clock.
	 */
	if (set_rate) {
	    ALpv rpv[2];
	    double old_rate;

	    pv.param = AL_RATE;
	    if (alGetParams(dev, &pv, 1) < 0) {
	        fprintf(stderr, "alGetParams failed: %s\n", alGetErrorString(oserror()));
	        exit(-1);
	    }
	    old_rate = alFixedToDouble(pv.value.ll);

	    /* 
	     * See if the device is in use
	     */
	    rpv[0].param = AL_PORT_COUNT;
	    if (alGetParams(dev, rpv, 1) < 0) {
	        fprintf(stderr, "alSetParams failed: %s\n", alGetErrorString(oserror()));
	        exit(-1);
	    }

	    if (old_rate != rate && rpv[0].value.i && set_rate == SET_RATE) {
	        fprintf(stderr, "device is already in use at another rate; not setting. Use -s <rate> to force\n");
	    }
	    else {
	        rpv[0].param = AL_RATE;
	        rpv[0].value.ll = alDoubleToFixed(rate);
	        rpv[1].param = AL_MASTER_CLOCK;
	        rpv[1].value.i = AL_CRYSTAL_MCLK_TYPE;

	        if (alSetParams(dev, rpv, 2) < 0) {
	            fprintf(stderr, "alSetParams failed: %s\n", alGetErrorString(oserror()));
	            exit(-1);
	        }
	    }
	}

	/* 
	 * Get the sample rate for the given device. If we set the rate, this may not be
	 * exactly what we set, because not all devices support all rates.
	 */
	pv.param = AL_RATE;
	if (alGetParams(dev, &pv, 1) < 0) {
	    fprintf(stderr, "alGetParams failed: %s\n", alGetErrorString(oserror()));
	    exit(-1);
	}
	rate = alFixedToDouble(pv.value.ll);

	/*
	 * check the validity of the returned rate.
	 * rate < 0 means the AL_RATE parameter was accepted by the device, but that
	 *	the device cannot figure out its nominal rate (perhaps because it's 
	 *	clocked off of an external source of unknown rate). We also prune
	 *	out rate = 0.
	 * pv.sizeOut < 0 means that the AL_RATE parameter was unsupported on the
	 *	given resource (awfully darn unlikely but we check anyway).
	 */
	if (rate <= 0 || pv.sizeOut < 0) {
	    fprintf(stderr, "unable to get sample rate: assuming 48kHz\n");
	    rate = 48000.0;
	}

	/*
	 * Figure out the number of sample-frames and bytes in 1/4 second of 
	 * audio.
	 */
	bufframes = (int) (rate * buf_secs);
	bufbytes = bufframes * nchans * bytes_per_word;

	/*
	 * Attempt to create the config. The config is what we use to specify
	 * the data format and device for the audio port we'll open later.
	 */
	c = alNewConfig();
	if (!c) {
	    fprintf(stderr, "alNewConfig failed: %s\n", alGetErrorString(oserror()));
	    exit(-1);
	}

	/*
	 * Set the queuesize to be large enough to hold our buffer. It doesn't 
	 * need to be larger than this (in fact, it can be even smaller).
	 */
	if (alSetQueueSize(c, bufframes) < 0) {
	    fprintf(stderr, "alSetQueueSize failed: %s\n", alGetErrorString(oserror()));
	    exit(-1);
	}

	/*
	 * Set the number of channels on the port. Even if we give this a bad number
	 * of channels, it may not fail until we actually try to open the port.
	 */
	if (alSetChannels(c, nchans) < 0) {
	    fprintf(stderr, "alSetChannels failed: %s\n", alGetErrorString(oserror()));
	    exit(-1);
	}

	/*
	 * Set the queue size to 
	 * of channels, it may not fail until we actually try to open the port.
	 */
	if (alSetChannels(c, nchans) < 0) {
	    fprintf(stderr, "alSetChannels failed: %s\n", alGetErrorString(oserror()));
	    exit(-1);
	}

	/*
	 * Set the sample format (two's comp, float, or double).
	 */
	if (alSetSampFmt(c, sampfmt) < 0) {
	    fprintf(stderr, "alSetSampFmt failed: %s\n", alGetErrorString(oserror()));
	    exit(-1);
	}

	/*
	 * Set the sample wordsize. This is ignored for float or double, but we set it
	 * anyway.
	 */
	if (alSetWidth(c, wsize) < 0) {
	    fprintf(stderr, "alSetWidth failed: %s\n", alGetErrorString(oserror()));
	    exit(-1);
	}

	/*
	 * Set the device. This won't fail if there is some problem with the device,
	 * but the ensuing alOpenPort will fail.
	 */
	if (alSetDevice(c, dev) < 0) {
	    fprintf(stderr, "alSetDevice failed: %s\n", alGetErrorString(oserror()));
	    exit(-1);
	}

	/*
	 * Now actually try to open an audio port with the config we've set up.
	 */
	p = alOpenPort("audin", "r", c);
	if (!p) {
	    fprintf(stderr, "alOpenPort failed: %s\n", alGetErrorString(oserror()));
	    exit(-1);
	}

	/*
	 * Allocate 1/4-second buffer
	 */
	buf = (void *) malloc(bufbytes);
	if (!buf) {
	    fprintf(stderr, "unable to allocate sample buffer\n");
	    exit(-1);
	}

	/*
	 * read & write data
	 */
	while (1) {
	    alReadFrames(p,buf,bufframes);
	    if (write(1,buf,bufbytes) < 0) {
	        fprintf(stderr, "unable to write to stdout\n");
		exit(-1);
	    }
	}

	alClosePort(p);
}
