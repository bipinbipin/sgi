/**********************************************************************
*
* File: streamDecompress.c
*
**********************************************************************/

/*
 * Continous decompression / display module.
 * This module does the work for the following playback modes:
 *
 * dmplay -p video,device=ev1,engine=cosmo [-p audio]
 * dmplay -p graphics,engine=sw
 * dmplay -p graphics,engine=cosmo
 *
 * Audio playback is implemented only for the
 * realtime display path through cosmo and ev1 video.
 * Here we synchronize playback by priming the audio and
 * video output pipes with silent/black preroll frames.
 * See the comments in the code below.
 *
 * For the nonrealtime display paths,
 * video playback would have to
 * be synchronized to the audio output rate.
 * The program would drop video frames as necessary to stay
 * in sync with audio.
 *
 */

#include <math.h>		/* floor() */

#include "dmplay.h"

/********
*
* Constants
*
********/

#define FRAMEBUFFERSIZE		4	/* Number of fields in the */
					/* uncompressed data buffer */
					/* must be a multiple of 2 -- */
					/* the graphicsDisplay() */
					/* function assumes that pairs */
					/* of fields never wrap. */

#define GRAPHICS_DELAY		( FRAMEBUFFERSIZE / 2 )
					/* The number of frames that */
					/* the graphics display lags */
					/* the decompression.  Must be */
					/* greater than 0 because of */
					/* buffering in Cosmo. */

#define COMPRESSEDBUFFERSIZE	(1024 * 1024 * 1)
					/* Size of the ring buffer */
					/* that feeds the */
					/* decompressor.  This is */
					/* enough room for about 10 to */
					/* 15 frames. */

#define FIRST_FIELD		1	/* The number of the first field */
					/* as returned by clGetNextImageInfo */

#define VIDEOPRIME		2	/* Number of frames to send to */
					/* make sure that there is no */
					/* garbage in the frame buffer */
					/* when we start video. */

#define VIDEOPREROLL		15	/* Number of frames to prime output */

#define AUDIOPREROLL		 2 	/* Number of video frame times */
					/* to prime audio output. */
					/* Must be less than VIDEOPREROLL */

#define NANOS_PER_SEC            1000000000

/********
*
* Prototypes for functions local to this file.
*
********/

static void preRollAndPlay(void);
static void primeVideo(int, void*);
static void preRoll(int, void*);
static void playMovie(void);
static void playAudioForFrame(int frame);
static void playImageForFrame(int frame);
static int  checkSync(void);
static void waitForDrain(void);
static void freezeVideo(void);
static void graphicsDisplay(void);
static int  makePrerollFrame(void** returnCompressedData);
static void startDecompressor(void);
static void waitForFieldToDecompress(int);

/********
*
* Global variables (local to this file).
*
********/

static CLbufferHdl dataHdl;	/* Buffer for compressed image data. */
				/* This is where the decompressor gets */
				/* its input. */

static CLbufferHdl frameHdl;	/* Buffer for uncompressed image data. */
				/* This is used only in GRAPHICS */
				/* display mode. */

static void* tempBuffer;	/* Buffer used for de-interlacing */
				/* images. */

static void* audioBuffer;	/* Buffer used for audio data.  Used */
static int audioBufferSize;	/* during pre-roll and during */
				/* playback. */

static void* unwrapBuffer;	/* Buffer used to split compressed */
static int unwrapBufferSize;	/* image data when writing across the */
				/* wrap point in a ring buffer. */

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

static unsigned long long startTime;

/********
*
* streamDecompress - Run the streamed movie player
*
********/

extern vlInitEv1(void);

void streamDecompressEv1
    (
    void
    )
{
    static int first = 1;

    mvInit();
    alInit();
    vlInitEv1();
    initPlayState(); 
    Xgo();

    /*
     * Initialize global variables.
     */

    dataHdl = NULL;
    frameHdl = NULL;
    tempBuffer = NULL;
    audioBuffer = calloc( audio.queueSize, audio.frameSize );
    if ( audioBuffer == NULL) {
	ERROR( "out of memory" );
    }
    audioBufferSize = audio.queueSize * audio.frameSize;
    unwrapBuffer = NULL;
    unwrapBufferSize = 0;
    prerollVideoFrames = 0;
    prerollAudioFrames = 0;
    audioFramesPlayed = 0;
    videoFramesSkipped = 0;
    dmGetUST( &startTime );

    /*
     * Set the priority of this thread.
     */

    setscheduling();

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
		preRollAndPlay();
	    }
            if (playstate.autoStop) {
                stopAllThreads();
            }
	    playstate.advanceVideo = 0;
	} else
            /* wait for user to trigger advanceVideo = 1 */
	    sginap (10);
    } /*for (;;) */
}

