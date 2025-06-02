/**********************************************************************
*
* File: streamDecompress.c
*
**********************************************************************/

/*
 * Continous decompression / display module.
 *
 * Audio playback is implemented only for the
 * realtime display.
 * Here we synchronize playback by priming the audio and
 * video output pipes with silent/black preroll frames.
 * See the comments in the code below.
 *
 * The program would drop video frames as necessary to stay
 * in sync with audio.
 *
 */

#include <math.h>		/* floor() */

#include "dmplay.h"

#include <X11/Xlib.h>           /* X event handling */


/********
*
* Prototypes for functions local to this file.
*
********/

typedef void (* PFNPLAYIMG)( int, int );

static void preRollAndPlay(DMbuffer, DMbuffer);
static void newPreRoll(DMbuffer, DMbuffer);
static void playMovie( PFNPLAYIMG PlayFunc );
static void playAudioForFrame(int);
static void playImageForFrame(int, int);
static void oldplayImageForFrame(int, int);
static int  checkSync(void);
static void  makePrerollFrame(DMbuffer *, DMbuffer *);
static void  oldmakePrerollFrame(DMbuffer *, DMbuffer *);
static void startDecompressor(void);
static void sendDMbufferTodmIC(DMbuffer );

static void handleXEvents(void);

extern Display *dpy;

#define LOCK_DMID 	spin_lock( &(dmID.lock) )
#define UNLOCK_DMID 	release_lock( &(dmID.lock) )

/********
*
* Global variables (local to this file).
*
********/

static void* audioBuffer;	/* Buffer used for audio data.  Used */
static int audioBufferSize;	/* during pre-roll and during */
				/* playback. */

static int prerollVideoFrames;	/* Number of video fields produced */
				/* during preroll. */
static int prerollAudioFrames;	/* Number of audio frames produced */
				/* during preroll. */
static int audioFramesPlayed;	/* Total number of audio frames */
				/* played, including preroll. */

static int videoFramesSkipped;	/* Number of video frames that have */
				/* been skipped to keep the audio and */
				/* video in sync.  Negative of frames */
				/* had to be replicated. */

static unsigned long long videoStartTime;
static unsigned long long audioStartTime;


static pid_t childPid;
static char *fldbdry = "\xFF\xD9\xFF\xD8";
static pid_t parentPid = -1;
static int frameNum;
static DMbuffer prerollBuf1, prerollBuf2;

/* >>> */
#define MREC 1000
static unsigned long long *b4_ssel, *af_ssel, *b4_rsel, *af_rsel;
static int c_b4_ssel =1, c_af_ssel = 1, c_b4_rsel = 1, c_af_rsel = 1;
#define TIMINGARRAYS 4


/*
 * getTimingArrays
 */
static void getTimingArrays(void)
{
    b4_ssel = calloc(MREC*TIMINGARRAYS, sizeof(unsigned long long));
    if (b4_ssel == NULL) {
	ERROR("out of memory");
    }
    af_ssel = b4_ssel + MREC;
    b4_rsel = af_ssel + MREC;
    af_rsel = b4_rsel + MREC;
} /* getTimingArrays */

/*
 * printTiming
 */
static void printTiming(void)
{
    int i, t;
    double a, c, d, e, f;
    double sa=0.0, sc=0.0, sd=0.0, se=0.0, sf=0.0;

    if (b4_ssel == NULL) return;

    t = c_b4_ssel;
    if (t > c_af_ssel) t = c_af_ssel;
    if (t > c_b4_rsel) t = c_b4_rsel;
    if (t > c_af_rsel) t = c_af_rsel;

    printf("all timings in milliseconds:\n");
    printf("index   s-cycle  wait/out   r-cycle wait/recv vice-time\n");
    for (i = 1; i < t; i++) {
	a = (double)(af_ssel[i] - af_ssel[i-1])/NANOS_PER_MILLI;
	sa += a;
	c = (double)(af_ssel[i] - b4_ssel[i])/NANOS_PER_MILLI;
	sc += c;
	d = (double)(b4_rsel[i] - b4_rsel[i-1])/NANOS_PER_MILLI;
	sd += d;
	e = (double)(af_rsel[i] - b4_rsel[i])/NANOS_PER_MILLI;
	se += e;
	f = (double)(af_rsel[i] - af_ssel[i])/NANOS_PER_MILLI;
	sf += f;
	printf("%5d %9.2f %9.2f %9.2f %9.2f %9.2f\n",
	     i, a, c, d, e, f);
    }
    printf("averg   s-cycle   wait/in  wait/out   r-cycle wait/recv vice-time\n");
    printf("      %9.2f %9.2f %9.2f %9.2f %9.2f\n",
	sa/t, sc/t, sd/t, se/t, sf/t);
} /* printTiming */







/********
*
* streamDecompress - Run the streamed movie player
*
********/

