/*
 * Example program for decoding Linear Time Code using
 * the Digital Media library LTC decoder (see dmLTC(3dm)).
 *
 * This program reads from an active audio port.  To
 * use, connect an LTC source to the line input, set
 * the default input to ``Line'' with apanel, set a
 * sampling rate greater than 22khz with apanel, and
 * run this program.  It will print out a decoded time
 * code.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <dmedia/dm_params.h>
#include <dmedia/audio.h>
#include <dmedia/dm_ltc.h>
#include <dmedia/dm_audio.h>

#define COUNT 200	/* Audio input buffer size */

/*
 * Open and set the modes on an audio port.
 * The LTC can be decoded from any format, stereo, or
 * mono.  However, it is not effective at sample rates below
 * 22khz.
 */
static void
SetupAudioPort(ALport *port, long *rate)
{
    ALconfig config;
    long alparams[2];

    config = ALnewconfig();
    ALsetwidth(config, AL_SAMPLE_8);
    ALsetchannels(config, AL_MONO);
    *port = ALopenport("inport", "r", config);
    ALfreeconfig(config);

    alparams[0] = AL_INPUT_RATE;
    ALgetparams(AL_DEFAULT_DEVICE, alparams, 2);

    if (alparams[1] < 22000) {
	    fprintf(stderr, "Input rate must be at least 22khz\n");
	    exit(1);
    }

    *rate = alparams[1];
}

main()
{
    ALport inport;			/* AL port for reading */
    DMparams * aparams;			/* Audio parameters    */
    unsigned long long	startframe;	/* Initial AL MSC */
    unsigned long long	starttime;	/* UST of startframe */
    unsigned long long	prevframe;	/* MSC of last timecode */
    unsigned long long	prevtime;	/* UST of prevframe */
    int	nanosperframe;			/* audio frame duration in nanoseconds*/
    DMLTCcode dmltc;
    DMLTCdecoder ltcdecoder;
    long inrate;

    /*
     * open an audio port and get the
     * sampling rate.
     */
    SetupAudioPort(&inport, &inrate);

    nanosperframe = 1000000000LL / inrate;

    /*
     * Create an LTC decoder and set its
     * parameters
     */
    dmLTCDecoderCreate(&ltcdecoder, DM_TC_FORMAT_NTSC | DM_TC_RATE_30);

    dmParamsCreate(&aparams);
    dmSetAudioDefaults(aparams, DM_AUDIO_WIDTH_8, inrate, 1);
    dmLTCDecoderSetParams(ltcdecoder, aparams, 0);
    dmParamsDestroy(aparams);

    /*
     * Get an initial frame number and time from
     * our AL port.  This is used to calculate
     * deltas.  A real application would want to
     * do this periodically to avoid long term
     * drift.
     */
    ALgetframetime(inport, &startframe, &starttime);

    for (; ; ) {
	unsigned long long	thisframe;
        char	samplebuffer[COUNT];
	void	*ltcBuffer;
        int	ltcCount;

	/*
	 * Get the frame number of the next sample
	 * in the queue.
	 */
	ALgetframenumber(inport, &thisframe);

	/*
	 * Read a block of samples.
	 */
	ALreadsamps(inport, samplebuffer, COUNT);

	/*
	 * Search for an LTC sequence.  This really means finding
	 * the end of a sequence and depending on the size of COUNT,
	 * there could be more than one.  If the buffer ends in the
	 * middle of a sequence, then the decoder remembers the partially
	 * decoded portion and returns false.
	 */
	for (ltcBuffer = samplebuffer, ltcCount = COUNT; ltcCount; ) {
	    if (dmLTCDecode(ltcdecoder, &ltcBuffer, &ltcCount, &dmltc) ==
							    DM_SUCCESS) {
		char	timestring[512];
		unsigned long long	ltctime;
		unsigned long long	ltcframe;

		/*
		 * Calculate the UST of the LTC sequence.  This
		 * could be used to synchronize with audio, video, or
		 * Cosmo Compress.
		 */
		ltctime = ltcframe * nanosperframe + starttime;

		/*
		 * Calculate the relative frame count
		 */
		ltcframe = (COUNT - ltcCount) + (thisframe - startframe);

		dmTCToString(timestring, &dmltc.tc);

		printf("Timecode: %s, Delta UST: %lld, Delta MSC: %lld\n",
		    timestring, ltctime-prevtime, ltcframe-prevframe);

		prevtime = ltctime;
		prevframe = ltcframe;
	    }
	}
    }
}