/********
*
* preRollAndPlay - do pre-roll, play the movie, and clean up
*
********/

static void preRollAndPlay (void)
{
    int prerollFrameSize = 0;
    void* prerollFrame = NULL;
    
    /*
     * Make a compressed black frame for use by pre-roll.  We have to
     * do it before starting the decompresor, because Cosmo can only
     * have one thing happening at once.
     */
    
    if ( image.display != GRAPHICS ) {
	prerollFrameSize = makePrerollFrame( &prerollFrame );
    }

    /*
     * Start the decompressor
     */
    clInit( 1 );
    startDecompressor();
    
    /*
     * Start the video output.
     */
    
    if ( image.display != GRAPHICS ) {
	primeVideo( prerollFrameSize, prerollFrame );
    }

    /*
     * If we're playing audio with the video, we'd like to synchronize
     * them. We only support synchronized audio and video if the
     * images are displayed through the video library.
     */

    if ( playstate.audioActive ) {
	if ( image.display != GRAPHICS ) {
	    preRoll( prerollFrameSize, prerollFrame );
	}
    }
    if ( prerollFrame != NULL ) {
	free( prerollFrame );
    }

    /*
     * Read the movie file and play the audio and images.
     */

    playMovie();

    /*
     * Let the streams drain out.
     */

    waitForDrain();
}

/********
*
* primeVideo
*
* This function is responsible for starting the video stream.  We need
* to do this before pre-rolling, because it takes a significant amount
* of time.
*
********/

static void primeVideo
    (
    int prerollFrameSize,
    void* prerollFrame
    )
{
    int i;
    
    /*
     * Send a couple of black frames through the decompressor.
     */
    
    for ( i = 0;  i < VIDEOPRIME;  i++ ) {
	int preWrap;
	int wrap;
	void* freeData;
	preWrap = clQueryFree( dataHdl, 0, &freeData, &wrap );
	if ( preWrap < prerollFrameSize ) {
	    ERROR( "Preroll buffer is too small\n" );
	}
	bcopy( prerollFrame, freeData, prerollFrameSize );
	if ( clUpdateHead( dataHdl, prerollFrameSize ) < SUCCESS ) {
	    ERROR( "clUpdateHead failed" );
	}
    }
    prerollVideoFrames += VIDEOPRIME;
    
    /*
     * Wait until the first full frame is decompressed.
     */
    
    waitForFieldToDecompress( FIRST_FIELD + 1 );
		
    /*
     * if we haven't started the data transfer, start it now
     */

    if (video.dataActive == 0 && image.display != GRAPHICS) {
	VLControlValue val;

	video.dataActive = 1;
	val.intVal = video.windowId;
	vlSetControl(video.svr, video.path, video.drn, VL_WINDOW, &val);

	if (vlBeginTransfer(video.svr, video.path, 0, NULL)) {
	    vlPerror("screenPath couldn't transfer");
	    stopAllThreads ();
	}
    }

    /*
     *  if the video was frozen, time to unfreeze ...
     */

    if ( video.dataFrozen ) {
	VLControlValue val;

	video.dataFrozen = 0;
	val.boolVal = FALSE;
	vlSetControl(video.svr, video.path, video.drn,
		     VL_FREEZE, &val);
	vlSetControl(video.svr, video.path, video.voutdrn,
		     VL_FREEZE, &val);
    }
}

/********
*
* preRoll - get audio and video output synchronized
*
* Synchronization of the audio and video stream is accomplished by
* sending out black images and silent audio to get things rolling and
* then adjusting the timing until the two are in sync:
*
* (1) Silence and blackness are queued up for output and started. This
*     is refered to as ``pre-roll''.
* (2) The relative positions of the audio and video queues are measured.
* (3) Additional silence is sent so that the first active video will
*     meet up with the first active audio.
*
* Once this is done, the audio and video streams are in sync.  As long
* as the audio output buffer and the compressed video buffer are fed
* they should remain in sync.
*
* This routine assumes that the image decompressor has already been
* started and that the audio output port is set up.
*
********/

