/************************************************************************\
 * 	File:		thru.c						*
 * 	Author:		jfk						*
 *									*
 *	Copy data from one MIDI port out to another (while optionally	*
 *	displaying the data on a tty).  The user can specify which	*
 *	ports to read from or write to on the command line.		*
 * 									*
 *   To run this so that it reads data from an interface named		*
 *	"Serial Port 2" and writes it to the Software Synthesizer,	*
 *	do the following:						*
 *									*
 *		./thru -e "Serial Port 2" "Software Synth"		*
 *									*
\************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stropts.h>
#include <poll.h>
#include "dmedia/midi.h"
#include "sys/select.h"


main(int argc, char **argv)
{
	int c;				/* Character from getopt */
	int nfds;			/* Number of ready file descriptors */
	MDport inport, outport;		/* Input and output midi ports */
	MDevent mdv;			/* MIDI event being forwarded */
	struct pollfd pfd;		/* Poll structure for MIDI in */
	char *inname, *outname;		/* Names of input and output devices */
	int echoflag = 0;		/* Boolean flag for echoing input */	
	
	/* Scan off any switches */
	while ((c = getopt(argc, argv, "e")) != EOF) {
		switch (c) {
		case 'e':
			echoflag = 1;
			break;
		case '?':
			usage();
			break;
		}
	}

	inname = NULL;
	outname = NULL;

	/* Now get the names of the input and output ports */
	if (optind < argc)
		inname = argv[optind++];

	if (optind < argc)
		outname = argv[optind++];

	/* Catch too many arguments */
	if (optind != argc)
		usage();


	/* Initialize the MIDI subsystem.  mdInit returns the current
	 * number of active interfaces, but we don't care for this program.
	 */
	(void) mdInit();

	inport = mdOpenInPort(inname);
	if (inport == NULL) {
		printf("open of input port failed\n");
		exit(1);
	}	

	outport = mdOpenOutPort(outname);
	if (outport == NULL) {
		printf("open of output port failed\n");
		exit(1);
	}

	/* We're just forwarding the data as fast as we can, so we
	 * don't need to timestamp.
	 */
	mdSetStampMode(inport, MD_NOSTAMP);
	mdSetStampMode(outport, MD_NOSTAMP);


	/* We want to illustrate how to poll for input on the
	 * file descriptor.  In this program, we could just equally
	 * well block on mdReceive(), since there are no other
	 * sources of input to the loop.  
	 */  
	pfd.fd = mdGetFd(inport);
	pfd.events = POLLIN;
	while(1) {
		nfds = poll(&pfd, 1, -1);
		if (nfds == 1) {
			if (mdReceive(inport, &mdv, 1) < 0) {
				printf("failure receiving message\n");
				abort();
				exit(-1);
			}

			if (mdSend(outport, &mdv, 1) < 0) {
				printf ("failure sending message\n");
				exit(-1);
			}
	
			if (echoflag) {
				char buf[200];
				mdPrintEvent(buf, &mdv, 1);
				printf("%s", buf);
			}

			if (mdv.msg[0] == MD_SYSEX) 
				mdFree(mdv.sysexmsg);
		} else {
			perror("Poll failed");
		}
	}
}


usage()
{
	fprintf(stderr, "Usage: thru [-e] [inport [outport]]\n");
	fprintf(stderr, "  where -e enables echoing of received events\n");
	fprintf(stderr, "  inport is an optional port from which to read\n");
	fprintf(stderr, "  outport is an optional port on which to write\n");
	fprintf(stderr, " If no optional ports are specified, the default\n");
	fprintf(stderr, " input and output device are used.\n");
	exit(1);
}
