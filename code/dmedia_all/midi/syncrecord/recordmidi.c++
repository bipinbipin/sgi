/* 
 * Synchronized MIDI + audio file recorder.
 * 
 * usage: recordmidi [-l] [filename]
 * 
 * default filename is "test.mid".  recordmidi will overwrite
 * the input file.
 *
 */

#include <limits.h>
#include <dmedia/midi.h>
#include "midifile.h"
#include <stdio.h>
#include <malloc.h>
#include <stropts.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <audio.h>
#include <audiofile.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/wait.h>
#include <getopt.h>

#define NCHANS 2
#define NSEC_PER_SEC_D (1000000000.0)

#define NEVENTS 10000

#if DEBUG
#define dbprintf    printf
#else
#define dbprintf    if(0)printf
#endif

int
get_input_rate()
{
    long buf[6];

    buf[0] = AL_INPUT_RATE;
    buf[2] = AL_INPUT_SOURCE;
    buf[4] = AL_DIGITAL_INPUT_RATE;
    ALgetparams(AL_DEFAULT_DEVICE,buf,6);

    if (buf[1] == AL_RATE_AES_1 || buf[3] == AL_INPUT_DIGITAL) {
        /*
         * we are clocked off of the digital input. find the
         * real input rate, if we can.
         */
        if (ALgetdefault(AL_DEFAULT_DEVICE,AL_DIGITAL_INPUT_RATE) >= 0) {
            return buf[5];
        }
    }
    else if (buf[1] > 0) {
        /*
         * input rate is in Hz and we're using an analog input -- return rate.
         */
        return buf[1];
    }
    return AL_RATE_UNDEFINED;
}

char prbuf[800];
struct pollfd pfds[3];		// check the midi & stdin fd's for termination
struct pollfd apfds[3];		// check the midi & stdin fd's for termination
struct stat statbuf;
int done = 0;
#define FNAME "test"


char         *myname;             /* name of this program              */
ALconfig      portconfig;         /* audio port configuration          */
ALport        port;               /* audio port                        */
long          portrate;           /* audio port sampling rate          */
AFfilesetup   filesetup;          /* audio file setup                  */
AFfilehandle  afile;              /* audio file handle                 */
double        filerate;           /* audio file sampling rate          */
void         *buf;                /* sample transfer buffer            */
int           numframeswrit;      /* number of frames written          */
int           samplesperbuf;      /* samples transfered per loop       */
int           framesperbuf;       /* sample frames transfered per loop */
int           samplespersec;      /* samples transfered per sec        */

char *optstring = "l";

void
audio_loop(void *)
{
    do {
	ALsetfillpoint(port, samplesperbuf);
	if (poll(apfds, 1, -1) < 0) {
	    perror("problem in poll");
	    _exit(-1);
	}
	
	if (apfds[0].revents & POLLIN) {
	    ALreadsamps(port, buf, samplesperbuf);
	    if ((numframeswrit = AFwriteframes(afile, AF_DEFAULT_TRACK, 
		buf, framesperbuf)) < framesperbuf) {
	    }
	    
	}
    } while (!done);
    
    printf("done with audio recording....\n");
    
    AFclosefile(afile);   /* this is important: it updates the file header */
    ALcloseport(port);
}

