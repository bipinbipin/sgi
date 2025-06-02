/************************************************************************\
 *	File:		panic.c						*
 *	Author:		jfk						*
 *									*
 *	Occasionally, your programs may accidentally leave one		*
 *	or more notes sounding.  The panic program sends the		*
 *	ALL NOTES OFF message, followed	by an ALL SOUND OFF message, 	*
 *	followed by a RESET ALL CONTROLLERS message on all channels.	*
 *									*
 *	For most modern synthesizers, these messages should be		*
 *	sufficient for shutting off all playing notes.  However,	*
 *	some older synthesizers don't properly respond to these		*
 *	messages.  For these occasions, we provide a special		*
 *	switch which will send explicit note off events on all		*
 * 	channels.  This takes a couple of seconds, so we don't do it 	*
 *	unless the '-f' switch is present on the command line.		*
 *									*
\************************************************************************/

#include <stdio.h>
#include <dmedia/midi.h>

long long stamp = 0;

char *usage = "Usage: panic [-f] [-a | interface name]\n\
    -f 		  	   Force.  Send explicit NOTE OFF events on \n\
			    all channels.\n\
			   \n\
    [-a | interface name]  If the '-a' switch is provided, messages are\n\
			    sent to all configured interfaces.  If a single\n\
			    interface name is given, messages are directed\n\
			    only to that interface.  If no interfaces are\n\
			    specified, messages are sent to the default\n\
			    interface.\n";

/*
 * Transmit ALL NOTES OFF, ALL SOUND OFF, and RESET ALL CONTROLLERS
 * messages on all 16 channels.
 */
void
send_control_messages(MDport port)
{
	int channel, byte; 
	MDevent ev;
	char data_byte[] = { MD_RESETALLCONTROLLERS, MD_ALLNOTESOFF,
			     MD_ALLSOUNDOFF };

	for (channel = 0; channel < 16; channel++) {
		for (byte = 0; byte < 3; byte++) {
			ev.stamp = stamp++;
			ev.msg[0] = MD_CONTROLCHANGE | channel;
			ev.msg[1] = data_byte[byte];
			ev.msg[2] = 0;
			mdSend(port, &ev, 1);
		}
	}
} 


/*
 * Transmit explicit NOTE OFF events on all channels.
 */
void
send_note_off_events(MDport port)
{
	int channel, note;
	MDevent ev;
	
	printf("Sending NOTE OFF on channel XX");
	for (channel = 0; channel < 16; channel++) {

		printf("\b\b%2d", channel);
		fflush(stdout);

		for (note = 0; note < 128; note++) {
			ev.stamp = stamp++;
			ev.msg[0] = MD_NOTEOFF;
			ev.msg[1] = note;
			ev.msg[2] = 0;
			mdSend(port, &ev, 1);
		}
	}

	printf("\n\n");
}
	

/*
 * Main program -- parse arguments and iterate over interfaces.
 */
void
main(int argc, char **argv)
{
	MDport	outport;		/* Port connected to requested port */
	int	num_interfaces; 	/* Number of configured interfaces */
	int	all_ifcs_flag = 0;	/* Flag for sending to all interfaces */
	int	force_flag = 0;		/* Flag for doing brute-force */
	int	c;			/* Flag character from getopt */
	int	i;			/* Iteration variable */

	/* Parse the command line options */
	while ((c = getopt(argc, argv, "fa")) != -1) {
		switch (c) {
		case 'a':
			all_ifcs_flag = 1;
			break;
		case 'f':
			force_flag = 1;
			break;
		case '?':
			fprintf(stderr, usage);
			exit(0);
		}
	}

	/* First initialize the MIDI library */
	num_interfaces = mdInit();
	if (num_interfaces == 0) {
		fprintf(stderr, "panic: no interfaces configured\n");
		exit(1);
	}

	for (i = 0; i < num_interfaces; i++) {
		char *name;

		if (all_ifcs_flag) {
			name = mdGetName(i);
		} else {
			if (optind < argc) 
				name = argv[optind];
			else
				name = NULL;
		}

		/* Open the port */
		outport = mdOpenOutPort(name);
		if (outport == NULL) {
			if (!all_ifcs_flag) {
				fprintf(stderr, "panic: Could not open interface\n");
				exit(1);
			} else {
				continue;
			}
		}

		if (name != NULL) 
			printf("Sending messages through interface %s...\n",
				name);
		else
			printf("Sending messages to default interface...\n");

		/* Transmit 1 event every millisecond */
		mdSetStampMode(outport, MD_RELATIVETICKS);
		mdSetDivision(outport, 1);
		mdSetTempo(outport, 1000);

		if (force_flag)
			send_note_off_events(outport);
		else
			send_control_messages(outport);

		/* Wait for messages to go out */
		while (mdTellNow(outport) < stamp) ;

		/* Exit the loop if we're done */
		if (!all_ifcs_flag)
			break;
	}
	
	exit(0);
}
