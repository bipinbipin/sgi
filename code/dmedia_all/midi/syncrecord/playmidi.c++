#include <dmedia/midi.h>
#include "midifile.h"

#include <audio.h>
#include <audiofile.h>

#include <stdio.h>
#include <malloc.h>
#include <getopt.h>
#include <libc.h>
#include <errno.h>
#include <stropts.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>

#define NEVENTS 100

char *optstring = "t:iqv";
int tempo=0, quiet, verbose;

#define CHUNKSIZE 20000
#define NCHANS 2

#if DEBUG
#define dbprintf    printf
#else
#define dbprintf    if(0)printf
#endif

ALport port;
struct pollfd apfds[1];
AFfilehandle afile;
short abuf[CHUNKSIZE*NCHANS];
    
void
play_audio(void *)
{
    int afdone = 0;
    int rsize;
    while (!afdone) {
	ALsetfillpoint(port, CHUNKSIZE*NCHANS);
	if (poll(apfds, 1, -1) < 0) {
	    perror("problem in poll");
	    return;
	}
	
	if (!afdone && apfds[0].revents & POLLOUT) {
	    if ((rsize = AFreadframes(afile, AF_DEFAULT_TRACK, abuf, CHUNKSIZE))		 == CHUNKSIZE) {
		ALwritesamps(port, abuf, CHUNKSIZE*NCHANS);
	    }
	    else {
		ALwritesamps(port, abuf, rsize*NCHANS);
		afdone=1;
	    }
	}
    }
    while(ALgetfilled(port) > 0) sginap(10);
    ALcloseport(port);
}

main(int argc, char **argv)
{
    MFfile file;
    char mfname[40], afname[40];
    MFfileEvent *eventBuf;
    MDport mdo;
    int mfdone = 0;
    int i;
    char *myname;
    long pbuf[2];
    double srate;
    long long ust, msc, nust, nmsc;
    long long lastStamp;
    struct pollfd pfds[1];
    char doinit = 0;
    int status;

    myname = argv[0];
    while ((i = getopt(argc,argv,optstring)) != -1)
	switch(i) {
	  case 't' :
	    tempo = atoi(optarg);
	    break;
          case 'i':
	    doinit = 1;
	    break;
          case 'q':
	    quiet = 1;
	    break;
          case 'v':
	    verbose = 1 ;
	    break;
	}

    i = mdInit();
    if (i < 0) {
	printf("MIDI not initialized!\n");
	exit(-1);
    }

    /*
     * open the default output MIDI port (that's what you get when you
     * pass `0' as the interface name).
     */
    mdo = mdOpenOutPort(0);
    if (mdo == NULL) 
	printf("open failed\n");

    /* set stamp mode. relative ticks is very useful for standard MIDI files. */
    
    mdSetStampMode(mdo, MD_RELATIVETICKS);
    
    pfds[0].fd = mdGetFd(mdo);
    pfds[0].events = POLLOUT;

    if (optind == argc) {
	printf("please supply a midi file\n");
	exit(0);
    }

    sprintf(mfname, "%s.mid", argv[optind]);
    sprintf(afname, "%s.aifc", argv[optind]);
    if ((afile = AFopenfile(afname, "r", 0)) == 0) {
	fprintf(stderr, "could not open file %s\n", afname);
	exit(-1);
    }
    srate = AFgetrate(afile, AF_DEFAULT_TRACK);
    pbuf[0] = AL_OUTPUT_RATE;
    pbuf[1] = srate;
    ALsetparams(AL_DEFAULT_DEVICE, pbuf, 2);
    srate = 1000000000.0 / srate;	    /* now in nanosecs per sample */

    /*
     * load the file and grab the first tempo event in the file. look on track
     * 0 only for the tempo. if there is no tempo value found in the file, then
     * use a default value of 500000 (same value used in recordmidi.c++ example).
     */
    file.load(mfname);
    if (!tempo) {
	int t = 500000;	     // default value if file contains no tempo info
	MFtrack *trk = file.getTrack(0);
	MFfileEvent *fev = trk->seekMetaEvent(MIDImeta_SetTempo);
	if (fev != NULL) {
	    MFmeta *mev = fev->metaEvent();
	    t = (mev->msg[0]<<16) + (mev->msg[1]<<8) + (mev->msg[2]);
	}
	printf("tempo = %d\n", t);
	mdSetTempo(mdo, t);
    } else {
	mdSetTempo(mdo, tempo);
    }
    file.rewind();
    
    /*
     * open an output audio port with default configuration: 16-bit stereo
     * samples.
     */
    port = ALopenport(myname, "w", 0);
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
    apfds[0].events = POLLOUT;
    
    /*
     * bring the audio port out of underflow. Otherwise,  we cannot
     * calculate the MSC & UST relationships.
     */
    bzero(abuf, CHUNKSIZE*NCHANS*sizeof(short));
    ALwritesamps(port, abuf,CHUNKSIZE*NCHANS);
    
    ALgetframenumber(port, (unsigned long long*)&msc);
    ALgetframetime(port, (unsigned long long*)&nmsc, (unsigned long long*)&nust);
    ust = nust - (nmsc-msc)*srate;
    
    /*
     * establish relationship between UST and ticks using mdSetStartPoint()
     * and set the division on the port.
     */
    mdSetStartPoint(mdo, ust, 0);
    mdSetDivision(mdo, file.division()); 

    printf("playing file\n");
    sproc(play_audio, PR_SALL);
    while (!mfdone) {
	if (poll(pfds, 1, -1) < 0) {
	    perror("problem in poll");
	    return -1;
	}
	
	if (!mfdone && pfds[0].revents & POLLOUT) {

	    eventBuf = file.nextEvent();

	    if (eventBuf != NULL) {

		MDevent *ev, event;
		char sysex[6];

		if (eventBuf->eventType() == MF_MIDI_EVENT) {

		    ev = eventBuf->midiEvent();
		    lastStamp = (long long)ev->stamp;
		    mdSend(mdo, ev, 1);
		    
		} else if (eventBuf->eventType() == MF_META_EVENT) {
		    /*
		     * midi library accepts MD_META tempo events for the
		     * purpose of synchronously changing the tempo of
		     * MIDI playback. if we hit a MF_META_EVENT of type
		     * MIDImeta_SetTempo, then we want to package it
		     * up as a MIDI event for the library.
		     */
		    MFmeta *mev = eventBuf->metaEvent();
		
		    if (mev->type == MIDImeta_SetTempo) {
			sysex[0] = MD_META;
			sysex[1] = 0x51;
			sysex[2] = 0x03;
			sysex[3] = mev->msg[0];
			sysex[4] = mev->msg[1];
			sysex[5] = mev->msg[2];
			
			event.msg[0] = MD_META;
			event.sysexmsg = sysex;
			event.stamp = mev->stamp;
			event.msglen = 6;
			
			lastStamp = (long long)mev->stamp;
			
			mdSend(mdo, &event, 1);
		    }
		}
	    }
	    else {
		mfdone = 1;
	    }
	}
    }
    /* wait for the last MIDI event to go out */
    while (mdTellNow(mdo) < lastStamp) {
	sginap(20);
    }
    wait(&status);
    printf("done\n");
    return 0;
}
