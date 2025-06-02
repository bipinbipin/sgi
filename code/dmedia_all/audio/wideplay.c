#include <stdio.h>
#include <signal.h>
#include <dmedia/audio.h>
#include <dmedia/audiofile.h>

#define MAXCHANS_PER_DEV 8
#define MAXCHANS 64

/*
 * small example program: "wideplay"
 * 
 * Play a wide (many channels) file on an audio device,
 * mixing down to however many channels the device has.
 *
 * The audio file reading and writing are fairly straightforward.
 * Of potential interest is the mixer, which does general
 * M->N channel mixing, and is fairly efficient.
 *
 * usage: "wideplay <file> <dev>"
 */
int      caught_sigint;

double        samprate;           /* audio file sampling rate          */

typedef struct devinfo_s {
    int resid;		/* resource ID of device */
    char *name;		/* name of device */
    int nchans;		/* number of channels */
    ALport p;		/* audio port connected to this device */
    stamp_t offset;
} devinfo_t;

/*
 * catch interrupt signal
 */
static void
catch_sigint()
{
    caught_sigint++;
}

int
device_setup(devinfo_t *dev, char *direction)
{
    int r;
    ALport alp;
    ALconfig c;
    ALpv p;

    /* get the device associated with the given name */
    r = alGetResourceByName(AL_SYSTEM, dev->name, AL_DEVICE_TYPE);
    if (!r) {
	return 0;
    }
    dev->resid = r;

    /* if the name refers to an interface, select that interface on the device */
    if (r = alGetResourceByName(AL_SYSTEM, dev->name, AL_INTERFACE_TYPE)) {
	p.param = AL_INTERFACE;
	p.value.i = r;
	alSetParams(dev->resid, &p, 1);
    }

    c = alNewConfig();
    if (!c) {
	return 0;
    }

    /* find out how many channels the device is. */
    p.param = AL_CHANNELS;
    if (alGetParams(dev->resid, &p, 1) < 0 || p.sizeOut < 0) {
	/* AL_CHANNELS unrecognized or GetParams otherwise failed */
	return 0;
    }

    dev->nchans = p.value.i;

    alSetChannels(c, dev->nchans);
    alSetDevice(c, dev->resid);
    alSetSampFmt(c, AL_SAMPFMT_FLOAT);

    alp = alOpenPort("monitor",direction,c);
    if (!alp) {
	return 0;
    }

    alFreeConfig(c);

    dev->p = alp;
    return dev->nchans;
}

/*
 * Simple mixer. Mix m input channels into n output channels.
 * Requires n = multiple of 2.
 *
 * a is the audio input data, interleaved.
 *   it is therefore a matrix of nsamps X m     in column-major order
 *   this is the same format used by the interleaved AL and AF calls.
 * b is the mix matrix.
 *   it is m X n                                in row-major order  (see below!)
 * c is the result, interleaved.
 *   it will be nsamps X n                      in column-major order
 *   this is the same format used by the interleaved AL and AF calls.
 *
 * (row-major indicates that all elements of a row are stored contiguously,
 * column-major that all elements of a column are stored contiguously).
 *
 * This is a reasonably fast m x n matrix multiplier for general m and 
 * any n which is a multiple of 2; however, you can speed it up if you only 
 * care about a specific pair of m and n: just write out the equation 
 * for that matrix multiply, i.e. completely unroll the inner 2 loops below.
 *
 * For clarification, the mix matrix is stored like this:
 *		(amount of input channel 1 to send to output channel 1)
 *		(amount of input channel 2 to send to output channel 1)
 *		[...]
 *		(amount of input channel M to send to output channel 1)
 *		(amount of input channel 1 to send to output channel 2)
 *		(amount of input channel 2 to send to output channel 2)
 *		[...]
 *		(amount of input channel M to send to output channel 2)
 *		[...]
 *		(amount of input channel 1 to send to output channel N)
 *		[...]
 *		(amount of input channel M to send to output channel N)
 */
void
matmult(const float *a, const float *b, float *c, int nframes, int m, int n)
{
    int i, j, k;
    for (i = 0; i < nframes; i++) {
        const float *bcol=b;
        for (j = 0; j < n; j+=2) {
            float acc1 = 0.0f;
            float acc2 = 0.0f;
            for (k = 0; k < m; k++) {
                acc1 += a[k] * bcol[k];     /* computing c[j] */
                acc2 += a[k] * bcol[k+m];   /* computing c[j+1] */
            }
            c[j] = acc1;
            c[j+1] = acc2;
            bcol += 2*m;
        }
        a += m;
        c += n;
    }
}

