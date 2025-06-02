/**********************************************************************
readltc.c
   A linear time code (ltc) decoder.

   NOTE: This program demonstrates how to parse the actual timecode
	 signal.  See the example program ``dmltc.c'' for a simpler
	 case that uses the LTC support in the Digital Media library.

If invoked as:  readltc filename.aiff
   It will read the file into memory, then decode the memory image as
   quickly as it can (faster than real time)

Alternately if invoked w/o arguments:  readltc
   It will open a mono audio library input and decode ltc from that
   stream in real time.  A suggested test would be to use apanel to
   configure digital loopthrough, and to use playaifc to play the
   supplied aiff file:

   Make sure that nothing is plugged into the digital audio port.

      apanel -source digital -outrate 22050
      readltc
      playaifc smpte.aifc

   Timecode should now be scrolling.  Experiment by changing the
   output rate up and down.  This corresponds to non-realtime
   cueing modes on a deck.  
**********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <audio.h>
#include <dmedia/audiofile.h>

#define FORWARD_SYNC_CODE 0xbffc
#define FORWARD_SYNC_MASK 0xffff
#define REVERSE_SYNC_CODE 0x3ffd
#define REVERSE_SYNC_MASK 0xffff

/* Globals */
unsigned long	bitpat[3] = {
    0, 0, 0};


void
printforwardcode(void)
{
    int	ht, hu, mt, mu, st, su, ft, fu, ug1, ug2, ug3, ug4, ug5, ug6, ug7, ug8,
    df, cf, ua1, ua2, ua3, ua4, syn;

    fu =  (bitpat[0] >> 0)  & 0xf;
    ug1 = (bitpat[0] >> 4)  & 0xf;
    ft =  (bitpat[0] >> 8)  & 0x3;
    df =  (bitpat[0] >> 10) & 0x1;
    cf =  (bitpat[0] >> 11) & 0x1;
    ug2 = (bitpat[0] >> 12) & 0xf;
    su =  (bitpat[0] >> 16) & 0xf;
    ug3 = (bitpat[0] >> 20) & 0xf;
    st =  (bitpat[0] >> 24) & 0x7;
    ua1 = (bitpat[0] >> 27) & 0x1;
    ug4 = (bitpat[0] >> 28) & 0xf;

    mu =  (bitpat[1] >> 0)  & 0xf;
    ug5 = (bitpat[1] >> 4)  & 0xf;
    mt =  (bitpat[1] >> 8)  & 0x7;
    ua2 = (bitpat[1] >> 11) & 0x1;
    ug6 = (bitpat[1] >> 12) & 0xf;
    hu =  (bitpat[1] >> 16) & 0xf;
    ug7 = (bitpat[1] >> 20) & 0xf;
    ht =  (bitpat[1] >> 24) & 0x3;
    ua3 = (bitpat[1] >> 26) & 0x1;
    ua4 = (bitpat[1] >> 27) & 0x1;
    ug8 = (bitpat[1] >> 28) & 0xf;

    syn = (bitpat[2] >> 0)  & 0xffff;

    printf("fwd %d%d:%d%d:%d%d:%d%d ", ht, hu, mt, mu, st, su, ft, fu);
    printf("ug:%x%x%x%x%x%x%x%x ", ug1, ug2, ug3, ug4, ug5, ug6, ug7, ug8);
    printf("df:%x cf:%x ua:%x%x%x%x syn:%x\n", df, cf, ua1, ua2, ua3, ua4, syn);
}