void streamDecompress
    (
    void
    )
{
    static int first = 1;

    /*
     * Initialize global variables.
     */
    childPid = -1;

    audioBuffer = calloc( audio.queueSize, audio.frameSize );
    if ( audioBuffer == NULL) {
	ERROR( "out of memory" );
    }
    audioBufferSize = audio.queueSize * audio.frameSize;

    if(playstate.timing)
	getTimingArrays();

    /*
     * Set the priority of this thread.
     */
    if (options.hiPriority)
	setscheduling();

    dmICDecInit();

    /* Make preroll frames */
    if (NEWFORMAT) {
	makePrerollFrame( &prerollBuf1, &prerollBuf2);
    } else {
	oldmakePrerollFrame( &prerollBuf1, &prerollBuf2);
    }

    /*
     * Start the decompressor
     */

    startDecompressor();

    /*
     * Main loop.
     */

    for (;;) {
	if (playstate.advanceVideo) {
	    while (playstate.loopMode == LOOP_REPEAT || first)  {
		first = 0;
		audioFramesPlayed = 0;
		videoFramesSkipped = 0;
		prerollVideoFrames = 0;
		prerollAudioFrames = 0;
		if (playstate.timing)
       	    	    dmGetUST(af_ssel);
		preRollAndPlay(prerollBuf1, prerollBuf2);
		if(playstate.timing)
		    printTiming();
	    }
            if (playstate.autoStop) {
		if (childPid != -1)
		    kill(childPid, SIGHUP); 
		/* shut down decompressor */
		dmICDestroy(dmID.id);
		exit(0);
            }
	    playstate.advanceVideo = 0;
	} else
            /* wait for user to trigger advanceVideo = 1 */
	    sginap (10);
    } /*for (;;) */

} /* streamDecompress */


/********
*
* preRollAndPlay - do pre-roll, play the movie, and clean up
*
********/

static void preRollAndPlay
    (
    DMbuffer prerollBuf1,
    DMbuffer prerollBuf2
    )
{
    /*
     * Start the video output.
     */
    
    dmID.inustmsc.msc = 0;
    dmID.outmsc = 0;

    if ( playstate.audioActive ) {
	newPreRoll(prerollBuf1, prerollBuf2);
    }

    /*
     * Read the movie file and play the audio and images.
     */

    if (!DO_FRAME)
	playMovie( playImageForFrame);
    else
	playMovie( oldplayImageForFrame );

    /* wait for last frame to be decompressed */
    while (dmID.outmsc < dmID.inustmsc.msc - 1) {
	sginap( 0 );
    }
    /* wait for the last frame to be sent to video/graphics */
    while (dmID.bufferDone == 0) {
	sginap( 0 );
    }

    /*
     * Wait for the audio queue to drain.
     */

    if ( playstate.audioActive ) {
	while ( ALgetfilled( audio.outPort ) > 0 ) {
	    sginap( 0 );
	}
    }

    if (options.verbose) {
       printf("Movie done.\n");
    }



} /* preRollAndPlay */


/*
 * findSecondField
 */
static char *findSecondField
    (
    char *buf,
    int size
    )
{
    char *p;
    char *last;

    /* Assume the break between the two fields is not in the first 4/9 */
    last = buf + size - 1 - strlen(fldbdry);
    for (p = buf + 4*size/9; p < last; p++) {
	if (bcmp((const void *)p, (const void *)fldbdry, strlen(fldbdry)) == 0) {
	    return p+2;
	}
    }

   /* search the first 4/9 */
    last = buf + (size*4)/9;
    for (p = buf; p < last; p++) {
	if (bcmp((const void *)p,(const void *)fldbdry,strlen(fldbdry)) == 0) {
	    return p+2;
	}
    }

    ERROR( "Cannot separate two fields of the frame");

    return(NULL);
} /* findSecondField */




/*
 * getInputDMbuffer
 */

static void getInputDMbuffer
    (
    DMbuffer *bufptr
    )
{
    fd_set fds;

    /* wait until a dmBuffer is available in inpool */
    do {
	FD_ZERO(&fds);
	FD_SET(dmID.inpoolfd, &fds);
	

	if (select(dmID.inpoolfd+1, NULL, &fds, NULL, NULL) < 0) {
            ERROR("select failed\n");
	}

    } while (dmBufferAllocate(dmID.inpool, bufptr) != DM_SUCCESS);
} /* getInputDMbuffer */


/*
 * sendDMbufferTodmIC
 *
 */