static void preRoll
    (
    int prerollFrameSize,
    void* prerollFrame
    )
{
    int audioPrerollSamples = 0;
    long long videoStartTime;
    long long audioStartTime;
    int i;
    int audioFramesNeeded;

    long long debugStartTime;
    dmGetUST( (unsigned long long*) &debugStartTime );

    /*
     * Put out the video pre-roll.  This puts up to VIDEOPREROLL
     * frames into the cl compressed data buffer.  If the pre roll
     * frame is large, then somewhat less than VIDEOPREROLL frames
     * will be inserted.
     */

    for ( i = 0;  i < VIDEOPREROLL;  i++ ) {
	int preWrap;
	int wrap;
	void* freeData;
	preWrap = clQueryFree( dataHdl, 0, &freeData, &wrap );
	if ( preWrap < prerollFrameSize )
	    break;
	bcopy( prerollFrame, freeData, prerollFrameSize );
	if ( clUpdateHead( dataHdl, prerollFrameSize ) < SUCCESS ) {
	    ERROR( "clUpdateHead failed" );
	}
    }
    prerollVideoFrames += i;

    /*
     * Put audio preroll into the audio queue. We need to check for 
     * audio preroll to always be less than video preroll, though.
     */
    if(prerollVideoFrames > AUDIOPREROLL) {
	bzero( audioBuffer, AUDIOPREROLL * audio.blockSize * audio.frameSize );
	audioPrerollSamples = AUDIOPREROLL * audio.blockSize * 
	    audio.channelCount;
	if ( ALgetfillable(audio.outPort) < audioPrerollSamples ) {
	    ERROR( "Audio preroll buffer is too small\n" );
	}
	if ( ALwritesamps( audio.outPort, audioBuffer, 
			  audioPrerollSamples ) != 0){
	    ERROR( "Unable to write audio samples." );
	}
	audioFramesPlayed += audioPrerollSamples / audio.channelCount;
    }
    /*
     * Calculate the time that the next field (the first real field)
     * that we put into the output queue will be sent out.
     * This is the time of the current field plus the time to output
     * the remaining pre-roll fields.
     *
     * (Remember that Cosmo starts counting fields at 1.)
     *
     * (clGetNextImageInfo will block until the first active video
     * field goes out of Cosmo).
     */

    {
	CLimageInfo imageinfo;
	int nanosPerField = NANOS_PER_SEC / (image.frameRate * 2 );
	int firstRealField = prerollVideoFrames * 2 + FIRST_FIELD;
	clGetNextImageInfo( codec.Hdl, &imageinfo, sizeof imageinfo );
	videoStartTime =
	    imageinfo.ustime +
	    nanosPerField * ( firstRealField - imageinfo.imagecount );
	if ( options.verbose > 2 ) {
	    printf( "Pre-roll image info: start time = %llu, count = %d\n",
		    videoStartTime,
		    imageinfo.imagecount );
	}
    }

    /*
     * Calculate the time that the next audio frame
     * that we put into the output queue will be sent out.
     * This is the time of the current frame plus the time to output
     * the remaining pre-roll frames.
     *
     * The AL does not have a time stamp mechanism in 5.2, but its latencies
     * are low (approx. 2ms) so we treat it here as instantaneous.
     * (Such a mechanism exists in 5.3.)
     */

    {
	int audioSamplesQueued;
	int audioFramesQueued;
	unsigned long long now;
	long long queueTime;
	dmGetUST( &now );
	audioSamplesQueued = ALgetfilled( audio.outPort );
	if ( audioSamplesQueued < 0 ) {
	    ERROR( "ALgetfilled failed" );
	}
	audioFramesQueued = audioSamplesQueued / audio.channelCount;
	queueTime = ((long long) audioFramesQueued) * NANOS_PER_SEC / 
	            ((long long) audio.frameRate);
	audioStartTime = now + queueTime;
	if(queueTime == 0)
	    printf( "Audio preroll was insufficient.\tContinuing...\n");
	if ( options.verbose > 2 ) {
	    printf( "Pre-roll audio info: start time = %llu, count = %d\n",
		    audioStartTime,
		    audioFramesQueued );
	}
    }

    /*
     * We purposely queued less audio than video, so at this point the
     * audioStartTime should be before the videoStartTime.  The next
     * step is to compute the number of audio samples needed to align
     * the audio and video.
     */

    audioFramesNeeded =
	( videoStartTime - audioStartTime ) * audio.frameRate / NANOS_PER_SEC;
    if ( ( audioFramesNeeded < 0 ) ||
	 ( ALgetfillable( audio.outPort ) * audio.channelCount
	   < audioFramesNeeded ) ) {
	fprintf(stderr, "Pre-roll error\n");
	fprintf(stderr, "video start time: %llu\n", videoStartTime);
	fprintf(stderr, "audio start time: %llu\n", audioStartTime);
	stopAllThreads();
    }
    
    /*
     * Remember the total numbef of audio frames for pre-roll.
     */
    
    prerollAudioFrames = 
	audioFramesNeeded + audioPrerollSamples / audio.channelCount;
    if ( options.verbose > 2 ) {
	printf( "Audio pre-roll = %d+%d\n",
	        audioFramesNeeded,
	        audioPrerollSamples / audio.channelCount );
    }

    /*
     * Now, put enough silent audio frames in the audio queue
     * so that the first real audio sample will come out at the
     * same time as the first real video field
     */

    bzero (audioBuffer, audioFramesNeeded * audio.frameSize);
    if ( ALwritesamps( audio.outPort, audioBuffer, 
		       audioFramesNeeded * audio.channelCount ) != 0) {
	ERROR( "ALwritesamps failed" );
    }
    audioFramesPlayed += audioFramesNeeded;
/*    assert( audioFramesPlayed == prerollAudioFrames );*/

    /*
     * At this point, the AL and CL queues are lined up, so all we
     * need to do is keep them full.
     */
}

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

