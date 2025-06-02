/************************************************************************\
 * 	File:		scale.c						*
 *									*
 * 	A more featureful version of the scales program.		*
 *	This takes a bunch of command line arguments for		*
 *	playing scales in various ways.					*
 *									*
\************************************************************************/

#include "dmedia/midi.h"
#include <getopt.h>
#include <stdio.h>
#include <sys/syssgi.h>

char *usage = "scale [-o octaves] [-m mode] [-l length] [-s start] [-u] [-d] [interface]\n\
    -o sets the number of octaves sounded\n\
    -m sets the diatonic mode, eg, 1 == ionian (major)\n\
       6 == aeolian (minor)\n\
       8 is whole tone\n\
    -l sets the length in milliseconds\n\
    -s sets the start note\n\
    -d play the scale down rather than up.\n\
    -c sets the MIDI channel used\n\
    -v sets velocity\n\
    -i infinite repetitions\n\
    -p set program (patch) number of timbre to play\n\
    -f seconds in the future scale will start\n";

char *optstr = "o:m:dc:s:v:if:l:ip:";
int uintervals[] = { 2, 2, 1, 2, 2, 2, 1 };
int dintervals[] = { 1, 2, 2, 1, 2, 2, 2 };
long long stamp = 0;

/* Simple routine to transmit a note on and subsequent note off
 * event.
 */
playnote(MDport port, char note, unsigned long long length, char channel, 
	 char velocity)
{
	MDevent mdv;

	/* Transmit a NOTE ON event. */	
	mdv.stamp = stamp; 
	mdv.msg[0] = MD_NOTEON | (channel & 0xf);
	mdv.msg[1] = note;
	mdv.msg[2] = velocity;
	stamp += length;
	if (mdSend(port, &mdv, 1) < 0) {
		exit(-1);
	}

	/* This is slightly tricky.  We turn off the note by sending
 	 * a NOTE ON event with a velocity of 0 which is equivalent
	 * to sending a NOTE OFF event.
	 */
	mdv.stamp = stamp;
	mdv.msg[2] = 0;
	if (mdSend(port, &mdv, 1) < 0) {
		exit(-1);
	}

	/* The inter-note period is fixed at 10 milliseconds */
	stamp += 10000000;
}


/*
 * Main program -- parse the command line options, initialize MIDI, and
 * 	play the notes.
 */
main(int argc, char **argv)
{
	MDport 	outport;		/* Output MIDI port */
	MDevent ev;
	char    *interface;		/* The interface name string */ 
	int	num_interfaces;		/* # of configured interfaces */	
	int     octaves = 8;		/* Octave range */
	int     mode = 0;		/* Play mode */
	char    startnote = 0x48;	/* First note to play */
	char    topnote;		/* Last note played */
	char    upflag = 1;		/* Play notes up, or down */
	char    loop_forever = 0;	/* Continue playing until interrupted */
	char    channel = 0;		/* Channel to transmit on */
	char    velocity = 0x48;	/* Velocity for note */
	char	program = 0x0;		/* Selected program */
	int     inp;			/* Random input control variable */
	int     c;			/* Getopt option */
	long long future = 0;  	        /* Amount of time before play starts */
	long long length = 500000000;   /* Amount of time note is held */ 

	/* Initialize the MIDI library */
	num_interfaces = mdInit();
	if (num_interfaces < 0) {
		printf("MIDI not initialized!\n");
		exit(-1);
	}

	/* Parse the command line arguments */
	while ((c = getopt(argc, argv, optstr)) > 0) {
		switch (c) {
		case 'o':
			octaves = 8 * atoi(optarg);
			if (!octaves) {
				fprintf(stderr,"illegal value for -o\n");
				octaves = 8;
			}
			break;
		case 'm':
			mode = atoi(optarg) - 1;
			if (mode < 0 || mode > 8) {
				fprintf(stderr,"illegal mode\n");
				mode = 0;
			}
			break;
		case 'd':
			upflag = 0;
			break;
		case 'l':
			/* Time needs to be in nanoseconds, hence the 
			 * 1000000 conversion factor 
			 */
			length = atoi(optarg) * 1000000;
			if (!length) {
				fprintf(stderr, "illegal note duration\n");
				length = 500 * 1000000;
			}
			break;

		case 'c':
			channel = atoi(optarg) - 1;
			if (channel < 0) {
				fprintf(stderr, "illegal MIDI channel\n");
				channel = 0;
			}
			break;
		case 's':
			startnote = atoi(optarg);
			if (startnote < 1 || startnote > 127) {
				fprintf(stderr, "illegal starting note\n");
				startnote = 0x48;
			}
			break;
		case 'v':
			velocity = atoi(optarg);
			if (velocity <= 0) {
				fprintf(stderr, "illegal velocity\n");
				velocity = 0x48;
			}
			break;
		case 'i':
			loop_forever = 1;
			break;
		case 'f':
			inp = atoi(optarg);
			future = (long long) inp * 1000000000ll;
			break;
		case 'p':
			program = atoi(optarg);
			if (program < 0 || program > 127) {
				fprintf(stderr, "illegal program\n");
				program = 0x0;
			}
			break;
		case '?':
			printf("usage: %s", usage);
			exit(0);
			break;
		}
	}

	/* Find the interface name, if available */
	interface = NULL;
	if (optind < argc) {
		interface = argv[optind];
		printf("Interface: %s\n", interface);
	} else {
		printf("Using default output device\n");
	}

	/* Now open up the port, if possible */
	if ((outport = mdOpenOutPort(interface)) == NULL) {
		fprintf(stderr, "scale: could not open port '%s'\n",
			interface);
		exit(1);
	}

	/* In this program, we set things up so that the timestamps
	 * used are precisely equivalent to USTs.  To do this, we simply
	 * map a stamp time of 0 to the UST time of 0.  This can be
	 * really handy when you're trying to sync MIDI events to some
	 * other digital medium (like audio or video) which uses UST
	 * as a timing reference.
 	 */
	mdSetStampMode(outport, MD_RELATIVESTAMP);
	mdSetStartPoint(outport, 0, 0);

	/* Since we're now working in the UST time base, we need to
	 * find out what the current time is so that we know when to
	 * send the first MIDI event.
	 */
	dmGetUST(&stamp);
	stamp += future;

	printf("mode %d\n", mode);

	/* Send the program change event */
	ev.stamp = stamp;
	ev.msg[0] = MD_PROGRAMCHANGE;
	ev.msg[1] = program;
	mdSend(outport, &ev, 1);

	/* Play the scale */
	do {
		int i;

		topnote = startnote;
		if (upflag) {
		    for (i = 0; i < octaves; i++ ) {
			playnote(outport, topnote, length, channel, velocity);
			topnote = uintervals[(i + mode) % 7] + topnote;

		    }
	        } 
		else {
		    /* Play scales from high to low */
		    for (i = octaves; i > 0; i--) {
			topnote -= dintervals[(i + mode) % 7];
			playnote(outport, topnote, length, channel, velocity);
		    }
		}
	} while (loop_forever); 


	/* Finally, we need to wait until all of the events we've 
	 * queued have actually been transmitted before we close
	 * the port (closing the port removes any queued events which have
	 * yet to be transmitted).  Since we know the timestamp at which 
	 * the final event was supposed to have been transmitted, we can simply
	 * wait until that time is past.
	 */
	for(;;) {
		long long now;

		dmGetUST(&now);
		if (now > stamp) {
			mdClosePort(outport);
			exit(0);
		} else {
			sleep(1);
		}
	}
}