static void sendDMbufferTodmIC
    (
    DMbuffer dmbuf
    )
{
    fd_set fds;

    /* Wait till there's room in outpool */
    FD_ZERO(&fds);
    FD_SET(dmID.outpoolfd, &fds);
    if (playstate.timing && (c_b4_ssel < MREC)) {
	dmGetUST((unsigned long long *)b4_ssel+c_b4_ssel);
	c_b4_ssel++;
    }
    if (select(dmID.outpoolfd+1, NULL, &fds, NULL, NULL) < 0) {
	    ERROR("select failed\n");
    }
    if (playstate.timing && (c_af_ssel < MREC)) {
	dmGetUST((unsigned long long *)af_ssel+c_af_ssel);
	c_af_ssel++;
    }
    /* Send frame or field to decompress */
    dmID.inustmsc.msc++;
    dmGetUST((unsigned long long *)&dmID.inustmsc.ust);
    dmBufferSetUSTMSCpair(dmbuf, dmID.inustmsc);
    if (dmICSend(dmID.id, dmbuf, 0, 0) != DM_SUCCESS) {
	    fprintf(stderr, "Frame %d:\n", frameNum);
	    ERROR( dmGetError( NULL, NULL ) );
    }

    return;
} /* sendDMbufferTodmIC */




/*
 * newPreRoll
 *
 * Just put in a few frames of video and audio to keep things
 * moving. We will rely on checkSync to keep audio and 
 * video in sync, repeating or dropping video frames as necessary.
 */

static void newPreRoll
    (
    DMbuffer prerollBuf1,
    DMbuffer prerollBuf2
    )
{
    int audioPrerollSamples = 0;
    int i;

    prerollBuf2 = prerollBuf2; /* this is to get the compiler to stop warning */
    /*
     * Put out the video pre-roll.  This puts up to video.preroll
     * frames into the compressed data buffer. 
     */
    for (i = 0; i < video.preroll; i++) {
	sendDMbufferTodmIC(prerollBuf1);
	if (DO_FIELD)
	    sendDMbufferTodmIC(prerollBuf1);
    }
    prerollVideoFrames += video.preroll;

    /*
     * Put audio preroll into the audio queue. We need to check for 
     * audio preroll to always be less than video preroll, though.
     */
	bzero( audioBuffer, audio.preroll * audio.blockSize * audio.frameSize );
	audioPrerollSamples = audio.preroll * audio.blockSize * 
	    audio.channelCount;
	if ( ALgetfillable(audio.outPort) < audioPrerollSamples ) {
	    ERROR( "Audio preroll buffer is too small\n" );
	}
	dmGetUST((unsigned long long *)&audioStartTime);
	if ( ALwritesamps( audio.outPort, audioBuffer, 
			  audioPrerollSamples ) != 0){
	    ERROR( "Unable to write audio samples." );
	}
	audioFramesPlayed += audioPrerollSamples / audio.channelCount;
	prerollAudioFrames += audio.preroll;
} /* newPreRoll */


/********
*
* playMovie - play a movie's audio and video
*
* This function assumes that the decompressor and audio output port
* have been set up, and that audio and video have been synchronized.
*
* This furnction is responsible for restoring synchronization when
* things get off.  The strategy is to either drop every other video
* frame or repeat video frames until things are correct.
*
* The value returned by checkSync is the number of frames that the video
* is ahead of the audio.
********/

static void playMovie
    (
     PFNPLAYIMG	PlayFunc
    )
{
    int firstSync    = 40;	/* First frame to check sync */
    int framesOff = 0;
    int spacing, spc;

    spc = playstate.syncInterval;
    for ( frameNum = 0;  frameNum < image.numberFrames;  frameNum++ ) {
	if ( playstate.audioActive )
	    playAudioForFrame( frameNum );

	spc--;
	if ( (framesOff == 0) || (spc != 0)) {
	    (*PlayFunc)( frameNum, 0 );
	}
	else if ( framesOff > 0 ) {
	    fprintf(stderr, "dmplay: frame %d repeated\n", frameNum);
	    (*PlayFunc)( frameNum, 1 );
	    videoFramesSkipped -= 1;
	    framesOff -= 1;
	    spc = spacing;
	}
	else /* framesOff < 0 */ {
	    fprintf(stderr, "dmplay: frame %d skipped\n", frameNum);
	    videoFramesSkipped += 1;
	    framesOff += 1;
	    spc = spacing;
	}

	if (playstate.audioActive && (frameNum > firstSync)) {
	    if ( (frameNum - firstSync ) % playstate.syncInterval == 0 ) {
		framesOff = checkSync();
		if(framesOff == 0) {
		    continue;
		}
		if (framesOff > 0) {
		    spacing = framesOff;
		} else {
		    spacing = -framesOff;
		}
		if (spacing > playstate.syncInterval) {
		    printf("Video and audio are too much out of sync\n");
		    exit(1);
	    	}
		spacing = playstate.syncInterval / spacing;
		if (spacing < 2) {
		    spacing = 2;
		}
		spc = spacing;

	    }
	}
    }


    if (playstate.loopMode == LOOP_FRAME && image.display != GRAPHICS
	&& !playstate.autoStop) {
	for (;;) 
	    (*PlayFunc)(image.numberFrames - 1, 0);
    }
} /* playMovie */


/********
*
* playAudioForFrame - play the audio for a given image frame number
*
********/