static void playMovie (void)
{
    int syncInterval = 10;	/* How often to check sync */
    int firstSync    = 0;	/* First frame to check sync */
    int frame;
    int framesOff = 0;

    /* XXX Check the sync before doing anything. XXX */
    if ( options.verbose > 2 && playstate.audioActive ) {
	printf( "Pre-play sync check...\n" );
	if ( checkSync() != 0 ) {
	    printf( "    Sync is bad at start!\n" );
	}
	printf( "   ... done with pre-play sync check.\n" );
    }

    for ( frame = 0;  frame < image.numberFrames;  frame++ ) {

	if ( playstate.audioActive ) {
	    playAudioForFrame( frame );
	}

	if ( framesOff == 0 ) {
	    playImageForFrame( frame );
	}
	else if ( framesOff > 0 ) {
	    fprintf(stderr, "dmplay: frame %d repeated\n", frame);
	    playImageForFrame( frame );
	    playImageForFrame( frame );
	    videoFramesSkipped -= 1;
	    framesOff -= 1;
	}
	else /* framesOff < 0 */ {
	    fprintf(stderr, "dmplay: frame %d skipped\n", frame);
	    videoFramesSkipped += 1;
	    framesOff += 1;
	}

	if ( ( image.display != GRAPHICS ) && ( playstate.audioActive ) ) {
	    if ( ( frame - firstSync ) % syncInterval == 0 ) {
		framesOff = checkSync();
	    }
	}

	if ( ( image.display == GRAPHICS ) && ( GRAPHICS_DELAY <= frame ) ) {
	    graphicsDisplay();
	}
    }
    if (playstate.loopMode == LOOP_FRAME && image.display != GRAPHICS
	&& !playstate.autoStop) {
	for (;;) 
	    playImageForFrame (image.numberFrames - 1);
    }
}


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

    if ( audio.frameCount < audioEnd ) {
	return;
    }

    /*
     * Get the audio frames from the movie.
     */

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
* playImageForFrame - get one image and give it to the decompressor.
*
********/