main(int argc, char **argv)
{
    ALpv	  pv[2];	      /* parameter/value pair		   */
    ALconfig      portconfig;         /* audio port configuration          */
    ALparamInfo   pinfo;	      /* audio parameter information */
    ALport        port;               /* audio port                        */
    AFfilesetup   filesetup;          /* audio file setup                  */
    AFfilehandle  file;               /* audio file handle                 */
    char         *filename;           /* audio file name                   */
    int           numframeswrit;      /* number of frames written          */
    int 	  nchans;
    int           done;               /* flag                              */
    int           framesperbuf;       /* sample frames transfered per loop */
    float	  *buf;
    float	  *outbuf;
    int		  device;	      /* the audio input device to use     */
    devinfo_t	  outdev;
    int 	  ndevs,i;
    char	  *defaultin="DefaultIn";
    float	  *mixmat;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <filename> <device>\n",argv[0]);
        exit(1);
    }

    sigset(SIGINT, catch_sigint);

/*
    filesetup = afNewFileSetup();
    if (!filesetup) {
	printf("Could not create filehandle\n");
	exit(-1);
    }
    */

    file = afOpenFile(argv[1],"r",0);
    if (!file) {
	printf("could not open file %s\n",argv[1]);
	exit(-1);
    }
    afSetVirtualSampleFormat(file, AF_DEFAULT_TRACK, AF_SAMPFMT_FLOAT, 4);

    outdev.name = argv[2];
    if (device_setup(&outdev,"w") == 0) {
	printf("Could not open device %s\n",outdev.name);
    }

    nchans = afGetChannels(file,AF_DEFAULT_TRACK);
    printf("mixing %d down to %d\n",nchans,outdev.nchans);

    /*
     * Get the sample-rate of the file 
     */
    samprate = afGetRate(file, AF_DEFAULT_TRACK);

    /*
     * Check to see that the file sample rate is at least
     * in range for the clock-generator for this device. (Not all 
     * clock-generators can generate all rates, either, so even
     * if it's in range, the exact rate may not be supported -- in
     * that case, we'll just get the closest supported rate).
     */
    if (alGetParamInfo(outdev.resid, AL_RATE, &pinfo) < 0) {
	fprintf(stderr,"Can't get AL_RATE parameter info: %s\n",
		alGetErrorString(oserror()));
	exit(-1);
    }
    if (alFixedToDouble(pinfo.max.ll) < samprate ||
	alFixedToDouble(pinfo.min.ll) > samprate) {
	fprintf(stderr,"Sample rate is out of range (%.0lf-%.0lf Hz) for the selected device.\n",alFixedToDouble(pinfo.min.ll),alFixedToDouble(pinfo.max.ll));
	exit(-1);
    }
    

    /*
     * Set the sample-rate, relative to a crystal master clock,
     * on the chosen output device.
     */
    pv[0].param = AL_RATE;
    pv[0].value.ll = alDoubleToFixed(samprate);
    pv[1].param = AL_MASTER_CLOCK;
    pv[1].value.i = AL_CRYSTAL_MCLK_TYPE;
    alSetParams(outdev.resid, pv, 2);
    
    /*
     * compute the number of audio sample frames in 1/8 second.
     */
    framesperbuf   = ((int) samprate) / 4;    /* half second buffer */

    buf = (float *)malloc(framesperbuf*nchans*sizeof(float));
    if (!buf) {
	printf("failed to allocate input sample buffer\n");
	exit(-1);
    }

    outbuf = (float *)malloc(framesperbuf*outdev.nchans*sizeof(float));
    if (!outbuf) {
	printf("failed to allocate output sample buffer\n");
	exit(-1);
    }

    mixmat = (float *)malloc(nchans*outdev.nchans*sizeof(float));
    if (!mixmat) {
	printf("failed to allocate mix matrix\n");
	exit(-1);
    }

    /*
     * now set up the mix matrix.
     * In a "real" application we would want dynamic control over this
     * matrix, so that we could adjust gain and pan in real-time.
     *
     * Many mixers are hard-coded to stereo output. This isn't; 
     * it can mix any number of input channels to any number of output channels.
     * So we could use this to pan a bunch of input signals in stereo,
     * or around 4 channels, or 8, or ...
     */

    for (i = 0; i < nchans*outdev.nchans; i++) {
	/* pan every other input channel full left / full right */
	mixmat[i] = 0.4*((i+(i/nchans))&1);
    }
    
    /*
     * loop playing the file.
     */
    done = 0;
    caught_sigint = 0;
    while (!done && !caught_sigint)
    {
	int chan = 0;	/* keeps track of starting channel for each port */
	int frames;

	/*
	 * Read in a nchans of interleaved data in floating point in the range 
	 * [-1.0,1.0].
	 */
	frames = afReadFrames(file,AF_DEFAULT_TRACK,buf,framesperbuf);

	/*
	 * Mix down the data.
	 */
	matmult(buf,mixmat,outbuf,frames,nchans,outdev.nchans);

	/*
	 * At this point, the floating-point sample data may be out of
	 * the range [-1.0, 1.0], because we have mixed multiple channels
	 * together. alWriteFrames will limit the data to this range during 
	 * output, since we haven't explicitly disabled that with alSetLimiting(). 
	 * If we wanted to do something besides alWriteFrames we might need to limit 
	 * the data ourselves. 
	 */
	alWriteFrames(outdev.p, outbuf, frames);
	if (frames < framesperbuf) done++;
    }

    exit(0);
}