static void playAudioForFrame
    (
    int frame
    )
{
    int audioStart;
    int audioEnd;
    int audioCount;

    /*
     * Determine which audio frames correspond to this image frame
     * number.
     */

    mvMapBetweenTracks( movie.imgTrack, movie.audTrack, frame,
		        (MVframe*) &audioStart );
    mvMapBetweenTracks( movie.imgTrack, movie.audTrack, frame + 1,
		        (MVframe*) &audioEnd );
    audioCount = audioEnd - audioStart;

    /*
     * If we're past the end of the audio track, don't do anything.
     */

    if (( audioCount <= 0) || ( audio.frameCount < audioEnd )) {
	return;
    }

    /*
     * Get the audio frames from the movie.
     */
    audioCount = audioEnd - audioStart;

    if ( mvReadFrames( movie.audTrack,
		       audioStart,	/* first frame to read */
		       audioCount,	/* number of frames to read */
		       audioBufferSize, /* size of buffer (bytes) */
		       audioBuffer	/* where to put the data */
		       ) != DM_SUCCESS ) {
	ERROR( "mvReadFrames failed" );
    }

    /*
     * Write the audio to the audio port.  (This will block until
     * there is room in the queue.)
     */

    {
	int audioSampleCount = audioCount * audio.channelCount;
	if ( ALwritesamps( audio.outPort,
			  audioBuffer,
			  audioSampleCount ) != 0 ) {
	    ERROR( "ALwritesamps failed" );
	}
	audioFramesPlayed += audioCount;
    }

    /*
     * Check for underflow in the audio buffer.  This is a
     * conservative test, checking after we have written the frame
     * out, to make sure that they are still there; i.e., that
     * previously written frame are still going out.
     */
    
    if ( options.verbose ) {
	if ( ALgetfilled( audio.outPort ) < audioCount ) {
	    printf( "Audio buffer underflow: frame %d\n", frame );
	}
    }
}




/********
*
* oldplayImageForFrame - get one image and give it to the decompressor.
*
* Handles field movies of the old format and frame movies.
*
********/

static void oldplayImageForFrame
    (
    int frame, int repeat
    )
{
    int size;
    char *f1, *f2;
    DMbuffer dmbuf1, dmbuf2;
    int f1size, f2size;

    assert( ( 0 <= frame ) && ( frame < image.numberFrames ) );

    if (DO_FRAME) {
	getInputDMbuffer(&dmbuf1);
	f1 = (char *)dmBufferMapData(dmbuf1);
	size = mvGetCompressedImageSize( movie.imgTrack, frame );
	if (mvReadCompressedImage(movie.imgTrack, frame, size, f1) != DM_SUCCESS) {
	    ERROR( "mvReadCompressedImage failed" );
	}
	dmBufferSetSize(dmbuf1, size);
	sendDMbufferTodmIC(dmbuf1);
	if (repeat) sendDMbufferTodmIC( dmbuf1 );
	dmBufferFree(dmbuf1);
	return;
    }

    size = mvGetCompressedImageSize( movie.imgTrack, frame );
    if (mvReadCompressedImage(movie.imgTrack, frame, size, imageBuffer) != DM_SUCCESS) {
	ERROR("mvReadCompressedImage failed\n");
    }
    f1 = imageBuffer;
    f2 = findSecondField(f1, size);
    f1size = f2 - f1;
    f2size = size - f1size;

    getInputDMbuffer(&dmbuf1);
    getInputDMbuffer(&dmbuf2);

    bcopy(f1, dmBufferMapData(dmbuf1), f1size);
    dmBufferSetSize(dmbuf1, f1size);
    sendDMbufferTodmIC(dmbuf1);
    
    bcopy(f2, dmBufferMapData(dmbuf2), f2size);
    dmBufferSetSize(dmbuf2, f2size);
    sendDMbufferTodmIC(dmbuf2);
    
    if (repeat)
    {
	sendDMbufferTodmIC(dmbuf2);
	sendDMbufferTodmIC(dmbuf2);
    }

    dmBufferFree(dmbuf1);
    dmBufferFree(dmbuf2);

} /* oldplayImageForFrame */




/********
*
* playImageForFrame - get one image and give it to the decompressor.
*
* Handles field movies of the new format.
*
********/