static void playImageForFrame
    (
    int frame
    )
{
    int size;
    int preWrap;
    int wrap;
    void* freeData;

    /*
     * Get the size of this compressed image.
     */

    assert( ( 0 <= frame ) && ( frame < image.numberFrames ) );
    size = mvGetCompressedImageSize( movie.imgTrack, frame );

    if ( options.verbose > 1 )
	printf("frame %d: %d\n", frame, size);

    /*
     * Wait until there is enough space in the decompressor's input
     * buffer.
     */

    preWrap = clQueryFree( dataHdl, size, &freeData, &wrap );
    if ( preWrap < 0 ) {
	ERROR( "clQueryFree failed" );
    }

    /*
     * If there is enough space before wrap, we can read directly into
     * the compressed data buffer.
     */

    if ( size <= preWrap ) {
	if ( mvReadCompressedImage( movie.imgTrack, frame,
				    preWrap, freeData ) != DM_SUCCESS ) {
	    ERROR( "mvReadCompressedImage failed" );
	}
	if ( clUpdateHead( dataHdl, size ) < 0 ) {
	    ERROR( "clUpdateHead failed" );
	}
    }

    /*
     * If we have to wrap, read the compressed image into a temporary
     * buffer and then copy the two parts.
     */

    else /* preWrap < size */ {
	int newPreWrap;
	if ( unwrapBufferSize < size) {
	    int newSize = size * 12 / 10;  /* 20% extra space */
	    if ( unwrapBuffer != NULL ) {
		free( unwrapBuffer );
	    }
	    unwrapBuffer = malloc( newSize );
	    if ( unwrapBuffer == NULL ) {
		ERROR( "out of memory" );
	    }
	    unwrapBufferSize = newSize;
	}
	if ( mvReadCompressedImage( movie.imgTrack,
				    frame,
				    unwrapBufferSize,
				    unwrapBuffer ) != DM_SUCCESS) {
	    ERROR( "mvReadCompressedImage failed" );
	}
	bcopy (unwrapBuffer, freeData, preWrap);
	if ( clUpdateHead( dataHdl, preWrap ) < SUCCESS ) {
	    ERROR( "clUpdateHead failed" );
	}
	newPreWrap = clQueryFree( dataHdl, 0, &freeData, &wrap );
	if ( newPreWrap < SUCCESS ) {
	    ERROR( "clQueryFree failed" );
	}
	bcopy( ((char*)unwrapBuffer) + preWrap, freeData, size - preWrap );
	if ( clUpdateHead( dataHdl, size - preWrap ) < SUCCESS ) {
	    ERROR( "clUpdateHead failed" );
	}
    }
}


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
    double imageRate = image.frameRate * 2;   /* field rate */
    double audioRate = audio.frameRate;

    double video_ust1;
    double video_msc1;
    double audio_ust2;
    double audio_msc2;
    double audio_ust3;
    double audio_msc3;
    
    double sync_error;
    double framesOff;

    /*
     * Get a timestamp for the video stream: a field number and the
     * time at which it started going out.  We need to deal in fields,
     * because that is the unit that clGetNextImageInfo gives us.
     *
     * (Remember that Cosmo starts counting at 1, so we add 1 to the
     * number of pre-rolled fields to make the calculation work out.)
     */
    
    {
	CLimageInfo imageinfo;
	if ( clGetNextImageInfo( codec.Hdl, &imageinfo, sizeof(imageinfo) ) <
             SUCCESS ) {
	    ERROR( "clGetNextImageInfo failed" );
	}
	video_ust1 = imageinfo.ustime - startTime;
	video_msc1 = ((int)imageinfo.imagecount) - 
	             ( prerollVideoFrames * 2 + FIRST_FIELD ) +
		     videoFramesSkipped * 2;
    }
    
    /*
     * Get a timestamp for the audio stream.  (In the future there
     * will be a function in the audio library to do this.  In 5.2, we
     * just have to assume that we can figure it out from the current
     * time and the number of frames in the audio buffer.
     */
    
    {
	unsigned long long now;
	int filled = ALgetfilled( audio.outPort ) / audio.channelCount;
	dmGetUST( &now );
	audio_ust2 = now - startTime;
	audio_msc2 = audioFramesPlayed - filled - prerollAudioFrames;
    }
    
    /*
     * Figure out which audio sample corresponds to the start of the
     * video frame for which we have a timestamp.  Estimate the
     * timestamp for that audio frame.
     */
    
    audio_msc3 = video_msc1 * audioRate / imageRate;
    audio_ust3 = audio_ust2 + ( audio_msc3 - audio_msc2 ) * 
	         NANOS_PER_SEC / audioRate;

    /*
     * Determine the amount of sync error.  This is the amount of time
     * that the image track is ahead of the audio track.
     */
    
    sync_error = audio_ust3 - video_ust1;
    framesOff = sync_error * imageRate / 2.0 / NANOS_PER_SEC;

    /*
     * To be perfectly correct, we would return the framesOff now to
     * ensure that we are as close as possble.  But, to avoid jitter
     * when the adjustment is right at a frame boundary, we add a
     * hysteresis and let the error go a but more than half a frame to
     * either side before we correct it.
     */
    
    if ( ( -0.6 < framesOff ) && ( framesOff < 0.6 ) ) {
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
}