main(int argc, char **argv)
{
    MDport ip, op;
    MDevent event[NEVENTS];
    MFfile file;
    MFtrack *track[2];
    MFfileEvent *ev[NEVENTS];
    int tempo = 500000;
    int nev = 0;
    char *fname = FNAME;
    char mfname[40], afname[40];
    int	status;
    double srate;
    unsigned long long ust, msc, nust, nmsc;
    int i, loopthru = 0;
    
    myname = argv[0];
    while ((i = getopt(argc, argv, optstring)) != -1)
	switch (i) {
	  case 'l' :
	    loopthru = 1;
	    break;
	}
    if (optind != argc)
	fname = argv[optind];

    sprintf(mfname, "%s.mid", fname);
    sprintf(afname, "%s.aifc", fname);
    
    if (stat(mfname, &statbuf) < 0) {
	if (errno != ENOENT) {
	    perror("problem with midi file");
	    return -1;
	}
    } else {
	int ans;
	printf("clobber file %s? (y/n)", mfname);
	ans = getchar();
	if (ans == 'y') {
	    unlink(mfname);
	    unlink(afname);
	}
	else return 0;
    }
    
    /*
     * get the current input rate in use by the hardware.
     */
    if ((portrate = get_input_rate()) == AL_RATE_UNDEFINED) {
	fprintf(stderr, "Can't get the current input rate\n");
	exit(-1);
    }
    filerate = portrate;
    srate = (NSEC_PER_SEC_D/filerate);
    
    /*
     * compute the number of input samples equal to half a 
     * second and allocate a transfer buffer
     */
    samplespersec   = (portrate) * NCHANS;
    samplesperbuf   = samplespersec / 2;    /* half second buffer */
    framesperbuf    = samplesperbuf / NCHANS;    
    buf             = (short *)malloc(samplesperbuf * sizeof(short));
    
    
    /* 
     * configure an audio file 
     */
    filesetup = AFnewfilesetup();
    AFinitfilefmt(filesetup, AF_FILE_AIFFC); 
    AFinitchannels(filesetup, AF_DEFAULT_TRACK,  NCHANS);
    AFinitrate(filesetup, AF_DEFAULT_TRACK, filerate);
    AFinitsampfmt(filesetup, AF_DEFAULT_TRACK, 
                      AF_SAMPFMT_TWOSCOMP, 16); /* in bits */
		      
    /*
     * open the audio file
     */
    afile = AFopenfile(afname, "w", filesetup);
    if (!afile) {
	fprintf(stderr, "Could not open file %s\n", afname);
	exit(-1);
    }

    /*
     * set up the MIDI file
     */
    file.setFormat(1);
    file.setDivision(480);
    
    /* place a tempo event into track 0 */
    
    MFmeta *mev = new MFmeta();
    mev->type = MIDImeta_SetTempo;
    mev->msglen = 3;
    mev->msg = (char*)malloc(3);
    mev->msg[0] = (tempo >> 16) & 0xff;
    mev->msg[1] = (tempo >> 8) & 0xff;
    mev->msg[2] = (tempo & 0xff);
    mev->stamp = 0;
    
    MFfileEvent *fev = new MFfileEvent(mev);
    
    /* create a new track and put the single tempo event there */
    
    track[0] = new MFtrack();
    track[0]->insertEvent(fev);
    
    file.addTrack(track[0]);
    
    /*
     * create a second track. this is where we will place the MIDI events.
     */
    track[1] = new MFtrack();
    file.addTrack(track[1]);

    if (mdInit() <= 0) return 0;

    if ((ip = mdOpenInPort(0)) == 0) {
	perror("couldn't open inport");
	return -1;
    }

    pfds[0].fd = 0;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;

    pfds[1].fd = mdGetFd(ip);
    pfds[1].events = POLLIN;
    pfds[1].revents = 0;

    /*
     * open an output MIDI port on the default MIDI output interface.
     * in the case that the software synth is installed, the default
     * should be the software synth interface.
     */
    if ((op = mdOpenOutPort(0)) == 0) {
	perror("couldn't open outport");
	return -1;
    } 
    
    /*
     * open the audio port
     */
    portconfig    = ALnewconfig();
    ALsetchannels(portconfig, NCHANS);
    ALsetwidth(portconfig, AL_SAMPLE_16);
    ALseterrorhandler(0);	/* shut up default AL error handler */
    
    port = ALopenport(myname, "r", portconfig);
    if (!port) {
	int err = oserror();
	if (err == AL_BAD_NO_PORTS) {
	    fprintf(stderr, " System is out of audio ports\n");
	} else if (err == AL_BAD_DEVICE_ACCESS) {
	    fprintf(stderr, " Couldn't access audio device\n");
	} else if (err == AL_BAD_OUT_OF_MEM) {
	    fprintf(stderr, " Out of memory: port open failed\n");
	}
	exit(1);
    }
    apfds[0].fd = ALgetfd(port);
    apfds[0].events = POLLIN;

    /*
     * use relative tick mode which is amenable to standard MIDI files.
     * also, set the division and tempo for the port. as noted in the
     * man page, mdSetDivision() is called before mdSetTempo().
     */
    mdSetStampMode(ip, MD_RELATIVETICKS);
    mdSetDivision(ip, 480);
    mdSetTempo(ip, tempo);
    
    /*
     * set stamp mode on the output port to no stamp. this allows the
     * input messages to be passed immediately to the output port with
     * no manipulation.
     */
    mdSetStampMode(op, MD_NOSTAMP);
    
    /*
     * Calculate the UST associated with the next audio sample frame to be read
     * from the input port.
     * 
     * 1. Get the frame number of the frame. Call this "msc."
     * 2. Get a pair of (UST, frame number) at the jacks.
     * 3. Use this pair to calculate the UST for when the frame
     *    "msc" was at the jacks.
     */
    ALgetframenumber(port, &msc);
    ALgetframetime(port, &nmsc, &nust);
    ust = nust - (nmsc-msc)*srate;
    
    /*
     * make the MIDI events relative to when our first audio sample
     * hit the jacks.
     */
    mdSetStartPoint(ip, ust, 0);
    
    sproc(audio_loop, PR_SALL);
    printf("audio UST: %llu\n", ust);
    printf("recording... hit <enter> to stop\n");
    schedctl(NDPRI, 36);
    do {
	if (poll(pfds, 3, -1) < 0) {
	    perror("problem in poll");
	    return -1;
	}
	
	if (pfds[1].revents & POLLIN) {
	    /*
	     * event(s) are ready for reading...grab one.
	     */
	    if (mdReceive(ip, &event[nev], 1) < 0) {
		perror("mdReceive failure");
		return -1;
	    }	    
	    /*
	     * if loopback is enabled, send the event out immediately.
	     * remember that the output port is in NOSTAMP mode.
	     */
	    if (loopthru) {
		if (mdSend(op, &event[nev], 1) < 0) {
		    perror("mdSend failure");
		    return -1;
		}	
	    }
	    
	    ev[nev] = new MFfileEvent(&event[nev]);
	    track[1]->insertEvent(ev[nev]);
	    
	    mdPrintEvent(prbuf, &event[nev], 1);
	    printf("%s", prbuf);
	    nev++;
	} 

	if (pfds[0].revents & POLLIN) {
	    done = 1;
	}
	    
    } while (!done);

    printf("inserted %d\n", nev);

    track[0]->seekBeginning();
    track[1]->seekBeginning();
    file.save(mfname);

    wait(&status);
    
    return 0;
}