static void playImageForFrame
    (
    int frame, int repeat
    )
{
    DMbuffer dmbuf1, dmbuf2;
    size_t f1size, f2size;
    off64_t gap;
    int dataIndex;
    MVframe frameOffset;


    assert( ( 0 <= frame ) && ( frame < image.numberFrames ) );

    getInputDMbuffer(&dmbuf1);
    getInputDMbuffer(&dmbuf2);

    /* Read the image into the dmBuffer */
    if (mvGetTrackDataIndexAtTime(movie.imgTrack, 
	 (MVtime)(((double)frame) * image.timeScale / image.frameRate),
	 image.timeScale, &dataIndex, &frameOffset) != DM_SUCCESS) {
	ERROR("mvGetTrackDataIndexAtTime failed");
    }
    if (mvTrackDataHasFieldInfo( movie.imgTrack, dataIndex ) == DM_TRUE)
    {
	if (mvGetTrackDataFieldInfo(movie.imgTrack, dataIndex, &f1size, &gap, &f2size) != DM_SUCCESS) {
	    ERROR("mvGetTrackDataFieldInfo failed");
	}
	if (mvReadTrackDataFields(movie.imgTrack, dataIndex, dmID.inbufsize, 
				  dmBufferMapData(dmbuf1), dmID.inbufsize, 
				  dmBufferMapData(dmbuf2)) != DM_SUCCESS) {
	    ERROR( "mvReadTrackDataFields failed" );
	}
	/* send first field */
	dmBufferSetSize(dmbuf1, f1size);
	sendDMbufferTodmIC(dmbuf1);
    
	/* send second field */
	dmBufferSetSize(dmbuf2, f2size);
	sendDMbufferTodmIC(dmbuf2);
    }
    else
    {
	size_t 	size;
	int	f1size, f2size;
	u_char	*f1, *f2;
	int	paramsId, numFrames;
	MVdatatype	dataType;
	
	if (mvGetTrackDataInfo( movie.imgTrack, dataIndex,
			        &size, &paramsId, 
			        &dataType, &numFrames ) != DM_SUCCESS)
	{
	    ERROR( "mvGetTrackDataInfo failed\n" );
	}
			       
	if (mvReadTrackData( movie.imgTrack, dataIndex,
			     size, imageBuffer ) != DM_SUCCESS)
	{
	    ERROR( "mvReadTrackData failed\n" );
	}
	f1 = (u_char *) imageBuffer;
	f2 = (u_char *) findSecondField((char *) f1, size);
	f1size = f2 - f1;
	f2size = size - f1size;

	bcopy(f1, dmBufferMapData(dmbuf1), f1size);
	dmBufferSetSize(dmbuf1, f1size);
	sendDMbufferTodmIC(dmbuf1);

	bcopy(f2, dmBufferMapData(dmbuf2), f2size);
	dmBufferSetSize(dmbuf2, f2size);
	sendDMbufferTodmIC(dmbuf2);
    }

    if (repeat)
    {
	sendDMbufferTodmIC(dmbuf2);
	sendDMbufferTodmIC(dmbuf2);
    }
    
    dmBufferFree(dmbuf1);
    dmBufferFree(dmbuf2);

} /* playImageForFrame */




/********
*
* checkSync - see if the audio and video are still in sync
*
* The pre-roll before the movie starts ensures that the audio and
* image tracks start out in sync.  The two can lose sync if either the
* audio buffer or the compressed image buffer underflow, causing a gap
* in the audio or a repeated image.
*
* This function checks the synchronization of audio and video by
* checking the current frame number in each stream.  The term
* Universal System Time (UST) is a system-wide clock with nanosecond
* resolution that is used as a time base for measuring
* synchronization.  The term Media Stream Counter (MSC) refers to the
* number of frames that have passed so far in a stream of data.
*
* This diagram illustrates what is going on.  Each V is the start
* of a video field, and each A is a part of the audio track.  The
* numbers indicate the steps in checking sync.
*
*
*
*                (1)--+
*                     |
*                     v
*
*     V---------------V---------------V---------------V---------------V
*
*                     |
*                +----+ 		time -->
*                |
*                v
*
*     AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
*
*                ^        ^
*                |        |
*           (3)--+   (2)--+
*
*	         |----|   (4) sync error
*
*
* (1) Get a UST/MSC pair for the video stream.  This tells us the time (UST)
*     at which a video field (MSC) started being displayed.
*
* (2) Get a UST/MSC pair for the audio stream.  This tells us the time
*     it which one of the audio frames hit the speaker.  NOTE: the
*     video and audio timestamps will be at different times.
*
* (3) Figure out which audio sample should correspond to the start of
*     the video field from (1), and compute the UST for this sample
*     from the audio timestamp (from 2) and the audio rate.
*
* (4) If everthing is synchronized, the times from (1) and (4) should
*     be the same; the video field and the corresponding audio frame
*     should be sent out at the same time.  If the sync is off, the
*     two times will be different.
*
* Sync problems will be corrected by dropping or repeating video
* frames (not fields).  Thus, we will do nothing if the audio and 
* video times are within 1/2 of a frame time.
*
* UST and MSC are of type "long long", but we use doubles in this
* routine to avoid overflow problems.  The USTs are measured from the
* time this movie was started playing so that they produce reasonable
* numbers when debugging.
*
********/