/********
*
* waitForDrain - done playing; wait for audio and images to finish
*
********/

static void waitForDrain (void)
{
    CLimageInfo imageinfo;

    /*
     * Tell the decompressor that we are done feeding it.
     */

    clDoneUpdatingHead(dataHdl);

    /*
     * If we're doing graphics display, show any pending frames.
     * First, let the compressor's input drain, then wait until we
     * have displayed all of the frames in the output buffer
     */

    if ( image.display == GRAPHICS ) {
	int preWrap;
	int wrap;
	void* freeData;
	for (;;) {
	    preWrap = clQueryValid( dataHdl, 0, &freeData, &wrap );
	    if ( preWrap < SUCCESS ) {
		ERROR( "clQueryValid failed" );
	    }
	    if ( preWrap + wrap == 0 ) {
		break;
	    }
	    graphicsDisplay ();
	    sginap( 1 );
	}
	for (;;) {
	    preWrap = clQueryValid( frameHdl, 0, &freeData, &wrap );
	    if ( preWrap < SUCCESS ) {
		ERROR( "clQueryValid failed" );
	    }
	    if ( preWrap + wrap == 0 ) {
		break;
	    }
	    graphicsDisplay ();
	    sginap( 1 );
	}
    }

    /*
     * If we're doing video display, we need to wait for any remaining
     * video frames to be processed.  To do this, we wait for the
     * image info for each of the frames being decompressed.
     */

    if ( image.display != GRAPHICS ) {
	int previous_imagecount;
	for (;;) {
	    /*
	     ** because the synchronization code may skip or repeat video
	     ** frames, we can't really be sure that the total number of
	     ** fields sent out of cosmo is twice the number of video frames
	     ** in the file (each frame is two fields).
	     **
	     ** so what we'll do here is wait until we've started repeating
	     ** a field ... and we'll use clGetNextImageInfo()
	     ** imageinfo.imagecount to tell us when
	     */
	    if (clGetNextImageInfo(codec.Hdl, &imageinfo, sizeof imageinfo) < 0)
		break;
	    else if (imageinfo.imagecount == previous_imagecount)
		break;
	    else {
		previous_imagecount = imageinfo.imagecount;
		sginap(10);
	    }
	}
    }

    /*
     * Wait for the audio queue to drain.
     */

    if ( playstate.audioActive ) {
	while ( ALgetfilled( audio.outPort ) > 0 ) {
	    sginap(1);
	}
    }

    if (options.verbose) {
       printf("Movie done.\n");
    }

    /*
    ** freeze the video
    */
    
    freezeVideo();
    if (image.display != GRAPHICS) {
	freezeVideo();
    }

    /*
    ** Shut down the decompressor.
    */
    
    if (clCloseDecompressor (codec.Hdl) != SUCCESS) {
	fprintf (stderr, "Error while closing the decompressor.\n");
	stopAllThreads ();
    }
}

/********
*
* freezeVideo
*
********/

static void freezeVideo ( void)
{
    VLControlValue val;
    
    if ( video.timingActive ) {
	video.timingActive = 0;
	if (vlDestroyPath(video.svr, video.timingPath) == -1) {
	    vlPerror("couldn't destroy the timing path");
	    }
    }
    
    /*
     ** take a quick nap, just a little longer than 1/30th of
     ** a second.  this will allow the video board to read out
     ** two fields before we tell it to freeze
     ** (CLK_TCK is the number of ticks per second)
     */
    
    sginap( (CLK_TCK/30) + 1 );

    val.boolVal = TRUE;
    vlSetControl(video.svr, video.path, video.drn, VL_FREEZE, &val);
    vlSetControl(video.svr, video.path, video.voutdrn, VL_FREEZE, &val);
    video.dataFrozen = 1;
}

