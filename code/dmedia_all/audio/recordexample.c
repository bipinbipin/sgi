#include <stdio.h>
#include <signal.h>
#include <dmedia/audio.h>
#include <dmedia/audiofile.h>

/*
 * small example program: "recordexample"
 *
 * record an AIFF-C file from an audio input port
 * stop recording when user sends an interrupt
 *
 * file is configured for 16-bit stereo data at the current
 *     sampling rate of the audio hardware
 *
 * usage: "recordexample <filename> [audio device]"
 *
 * If no audio device is specified, the default input is used.
 */
int      caught_sigint;

/*
 * catch interrupt signal
 */
static void
catch_sigint()
{
    caught_sigint++;
}

main(int argc, char **argv)
{
    ALpv	  pv;		      /* parameter/value pair		   */
    ALconfig      portconfig;         /* audio port configuration          */
    ALport        port;               /* audio port                        */
    AFfilesetup   filesetup;          /* audio file setup                  */
    AFfilehandle  file;               /* audio file handle                 */
    char         *filename;           /* audio file name                   */
    double        samprate;           /* audio file sampling rate          */
    int           numframeswrit;      /* number of frames written          */
    int           done;               /* flag                              */
    int           framesperbuf;       /* sample frames transfered per loop */
    short	  buf[50000];	      /* enough for 1/2 second stereo data @ 50kHz */
    int		  device;	      /* the audio input device to use     */

    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "Usage: %s <filename> [device]\n", argv[0]);
        exit(1);
    }

    sigset(SIGINT, catch_sigint);

    filename = argv[1];

    if (argc == 3) {
	int itf;
	
	/*
	 * A device or interface name was given on the command-line.
	 * 
	 * Find the device given,  or the device associated with the 
	 * interface given.
	 */
	 device = alGetResourceByName(AL_SYSTEM, argv[2], AL_DEVICE_TYPE);
	 if (!device) {
	    fprintf(stderr, "invalid device %s\n", argv[2]);
	    exit(-1);
	 }
    
	 /*
	  * Now see if the given name is an interface. If it is,
	  * set the interface on the device to be the interface.
	  *
	  * For example, if the user specifies "Microphone," and that
	  * is an interface on AnalogIn, set the input source on AnalogIn
	  * to be the microphone. In this case, device will be AnalogIn, 
	  * and itf will be microphone.
	  */
	 if (itf = alGetResourceByName(AL_SYSTEM, argv[2], AL_INTERFACE_TYPE)) {
	     ALpv p;
    
	     p.param = AL_INTERFACE;
	     p.value.i = itf;
	     if (alSetParams(device, &p, 1) < 0 || p.sizeOut < 0) {
		printf("set interface failed\n");
	     }
	 }

    }
    else {
	device = AL_DEFAULT_INPUT;
    }
    
    /*
     * get the nominal input sample rate, if known (in some cases
     * when we are receiving a sample rate from an external digital 
     * device,  the sample rate may not be known).
     */
    pv.param = AL_RATE;
    if (alGetParams(AL_DEFAULT_INPUT, &pv, 1) < 0)  {
	fprintf(stderr, "alGetParams failed: %s\n", alGetErrorString(oserror()));
	exit(-1);
    }
    
    /*
     * the sample rate comes back as a 64-bit fixed-point value, in the "ll" (long long)
     * field of the value. Convert it to a double for ease of use.
     */
    samprate = alFixedToDouble(pv.value.ll);
    
    /*
     * Check to make sure that the sample rate works for us.
     */
    if (samprate > 50000) {
	fprintf(stderr, "Sample rate is too high!\n", samprate);
	exit(-1);
    }
    else if (samprate < 0) {
	fprintf(stderr, "Couldn't determine sample rate!\n", samprate);
	exit(-1);
    }
    
    /*
     * compute the number of audio sample frames in 1/2 second.
     */
    framesperbuf   = ((int) samprate) / 2;    /* half second buffer */
    
    /* 
     * configure an audio file. We'll create an AIFF-C file. But note
     * that the Audio File library handles many other file formats; see
     * the afIntro man page for more information.
     */
    afSetErrorHandler(NULL);	    /* turn off default error reporting */

    filesetup    = afNewFileSetup();
    afInitFileFormat(filesetup, AF_FILE_AIFFC);
    afInitChannels(filesetup, AF_DEFAULT_TRACK,  2);	    /* stereo (the default) */
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
     * open an audio input port. As soon as we do this, it begins to
     * fill with samples. That's why we create the file first; if, for
     * some reason, opening the audio file takes a while (e.g. for network
     * mounted file),  we won't lose audio data.
     */
    portconfig    = alNewConfig();
    alSetChannels(portconfig, 2);		    /* stereo (the default) */
    alSetWidth(portconfig, AL_SAMPLE_16);	    /* 16 bit (the default) */
    alSetQueueSize(portconfig, framesperbuf);
    alSetDevice(portconfig, device);		    /* the requested device */
    port = alOpenPort(argv[0], "r", portconfig);
    if (!port) {
	fprintf(stderr, "alOpenPort failed: %s\n", alGetErrorString(oserror()));
	exit(-1);
    }
    
    /*
     * loop recording to the file.
     */
    done = 0;
    caught_sigint = 0;
    while (!done && !caught_sigint)
    {
        alReadFrames(port, buf, framesperbuf);
        if ((numframeswrit 
                 = afWriteFrames(file, AF_DEFAULT_TRACK, 
                             buf, framesperbuf)) < framesperbuf) {
            done++;
        }
    }

    afCloseFile(file);   /* this is important: it updates the file header */
    alClosePort(port);
    exit(0);
}