void
printreversecode(void)
{
    unsigned long	mypat[3];
    unsigned long	lmask, rmask, *lword, *rword, lbit, rbit;
    int	i;
    int	ht, hu, mt, mu, st, su, ft, fu;
    int	ug1, ug2, ug3, ug4, ug5, ug6, ug7, ug8;
    int	df, cf;
    int	ua1, ua2, ua3, ua4;
    int	syn;

    /* Make a copy of the shift register */
    mypat[0] = bitpat[0];
    mypat[1] = bitpat[1];
    mypat[2] = bitpat[2];

    /* Bit-reverse the copy of the shift register */

    lmask = 0x8000;
    rmask = 1;
    lword = &mypat[2];
    rword = &mypat[0];
    for (i = 0; i < 40; i++) {
	lbit = (*lword & lmask) != 0;
	rbit = (*rword & rmask) != 0;
	*lword &= ~lmask;
	*rword &= ~rmask;
	if (lbit)
	    *rword |= rmask;
	if (rbit)
	    *lword |= lmask;
	lmask >>= 1;
	if (lmask == 0) {
	    lmask = 0x80000000;
	    lword = &mypat[1];
	}
	rmask <<= 1;
	if (rmask == 0) {
	    rmask = 1;
	    rword = &mypat[1];
	}
    }

    fu =  (mypat[0] >> 0)  & 0xf;
    ug1 = (mypat[0] >> 4)  & 0xf;
    ft =  (mypat[0] >> 8)  & 0x3;
    df =  (mypat[0] >> 10) & 0x1;
    cf =  (mypat[0] >> 11) & 0x1;
    ug2 = (mypat[0] >> 12) & 0xf;
    su =  (mypat[0] >> 16) & 0xf;
    ug3 = (mypat[0] >> 20) & 0xf;
    st =  (mypat[0] >> 24) & 0x7;
    ua1 = (mypat[0] >> 27) & 0x1;
    ug4 = (mypat[0] >> 28) & 0xf;

    mu =  (mypat[1] >> 0)  & 0xf;
    ug5 = (mypat[1] >> 4)  & 0xf;
    mt =  (mypat[1] >> 8)  & 0x7;
    ua2 = (mypat[1] >> 11) & 0x1;
    ug6 = (mypat[1] >> 12) & 0xf;
    hu =  (mypat[1] >> 16) & 0xf;
    ug7 = (mypat[1] >> 20) & 0xf;
    ht =  (mypat[1] >> 24) & 0x3;
    ua3 = (mypat[1] >> 26) & 0x1;
    ua4 = (mypat[1] >> 27) & 0x1;
    ug8 = (mypat[1] >> 28) & 0xf;

    syn = (mypat[2] >> 0) & 0xffff;

    printf("rev %d%d:%d%d:%d%d:%d%d ", ht, hu, mt, mu, st, su, ft, fu);
    printf("ug:%x%x%x%x%x%x%x%x ", ug1, ug2, ug3, ug4, ug5, ug6, ug7, ug8);
    printf("df:%x cf:%x ua:%x%x%x%x syn:%x\n", df, cf, ua1, ua2, ua3, ua4, syn);
}


void
printbits(unsigned long pat[3])
{
    unsigned int	msk, i;

    msk = 0x8000;
    for (i = 0; i < 16; i++) {
	printf("%d", (pat[2] & msk) != 0);
	msk /= 2;
    }
    printf(" ");

    msk = 0x80000000;
    for (i = 0; i < 32; i++) {
	printf("%d", (pat[1] & msk) != 0);
	msk /= 2;
    };
    printf(" ");

    msk = 0x80000000;
    for (i = 0; i < 32 ; i++) {
	printf("%d", (pat[0] & msk) != 0);
	msk /= 2;
    }
    printf("\n");
}


void
process_data(short *sampBuffer, int buffLength)
{
    static int	state, bit, spantype, c21, c10;
    static int	cellsize = 1000, gotshort = 0, oldstate = 1, spanwidth = 0;
    int	sampleIndex;

    for (sampleIndex = 0; sampleIndex < buffLength; sampleIndex++) {
	/* Quantitize (with hysteresis) */
	if (state == 1)
	    state = (sampBuffer[sampleIndex] + 1000) > 0;
	else
	    state = (sampBuffer[sampleIndex] - 1000) > 0;

	if (state == oldstate) /* Count the span width */ {
	    spanwidth++;
	    continue;
	}

	/* Categorize spantype as either a long span or a short span */
	if (4 * spanwidth > 3 * cellsize) {
	    cellsize = spanwidth;   /* Input was a long span */
	    spantype = 1;
	} else      {
	    cellsize = 2 * spanwidth; /* Input was a short span */
	    spantype = 0;
	}

	/* Reset the span length counter. */
	spanwidth = 1;
	oldstate = state;

	/* Decode a span into a bit */
	if (spantype == 1) {
	    gotshort = 0;
	    bit = 0;
	} else      {
	    if (gotshort) {
		bit = 1;
		gotshort = 0;
	    } else	 {
		gotshort = 1;
		continue;
	    }
	}

	/* Put the bit into the shift register */
	c21 = bitpat[2] & 1; /* shift carry from word 2 to word 1 */
	c10 = bitpat[1] & 1; /* shift carry from word 1 to word 0 */
	bitpat[2] >>= 1; 
	bitpat[2] |= (bit << 15);
	bitpat[1] >>= 1; 
	bitpat[1] |= (c21 << 31);
	bitpat[0] >>= 1; 
	bitpat[0] |= (c10 << 31);

	/* Test the shift register for the span sync codes */
	if ( (bitpat[2] & FORWARD_SYNC_MASK) == FORWARD_SYNC_CODE)
	    printforwardcode();
	else if ((bitpat[0] & REVERSE_SYNC_MASK) == REVERSE_SYNC_CODE)
	    printreversecode();
    }
}