/********
*
* graphicsDisplay - read an uncompressed image from codec and display it
*
* This function will block until a frame is ready.
*
********/

static void graphicsDisplay (void)
{
    int framesPerFrame;
    void* frameAddr;
    int actualFrames;
    int wrap;
    static int videoFramesDisplayed = 0;
    
    /*
     * Determine the number of "frames" (the number of chunks in the
     * decompressed "frame" buffer) that need to be decompressed
     * before we can display a frame (a real frame) of the movie.
     */
    
    if ( image.interlacing == DM_IMAGE_NONINTERLACED ) {
	framesPerFrame = 1;
    }
    else { 
	framesPerFrame = 2;
    }
    
    /*
     * Wait until a frame has been decompressed. 
     */
    
    actualFrames = clQueryValid( frameHdl, framesPerFrame, &frameAddr, &wrap );
    if ( actualFrames <= 0 ) {
	ERROR( "Error in clQueryValid" );
    }

    /*
     * For the case where the image is not interlaced, or for hardware
     * which does de-interlace, we just read the frame from the compressor's 
     * output ring buffer and paint it on the screen.  If there is no data 
     * there, bail out.
     */

    if (image.interlacing == DM_IMAGE_NONINTERLACED ||
        codec.hardwareInterleave) {

	lrectwrite (0, 0, image.mem_width-1,
		image.mem_height-(image.cliptop+image.clipbottom)-1, 
		  (unsigned long *)frameAddr+(image.cliptop*image.mem_width));
    }

    /*
     * For the interlaced case, we must de-interlace two fields
     * into a temporary buffer and then display the frame.
     */

    else {
	if (tempBuffer == NULL) {
	    tempBuffer = (unsigned char *)
		malloc( image.mem_width * image.mem_height * sizeof(int) );
	}
	if ( tempBuffer == NULL ) {
	    fprintf( stderr, "Out of memory" );
	    stopAllThreads();
	}
	deinterlaceImage( (unsigned char *)frameAddr, tempBuffer );
	lrectwrite (0, 0, image.mem_width-1,
		image.mem_height-(image.cliptop+image.clipbottom)-1, 
		  (unsigned long *)tempBuffer+(image.cliptop*image.mem_width));

    }

    /*
     * Release the uncompressed data in the ring buffer.
     */
    
    if ( clUpdateTail( frameHdl, framesPerFrame ) < SUCCESS ) {
	ERROR( "clUpdateTail failed." );
    }

    /*
     * All done.
     */

    if (options.verbose > 1) {
	printf( "Doing frame %d\n", videoFramesDisplayed );
    }
    videoFramesDisplayed += 1;
    videoFramesDisplayed = videoFramesDisplayed % image.numberFrames;
}

/********
*
* makePrerollFrame - make a frame of compressed black
*
* Because we are going out to video (that is the only configuration
* for which synchronization is supported), we know we need interlaced
* data.
*
* This function allocates space that must be freed by the caller.
*
********/

static int makePrerollFrame
    (
    void** returnCompressedData
    )
{
#ifndef BLACK_FRAME_PREROLL

    int size;

    size = mvGetCompressedImageSize( movie.imgTrack, 0 );
    *returnCompressedData = (char *) malloc( size );
    if (*returnCompressedData == NULL) {
	ERROR ("Out of memory");
    }

    if (mvReadCompressedImage (movie.imgTrack, 0, size, 
			     *returnCompressedData) != DM_SUCCESS ) {
	ERROR ("mvReadCompressedImage failed during preroll");
    }

    return size;

#else
    int		width;
    int		height;
    int		fieldSize;
    int		compressedBufferSize;
    void *	ibuf;
    void *	obuf;
    int		compressedSize;

    /*
     * Create a compressor.
     */

    clInit( 0 );

    /*
     * Create two uncompressed, black (all zeros) fields
     */

    width  = clGetParam (codec.Hdl, CL_IMAGE_WIDTH);
    height = clGetParam (codec.Hdl, CL_IMAGE_HEIGHT);
    fieldSize = width * height * sizeof( int );
    ibuf = calloc( 2, fieldSize );

    /*
     * Compress the two fields to create one interlaced, compressed
     * frame.
     */

    compressedBufferSize =
	clGetParam( codec.Hdl, CL_COMPRESSED_BUFFER_SIZE ) * 2;
    obuf = malloc( compressedBufferSize );
    if ( obuf == NULL ) {
	ERROR( "out of memory" );
    }
    if ( clCompress( codec.Hdl, 2, ibuf, &compressedSize, obuf ) != 2) {
	ERROR( "clCompress failed for preroll frame" );
    }

    /*
     * Clean up.
     */

    free(ibuf);
    if ( clCloseCompressor( codec.Hdl ) != SUCCESS ) {
	ERROR( "clCloseCompressor failed" );
    }

    /*
     * All done.
     */

    *returnCompressedData = obuf;
    return compressedSize;
#endif
}

