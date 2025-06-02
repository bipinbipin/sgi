/************************************************************************\
 * 	File:		simple_scale.c					*
 *	Author:		jfk						*
 *									*
 *	This is a really simple program which simply plays eight	*
 *	scales on the default MIDI device using two different		*
 *	programs (programs control what timbre a synthesizer		*
 *	uses when playing a note).  Don't be put off by the		*
 *	length of this file; most of the lines in it are comments.	*
 *  	What is actually going on here is fairly straightforward.	*	
\************************************************************************/


#include <stdio.h>
#include <dmedia/midi.h>
#include <dmedia/dmedia.h>

main(int argc, char **argv)
{
    char        *intfcname;	/* Name of the interface to send to */
    MDport      port;		/* Port connected to named interface */
    int         num_intfcs;	/* Number of configured interfaces */
    MDevent     event;		/* A MIDI event structure */
    int         channel = 0;    /* MIDI channel to play on */
    int		i, j;		/* The ever-popular iteration variables */
    unsigned long long now;	/* UST of current time */
    long long	stamp = 0;	/* The timestamp for the next event */
   
    /* First, we initialize the MIDI library. */ 
    num_intfcs = mdInit();
    if (num_intfcs == 0) {
        fprintf(stderr, "No MIDI interfaces configured.\n");
        exit(1);
    }

    /* Select the name of the interface to open.  If no interface is
     * specified on the command line, we open the default device 
     * (which is indicated by passing NULL to mdOpenOutPort).
     */
    intfcname = NULL;
    if (argc == 2) {
	intfcname = argv[1];
    } 
    else if (argc > 2) {
	fprintf(stderr, "Usage: simple_scale [interface]\n");
	exit(1);
    }

    if ((port = mdOpenOutPort(intfcname)) == NULL) {
        fprintf(stderr, "Cannot open MIDI interface '%s'for output.\n", 
		intfcname);
        exit(1);
    }

    /* Reckon time in ticks relative to the current time. */
    mdSetStampMode(port, MD_RELATIVETICKS);

    /* We now establish the correspondence between real-time (measured
     * in UST) and ticks.  We do this by figuring out what time it is
     * now and telling the system that the tick origin is now.
     */
    dmGetUST(&now);
    mdSetStartPoint(port, (long long) now, 0);

    /* Make the duration of one tick be 30 milliseconds (which is equivalent
     * to 30,000 microseconds, which is actually the unit taken by mdSetTempo)
     */
    mdSetDivision(port, 1);
    mdSetTempo(port, 30000);

    /* Play scales using eight different timbres. */
    for (i = 0; i < 8; i++) {

        /* Transmit a program change event; since many synths (esp.
	 * general midi synths) group similar sounding patches together, 
	 * we multiply i by 10 to try and get more different sounding
	 * programs.
 	 */
        event.stamp = stamp;	stamp++;
        event.msg[0] = MD_PROGRAMCHANGE|channel;
        event.msg[1] = i*10;
        mdSend(port, &event, 1);

	/* Now just play the scale */
        for (j = 0; j < 12; j++) {
                /* Send a note on */
                event.stamp = stamp; stamp++;
                event.msg[0] = MD_NOTEON|channel;
                event.msg[1] = 48 + j;
                event.msg[2] = 100;
                mdSend(port, &event, 1);

                /* Now send a note off */
                event.stamp = stamp; stamp += 2;
                event.msg[0] = MD_NOTEOFF|channel;
                event.msg[1] = 48 + j;
                event.msg[2] = 0;
                mdSend(port, &event, 1);
        }
    }

    /* Unfortunately, closing a port will cause any pending MIDI
     * data to be discarded.  Therefore, we need to wait until
     * all of the events actually go out.  To do this, we call
     * the function mdTellNow() which tells us what tick is currently
     * being transmitted.  In order to remain a good Unix citizen,
     * we sleep in between tests so that we don't unnecessarily 
     * consume the CPU.
     */
    while (mdTellNow(port) < stamp) {
	sleep(1);
    }

    mdClosePort(port);
}