void
initAF(char *audioFileName, AFfilehandle *audiofile, long *totalFrames)
{
    int	vers, filefmt, channels, sampwidth, samptype, n;
    double	audiorate;
    int	nread;

    *audiofile = AFopenfile(audioFileName, "r", AF_NULL_FILESETUP);
    if (*audiofile == AF_NULL_FILEHANDLE) {
	printf("initAF: couldn't open %s\n", audioFileName);
	exit(1);
    }

    filefmt =      AFgetfilefmt(*audiofile, &vers);
    *totalFrames = AFgetframecnt(*audiofile, AF_DEFAULT_TRACK);
    audiorate =    AFgetrate(*audiofile, AF_DEFAULT_TRACK);
    channels =     AFgetchannels(*audiofile, AF_DEFAULT_TRACK);
    AFgetsampfmt(*audiofile, AF_DEFAULT_TRACK, &samptype, &sampwidth);
    printf("name %s\n", audioFileName);
    printf("format %d vers %d\n", filefmt, vers);
    printf("frames %d rate %f channels %d samptype %d sampwidth %d\n",
        *totalFrames, audiorate, channels, samptype, sampwidth);
}


/**********************************************************************
Setup hardware for: 8K i/o rate, mic input, 16bit mono samples.
And open an input and output port.
**********************************************************************/
void
initAL(ALport *alPort)
{
    long	pvbuf[8];
    ALconfig aconfig;

    /* set the i/o rates, select digital input */
#ifdef SET_AUDIO
    pvbuf[2] = AL_OUTPUT_RATE;
    pvbuf[3] = AL_RATE_22050;

    pvbuf[4] = AL_INPUT_SOURCE;
    pvbuf[5] = AL_INPUT_DIGITAL;
    ALsetparams(AL_DEFAULT_DEVICE, pvbuf, 6);
#endif /* SET_AUDIO */

    aconfig = ALnewconfig();
    ALsetwidth(aconfig, AL_SAMPLE_16);
    ALsetchannels(aconfig, AL_MONO);

    *alPort =  ALopenport("alPort", "r", aconfig);
}


#define BUFF_SIZE 1000
main(int argc, char **argv)
{
    ALport alPort;
    AFfilehandle audiofile;
    short	sampBuffer[BUFF_SIZE];
    int	done = 0;
    long	totalFrames; /* total frames in file (file mode) */
    long	totalRead = 0;
    int	nread;


    /* initialize */
    if (argc > 1) /* use a file? */
	initAF(argv[1], &audiofile, &totalFrames);
    else
     {
	initAL(&alPort);
	nread = BUFF_SIZE; /* ALreadsamps blocks until requested samps are read */
    }

    while (!done) {
	/* get samples to be processed */
	if (argc > 1) /* use a file? */ {
	    nread = AFreadframes(audiofile, AF_DEFAULT_TRACK, sampBuffer, BUFF_SIZE);
	    if (totalRead >= totalFrames)
		done = 1;
	} else
	    ALreadsamps(alPort, sampBuffer, BUFF_SIZE);

	totalRead += nread;
	process_data(sampBuffer, nread);
    }
}


