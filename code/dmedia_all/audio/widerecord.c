#include <stdio.h>
#include <signal.h>
#include <dmedia/audio.h>
#include <dmedia/audiofile.h>

#define MAXDEVS 4
#define MAXCHANS_PER_DEV 8
#define MAXCHANS (MAXCHANS_PER_DEV*MAXDEVS)

/*
 * small example program: "widerecord"
 *
 * record an AIFF-C file from a set of audio input ports.
 * stop recording when user sends an interrupt
 *
 * file is configured for the sum of the numbers of channels
 *     for the given devices.
 *
 * This program is a little bit simplified in that it assumes
 * the sample-rates of the devices are already synchronized. This
 * means that if multiple digital inputs are used, this program
 * expects them to come from sync'ed sources,  or if
 * analog inputs are used, they must all have exactly the same sample-rate.
 *
 * usage: "widerecord <filename> device1 device2 device3 ..."
 *
 * If no audio device is specified, the default input is used.
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
device_setup(devinfo_t *dev)
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

    alp = alOpenPort("widerecord","r",c);
    if (!alp) {
	return 0;
    }

    alFreeConfig(c);

    dev->p = alp;
    return dev->nchans;
}

void 
sync_ports(devinfo_t *port, int nports)
{
	double nanosec_per_frame = (1000000000.0)/samprate;
	stamp_t buf_delta_msc;
	stamp_t msc[MAXDEVS];
	stamp_t ust[MAXDEVS];
	int corrected;
	int i;
	
	/* 1 port is implicitly synchronized with itself */
	if (nports == 1) 
	    return;
	    
	/*
	 * Get UST/MSC (time & sample frame number) pairs for the
	 * device associated with each port.
	 */
	for (i = 0; i < nports; i++) {
	    alGetFrameTime(port[i].p, &msc[i], &ust[i]);
	}

	/*
	 * We consider the first device to be the "master" and we
	 * calculate, for each other device, the sample frame number
	 * on that device which has the same time as ust[0] (the time
	 * at which sample frame msc[0] went out on device 0).
	 * 
	 * From that, we calculate the sample frame offset between
	 * contemporaneous samples on the two devices. This remains
	 * constant as long as the devices don't drift.
	 *
	 * If the port i is connected to the same device as port 0, you should
	 * see offset[i] = 0.
	 */
	
	port[0].offset = 0;	    /* by definition */
	for (i = 1; i < nports; i++) {
	    stamp_t msc0 = msc[i] + (stamp_t)((double) (ust[0] - ust[i]) / (nanosec_per_frame));
	    port[i].offset = msc0 - msc[0];
	}

	do {
	    stamp_t max_delta_msc;
	    corrected = 0;
	    
	    /*
	     * Now get the sample frame number associated with
	     * each port.
	     */
	    for (i = 0; i < nports; i++) {
		alGetFrameNumber(port[i].p, &msc[i]);
	    }

	    max_delta_msc = msc[0];
	    
	    for (i = 1; i < nports; i++) {
		/*
		 * For each port, see how far ahead or behind
		 * the first port we are, and keep track of the
		 * minimum. in a moment, we'll bring all the ports to this
		 * number.
		 */
		buf_delta_msc = msc[i] - port[i].offset;
		if (max_delta_msc < buf_delta_msc) {
		    max_delta_msc = buf_delta_msc;
		}
	    }
	    
	    for (i = 0; i < nports; i++) {
		buf_delta_msc = msc[i] - port[i].offset;
		if (buf_delta_msc != max_delta_msc) {
			
		    alDiscardFrames(port[i].p, (int)(max_delta_msc-buf_delta_msc));
		    corrected++;
		}
	    }
	} while (corrected);
}

void
check_sync(devinfo_t *port, int nports)
{
    stamp_t msc[MAXDEVS];
    int need_sync = 0;
    int i;
    stamp_t buf_delta_msc;
    stamp_t ref_delta_msc;

    if (nports == 1) {
	return;			/* 1 port is implicitly synced to itself */
    }
    
    /*
     * Get the sample frame number associated with
     * each port.
     */
    for (i = 0; i < nports; i++) {
	alGetFrameNumber(port[i].p, &msc[i]);
    }

    ref_delta_msc = msc[0];
    
    for (i = 1; i < nports; i++) {
	/*
	 * For each port, see how far ahead or behind
	 * the first port we are, and keep track of the
	 * maximum. in a moment, we'll bring all the ports to this
	 * number.
	 */
	buf_delta_msc = msc[i] - port[i].offset;
	if (buf_delta_msc != ref_delta_msc) {
	    need_sync++;
	}
    }
    
    if (need_sync) {
	sync_ports(port,nports);
    }
}