static int checkSync (void)
{
    double bufferRate;
    double audioRate = audio.frameRate;

    double video_ust1;
    double video_msc1;
    double audio_ust2;
    double audio_msc2;
    double audio_ust3;
    double audio_msc3;
    
    double sync_error;
    double framesOff;


    if (DO_FRAME) {
	bufferRate = image.frameRate;
    } else {
	bufferRate = image.frameRate*2;
    }

    /*
     * Get a timestamp for the video stream: a buffer number and the
     * time at which it started going out.
     */
    
    {
	unsigned long long msc, ust;

	LOCK_DMID;
	msc = dmID.outmsc;
	ust = dmID.outust;
	UNLOCK_DMID;
	video_ust1 = ust - videoStartTime;
	if(DO_FRAME) {
	    video_msc1 = msc - (prerollVideoFrames + FIRST_FIELD) + videoFramesSkipped;
	} else {
	    video_msc1 = msc - ( prerollVideoFrames * 2 + FIRST_FIELD ) + videoFramesSkipped * 2;
	}
    }
    
    /*
     * Get a timestamp for the audio stream.
     */
    
    {
	unsigned long long now;
	int filled = ALgetfilled( audio.outPort ) / audio.channelCount;
	dmGetUST( &now );
	audio_ust2 = now - videoStartTime;
	audio_msc2 = audioFramesPlayed - filled - prerollAudioFrames;
    }
    
    /*
     * Figure out which audio sample corresponds to the start of the
     * video frame for which we have a timestamp.  Estimate the
     * timestamp for that audio frame.
     */
    
    audio_msc3 = video_msc1 * audioRate / bufferRate;
    audio_ust3 = audio_ust2 + ( audio_msc3 - audio_msc2 ) * 
	         NANOS_PER_SEC / audioRate;

    /*
     * Determine the amount of sync error.  This is the amount of time
     * that the image track is ahead of the audio track.
     */
    
    sync_error = audio_ust3 - video_ust1;
    if (DO_FRAME) {
	framesOff = sync_error *bufferRate /NANOS_PER_SEC;
    } else {
	framesOff = sync_error * bufferRate / 2.0 / NANOS_PER_SEC;
    }
    /*
     * Right now, the time it takes for a buffer to get through
     * the decompressor can vary substantially from time to time.
     * So if we take the computed results here too strictly, we
     * will likely end up skipping frame here and repeating frames
     * there because of the fluctuations. For overall performance,
     * it's better that we ignore small errors.
     */
    
    if ((-3.5 <= framesOff) && (framesOff < 3.5)) {
	return 0;
    }

    /*
     * Round the result, rather than truncating, to avoid jitter when
     * things are on track.	
     */

    if ( options.verbose > 1 ) {
	printf( "sync_error = %7.4f frames\n", framesOff );
    }
    return ((int) floor(framesOff + 0.5));
} /* checkSync */



/********
*
* oldmakePrerollFrame - Use frame 0
*
* For field movies of the old format and frame movies.
*
********/

static void oldmakePrerollFrame
    (
    DMbuffer *bufptr1,
    DMbuffer *bufptr2
    )
{
    int size;
    int f1size, f2size;
    char *frame, *field2;

    size = mvGetCompressedImageSize( movie.imgTrack, 0 );
    frame = (char *) malloc( size );
    if (frame == NULL) {
	ERROR ("Out of memory");
    }

    if (mvReadCompressedImage (movie.imgTrack, 0, size, frame) != DM_SUCCESS ) {
	ERROR ("mvReadCompressedImage failed");
    }

    if (DO_FRAME) {
	getInputDMbuffer(bufptr1);
	bcopy(frame, dmBufferMapData(*bufptr1), size);
	dmBufferSetSize(*bufptr1, size);
	*bufptr2 = NULL;
    } else {
	field2 = findSecondField(frame, size);
	f1size = field2 - frame;
	f2size = size - f1size;
	getInputDMbuffer(bufptr1);
	getInputDMbuffer(bufptr2);
	bcopy(frame, dmBufferMapData(*bufptr1), f1size);
	dmBufferSetSize(*bufptr1, f1size);
	bcopy(field2, dmBufferMapData(*bufptr2), f2size);
	dmBufferSetSize(*bufptr2, f2size);
    }
    free(frame);

} /* oldmakePrerollFrame */




/********
*
* makePrerollFrame - Use frame 0
*
* For field movies of the new format.
*
********/

static void makePrerollFrame
    (
    DMbuffer *bufptr1,
    DMbuffer *bufptr2
    )
{
    size_t f1size, f2size;
    off64_t gap;
    int dataIndex;
    MVframe frameOffset;

    getInputDMbuffer(bufptr1);
    getInputDMbuffer(bufptr2);
    if (mvGetTrackDataIndexAtTime(movie.imgTrack, (MVtime)0, image.timeScale, &dataIndex, &frameOffset) != DM_SUCCESS) {
	ERROR("mvGetTrackDataIndexAtTime failed");
    }
    if (mvGetTrackDataFieldInfo(movie.imgTrack, dataIndex, &f1size, &gap, &f2size) != DM_SUCCESS) {
	ERROR("mvGetTrackDataFieldInfo failed");
    }
    if (mvReadTrackDataFields(movie.imgTrack, 0, dmID.inbufsize, dmBufferMapData(*bufptr1), dmID.inbufsize, dmBufferMapData(*bufptr2)) != DM_SUCCESS) {
	ERROR("mvReadTrackDataFields failed\n");
    }
    dmBufferSetSize(*bufptr1, f1size);
    dmBufferSetSize(*bufptr2, f2size);
} /* makePrerollFrame */