/********
*
* startDecompressor - start the dempressor engine running (non-blocking)
*
* Creates frame and data buffers as needed and starts the decompressor.
*
********/

static void startDecompressor (void)
{
    /*
     * Create the compressed data buffer.
     */

    int width  = clGetParam (codec.Hdl, CL_IMAGE_WIDTH);
    int height = clGetParam (codec.Hdl, CL_IMAGE_HEIGHT);
    int xxsize = width * height * 2 *3; /*3 frames worth*/
    if (xxsize < COMPRESSEDBUFFERSIZE) xxsize= COMPRESSEDBUFFERSIZE;

    dataHdl = clCreateBuf( codec.Hdl, CL_DATA, xxsize, 1, NULL );
    if ( dataHdl == NULL ) {
	ERROR( "Unable to create compressed data buffer" );
    }

    /*
     * If we're displaying to graphics, we need an uncompressed frame
     * buffer.
     */

    if ( image.display == GRAPHICS ) {
	int frameSize = image.mem_width * image.mem_height * sizeof (int);
	frameHdl = clCreateBuf( codec.Hdl, CL_FRAME, FRAMEBUFFERSIZE, frameSize, NULL );
	if ( frameHdl == NULL ) {
	    ERROR( "Unable to create uncompressed frame buffer" );
	}
    }

    /*
     * Start the compressor and tell it where to get data and where to
     * put data.  The uncompressed data buffer passed in is NULL,
     * which means to use the buffer created above.  The same is true
     * for the uncompressed frame buffer for displaying to graphics;
     * for displaying to video, we tell the decompressor to use its
     * direct connection to the video device.
     */

    {
	void* frameBuffer;
	if ( image.display == GRAPHICS ) {
	    frameBuffer = NULL;
	}
	else {
	    frameBuffer = (void*) CL_EXTERNAL_DEVICE;
	}
	if ( clDecompress( codec.Hdl, 		  /* decompressor */
			   CL_CONTINUOUS_NONBLOCK,/* run asynchronously */
			   0, 			  /* # of frames */
			   NULL, 		  /* compressed source */
			   frameBuffer		  /* uncompressed dest */
			   ) != SUCCESS ) {
	    ERROR( "clDecompress failed" );
	}
    }
}

/********
*
* waitForFieldToDecompress
*
* Wait until the decompressor is done with a specific field.
*
* NOTE: this may hang if waiting for the last field placed in the
* compressed data buffer because of the buffering in Cosmo.
*
********/

void waitForFieldToDecompress( int fieldNum )
{
    CLimageInfo imageinfo;

    if ( options.verbose > 2 ) {
	printf( "Waiting for field %d\n", fieldNum );
    }
    for ( ;; ) {
	int status;
	status = clGetNextImageInfo( codec.Hdl, &imageinfo, sizeof imageinfo );
	if ( status == SUCCESS ) {
	    if ( options.verbose > 2 ) {
		printf( "Got image info: field = %d  time = %llu\n", 
		        imageinfo.imagecount, imageinfo.ustime );
	    }
	    if ( fieldNum <= imageinfo.imagecount ) { 
		break;
	    }
	    sginap( 1 );
	}
	else if ( status != CL_NEXT_NOT_AVAILABLE ) {
	    fprintf( stderr, "clGetNextImage failed\n" );
	    stopAllThreads();
	}
    }
    if ( options.verbose > 2 ) {
	printf( "   ... got field %d\n", imageinfo.imagecount );
    }
}