main(int argc, char **argv)
{
    ALpv	  pv;		      /* parameter/value pair		   */
    ALconfig      portconfig;         /* audio port configuration          */
    ALport        port;               /* audio port                        */
    AFfilesetup   filesetup;          /* audio file setup                  */
    AFfilehandle  file;               /* audio file handle                 */
    char         *filename;           /* audio file name                   */
    int           numframeswrit;      /* number of frames written          */
    int 	  nchans;
    int           done;               /* flag                              */
    int           framesperbuf;       /* sample frames transfered per loop */
    short	  *buf;
    int		  device;	      /* the audio input device to use     */
    devinfo_t	  dev[MAXDEVS];
    int 	  ndevs,i;
    void	  *bufps[MAXCHANS];
    int	  	  strides[MAXCHANS_PER_DEV];
    char	  *defaultin="DefaultIn";

    if (argc < 2 || argc > 2 + MAXDEVS) {
        fprintf(stderr, "Usage: %s <filename> [device1 device2 ...]\n", argv[0]);
        exit(1);
    }

    filename = argv[1];

    sigset(SIGINT, catch_sigint);

    if (argc > 2) {
        ndevs = argc-2;
        for (i = 0; i < argc-2; i++) {
	    int tnc; 
	    dev[i].name = argv[i+2];
	    if (tnc = device_setup(&dev[i])) {
		/* keep track of the total number of channels */
		nchans += tnc;
	    }
	    else {
		fprintf(stderr, "could not open device %s\n",dev[i].name);
		exit(-1);
	    }
        }
    }
    else {
	dev[0].name = defaultin;
	if (device_setup(dev) == 0) {
	    fprintf(stderr, "could not open default device\n");
	    exit(-1);
	}
    }

    printf("nchans=%d\n",nchans);
    
    /*
     * Assume all devices have the same sample-rate.
     */
    pv.param = AL_RATE;
    alGetParams(dev[0].resid, &pv, 1);
    samprate = alFixedToDouble(pv.value.ll);
    
    /*
     * Check to make sure that the sample rate works for us.
     */
    if (samprate > 50000) {
	fprintf(stderr, "Sample rate is too high!\n", samprate);
	exit(-1);
    }
    else if (samprate <= 0) {
	fprintf(stderr, "Couldn't determine sample rate!\n", samprate);
	exit(-1);
    }
    
    /*
     * compute the number of audio sample frames in 1/8 second.
     */
    framesperbuf   = ((int) samprate) / 8;    /* half second buffer */

    buf = (short *)malloc(framesperbuf*nchans*sizeof(short));
    if (!buf) {
	printf("failed to allocate sample buffer\n");
	exit(-1);
    }

    /*
     * now set up the buffer pointer and stride values for
     * alReadBuffers.
     */
    for (i = 0; i < nchans; i++) {
	bufps[i] = &buf[i];
    }
    for (i = 0; i < MAXCHANS_PER_DEV; i++) {
	strides[i] = nchans;
    }
    
    /* 
     * configure an audio file. We'll create an AIFF-C file. But note
     * that the Audio File library handles many other file formats; see
     * the afIntro man page for more information.
     */
    afSetErrorHandler(NULL);	    /* turn off default error reporting */

    filesetup    = afNewFileSetup();
    afInitFileFormat(filesetup, AF_FILE_AIFFC);
    afInitChannels(filesetup, AF_DEFAULT_TRACK,  nchans);    /* stereo (the default) */
    afInitRate(filesetup, AF_DEFAULT_TRACK, samprate);
    afInitSampleFormat(filesetup, AF_DEFAULT_TRACK, 
	AF_SAMPFMT_TWOSCOMP, 16);	    /* 16 bits, 2's complement (the default) */
		      
    /*
     * open the audio file
     */
    file = afOpenFile(filename, "w", filesetup);
    if (!file) {
	fprintf(stderr, "afOpenFile failed: %s\n", dmGetError(NULL, NULL));
	exit(-1);
    }
    
    /*
     * Synchronize the ports.
     */
    sync_ports(dev, ndevs);

    /*
     * loop recording to the file.
     */
    done = 0;
    caught_sigint = 0;
    while (!done && !caught_sigint)
    {
	int chan = 0;	/* keeps track of starting channel for each port */

	for (i = 0; i < ndevs; i++) {
	    /*
	     * read data from port i into a set of channels
	     * in buf.
	     */
	    alReadBuffers(dev[i].p, &bufps[chan], strides, framesperbuf);
	    chan += dev[i].nchans;
	}
	afWriteFrames(file, AF_DEFAULT_TRACK, buf, framesperbuf);

	/*
	 * Check to see if we're still in sync, and re-sync if necessary.
	 * We don't really have to do this every time. Here, since our blocks 
	 * are fairly large, this overhead is no problem. But if we were writing
	 * a low-latency application (small blocks), we might only
	 * want to do this every once in a while.
	 */
	check_sync(dev,ndevs);
    }

    afCloseFile(file);   /* this is important: it updates the file header */
    exit(0);
}