/*
 * getdmICOutput
 */
static void getdmICOutput
    (
    void *dummy
    )
{
    fd_set fds;
    DMbuffer dmbuf;
    int bufferNum;
    int lastBufferNum;
    int topWindowLine;
    int interlaced;
    int dtime;
    long long currentTime;
    USTMSCpair outustmsc;    
    int startBuffer;
    long long nanosPerBuffer;
    long long ptime, utime;
    int counter = 1;

    int selectTest;
    struct timeval displayTimeout = {0,5000};
    struct timeval *selectTimeout = NULL;

    dummy = dummy;  /* to get the compiler warning to shut up */
    /* Get parent's process id */
    parentPid = getppid();

    /* Receive SIGHUP and quit if parent terminates */
    sigset(SIGHUP, SIG_DFL);
    (void)prctl(PR_TERMCHILD);


    interlaced = 0;

    if (DO_FRAME) {
	nanosPerBuffer = (long long)NANOS_PER_SEC/image.frameRate;
    } else {
	nanosPerBuffer = (long long)NANOS_PER_SEC/(image.frameRate*2);
	if (!HASIMPACTCOMP)
	    interlaced = 1;
    }

    if (HASIMPACTCOMP) {
	if (image.display != GRAPHICS) {
	    /* if it's not using gfx and going to be displayed on 
	       the screen, we create a X window. */
	    if (!options.videoOnly)
		x_open(image.width, image.height);
	    /* start vlBeginTransfer and connect the VL screen drain node
	       with the X window if the display is to the screen as well. */
	    vl_start();
	}
	else {
	    /* use Octane gfx to display the decoded images. */
    	    if (SINGLESTEP && DO_FIELD) {
            	topWindowLine = gfx_open(image.width, image.height/2,
                                 options.zoom, options.zoom,
                                 0, movie.title);
    	    } else {
            	topWindowLine = gfx_open(image.width, image.height,
                                 options.zoom, options.zoom,
                                 interlaced, movie.title);
    	    }
	}
    }
    else {
    	if (image.display != GRAPHICS) {
            vlBeginTransfer(video.svr, video.path, 0, 0);
    	}

  	if (!options.videoOnly) {
    	    if (SINGLESTEP && DO_FIELD) {
            	topWindowLine = gfx_open(image.width, image.height/2,
                                 options.zoom, options.zoom,
                                 0, movie.title);
    	    } else {
            	topWindowLine = gfx_open(image.width, image.height,
                                 options.zoom, options.zoom,
                                 interlaced, movie.title);
    	    }
  	}
    }
	
    dmID.idfd = dmICGetDstQueueFD(dmID.id);

  /* 
   * The first N buffers (where N is the number of buffers in the
   * output pool) take a disproportionally long time to get through
   * the ICE -- most probably because of allocation of DMbuffers.
   * This messes up the timing. So we push through N buffers here
   * and exclude them in our timing and synching.
   */

  if (dmID.speed == DM_IC_SPEED_REALTIME)
  {
    DMbuffer dmb[200];
    int i;

    for (i = 0; i < dmID.outbufs; i++) {
	sendDMbufferTodmIC(prerollBuf1);
back:
	FD_ZERO(&fds);
	FD_SET(dmID.idfd, &fds);
	if (select(dmID.idfd+1, &fds, NULL, NULL, NULL) <= 0) {
	    CERROR( "selecet failed");
	}
	if (dmICReceive(dmID.id, &dmb[i]) != DM_SUCCESS) {
	    if (options.verbose)
		fprintf(stderr, "dmICReceive is failed in frame %d\n", i);
	    goto back;
	}
    }
    for (i = 0; i < dmID.outbufs; i++) {
	dmBufferFree(dmb[i]);
    }

  }

    unblockproc(parentPid);

    dmID.outust = 0;
    dmID.outmsc = 0;
    init_lock( &(dmID.lock) );
    dmID.bufferDone = 0;

    if (playstate.timing)
	dmGetUST(b4_rsel);

    for (;;) {
	/* Get a field as output from dmIC */
	FD_ZERO(&fds);
	FD_SET(dmID.idfd, &fds);
	if (playstate.timing && (c_b4_rsel < MREC)) {
	    dmGetUST(b4_rsel+c_b4_rsel);
	    c_b4_rsel++;
	}


	/* Only force a time-out when we're sure that there's a window
	 * that might be getting events */
       	selectTimeout = (dpy==NULL)?NULL:(&displayTimeout);

	if ((selectTest=select(dmID.idfd+1, 
			       &fds, NULL, NULL, selectTimeout)) < 0) 
	  {
	    CERROR( "select failed");
	  }

	/* Handle X events.. Returns immediately if no events are pending. */
	if (dpy) 
	  handleXEvents();

	/* If we timed-out of select, there's no new frame waiting for us */
	if (selectTest == 0)
	  continue;
	
	if (playstate.timing && (c_af_rsel < MREC)) {
	    dmGetUST(af_rsel+c_af_rsel);
	    c_af_rsel++;
	}
	if (dmICReceive(dmID.id, &dmbuf) != DM_SUCCESS) {
	    continue;
	}
	outustmsc = dmBufferGetUSTMSCpair(dmbuf);

	bufferNum = outustmsc.msc;

	if (options.verbose && (bufferNum > 1) && (bufferNum != (lastBufferNum+1))) {
	    printf("Lost buffer(s) between %d and %d\n", lastBufferNum, bufferNum);
	}
	if (playstate.speedcontrol && (image.display == GRAPHICS)) {
	    if ((bufferNum == 1) || (videoStartTime == 0)) {
		startBuffer = bufferNum;
		dmGetUST((unsigned long long *)&videoStartTime);
	    } else {
		dmGetUST((unsigned long long *)&currentTime);
		ptime = nanosPerBuffer*(bufferNum - startBuffer);
		utime = currentTime - videoStartTime;
		dtime = (ptime - utime)/NANOS_PER_TICK;
		if (dtime > 0) {
		    sginap(dtime-1);
		}
	    }
	}

	/* Only time-stamp on a pair of buffers. Timing for each
	 * buffer seems to be showing a long-short-long-short
	 * pattern, with the timing for a pair remaining fairly
	 * constant.
	 */
	if (((bufferNum - startBuffer) & 1) == 0) {
	    LOCK_DMID;
	    dmID.outmsc = outustmsc.msc;
	    dmGetUST((unsigned long long *)&(dmID.outust));
	    dmID.bufferDone = 0;
	    UNLOCK_DMID;
	}
	lastBufferNum = bufferNum;

      if (image.display != NO_DISPLAY){
       if (!options.videoOnly) {
	if (DO_FRAME) {
	    gfx_show(topWindowLine, image.height, dmBufferMapData(dmbuf), 
			counter);
	} else if (SINGLESTEP) {
	    gfx_show(topWindowLine, image.height/2, dmBufferMapData(dmbuf),
			counter);
	} else if (((bufferNum & 1) && (image.interlacing == DM_IMAGE_INTERLACED_ODD))
	     || (((bufferNum & 1) == 0) && (image.interlacing == DM_IMAGE_INTERLACED_EVEN))) {
	    gfx_show(topWindowLine-1, image.height/2, dmBufferMapData(dmbuf),
			counter);
	} else {
	    gfx_show(topWindowLine, image.height/2, dmBufferMapData(dmbuf),
			counter);
	}
       }

	if (image.display != GRAPHICS) {
	    if (vlDMBufferPutValid(video.svr, video.path, video.src, dmbuf)
                < 0)
		CERROR( "vlDMBufferPutValid failed");
	}
      }
	counter++;
	dmBufferFree(dmbuf);
	dmID.bufferDone = 1;

	if (SINGLESTEP) {
	    char c;
	    if (DO_FRAME) {
		printf("Done frame %d\n", bufferNum-1);
	    } else {
		printf("Done field %d\n", bufferNum);
	    }
	    scanf("%c", &c);
	}
    }
    /* 
     * unreachable -> 
     * childPid = -1; 
     * exit(0);
     */
} /* getdmICOutput */


/*
 * startDecompressor
 */

static void startDecompressor
    (
    void
    )
{

    if (childPid != -1) {
	/* Child process is already running */
	return;
    }

    /* Spin off a thread to feed dmIC output to video/graphics */
    if ((childPid = sproc(getdmICOutput, PR_SADDR|PR_BLOCK|PR_SFDS, &dmID)) == -1) {
        ERROR("sproc() failed");
    }
    if (dmID.outpoolfd == -1) {
	dmID.outpoolfd = dmBufferGetPoolFD(dmID.outpool);
    }
    
} /* startDecompressor */


static void handleXEvents() {
  XEvent event;
  KeySym key;
  pid_t parentPid = getppid();

  while (XPending(dpy)){

    XNextEvent(dpy,&event);
    
    switch (event.type){
    case KeyPress:
      key = XLookupKeysym((XKeyEvent *) &event, 0);
      if (key == XK_Escape){
	fprintf (stderr, "Escape key pressed - Exiting..\n");
	kill(parentPid, SIGHUP);
      }

    /* vvv Fall through on purpose vvv */
    case ButtonRelease:
    case ButtonPress:

      if (playstate.advanceVideo == 0){
	/* The read thread is waiting for user input (key or buttonpress)
	 * to exit. Update the state so the parent thread knows we got it */
	playstate.autoStop = 1;
	playstate.advanceVideo = 1;
      }
      break;

    case ClientMessage:
      /* The only Client Message we signed up to get was WM_DESTROY,
       * so we should stop the master thread here */
      fprintf (stderr, "Window Closed - Exiting..\n");
      kill(parentPid, SIGHUP);
      exit (0);
      break;
    }
  }
  
}

