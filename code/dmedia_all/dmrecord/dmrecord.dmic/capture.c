/**********************************************************************
*
* File: capture.c
*
* This is an example program that capture audio and video to a movie
* file. It runs on or machines with mvp/ice combination.
*
**********************************************************************/

#ifdef CACHEHACK
#include <sys/cachectl.h>
#endif

#include <dmedia/vl_mvp.h>
#include "dmrecord.h"
#include "handy.h"

#define BILLION		1000000000.0

#define FRAMETIME 100
#define FRAMETIME_30000  1001

static MVtime frameTime = FRAMETIME;

/********
*
* Global Variables
*
********/

static int abortRecording; 
                     /*set when a frame is dropped during critical recording*/

static long      totalFramesDropped = 0;
static long      totalFramesCaptured = 0;	/* Number of frames not */
					/* dropped */
static long long totalFrameSize;	/* Total compressed size of */
					/* all frames not dropped. */
    
static volatile USTMSCpair firstVideoUSTMSC; /* Timestamp of the first image field */
#define firstVideoUST firstVideoUSTMSC.ust
#define firstVideoMSC firstVideoUSTMSC.msc

MovieState movie;
AudioState audio;
VideoState video;

int videoTransferStarted = 0;
int childPid;

static DMbufferpool inpool, outpool;
static volatile int goOn = 1;

static double imageRate;
static double audioRate;
static double audioToImageRate;

/**********************************************************************
*
* Utilities
*
**********************************************************************/


static void* tempBuffer = NULL;
static int   tempBufferSize = 0;

/********
*
* GrowTempBuffer
*
********/

void GrowTempBuffer
    (
    int size
    )
{
    if ( tempBufferSize < size ) {
	if ( tempBuffer != NULL ) {
	    free( tempBuffer );
	}
	tempBuffer = malloc( size );
	if ( tempBuffer == NULL ) {
	    fprintf( stderr, "Out of memory, size=%d.", size );
	    exit( EXIT_FAILURE );
	}
        tempBufferSize = size;
    }
}

/**********************************************************************
*
* Audio Stuff
*
**********************************************************************/


#define AUDIO_WIDTH      16
#define AUDIO_QUEUE_SIZE_PER_CHANNEL 100000
#define MAX_ALLOWED_AUDIO_QUEUE_SIZE_PER_CHANNEL 80000 

/********
*
* PrintAudioLevel
*
********/

void PrintAudioLevel
    (
    void
    )
{
    int samplesInQueue;
    samplesInQueue = ALgetfilled( audio.port );
    printf( "audio queue = %d samples %.1f%% full\n",
	    samplesInQueue,
	    (((float)samplesInQueue/(float)audio.channels)/(float)AUDIO_QUEUE_SIZE_PER_CHANNEL)*100);
}       

/********
*
* SetupAudio
*
* Fills in all of the audio state, except for the port, which is not
* created until StartAudio.
*
********/

void SetupAudio
    (
    void
    )
{
    double rate;

    /*
    ** Ask the audio device what its input sampling rate is.
    */
    
    {
	long pvbuffer[2];
	pvbuffer[0] = AL_INPUT_RATE;
	ALgetparams( AL_DEFAULT_DEVICE, pvbuffer, 2 );
	assert( pvbuffer[1] > 0 );
	rate = pvbuffer[1];
	if ( options.verbose ) {
	    printf( "            Audio rate: %dhz\n", (int) rate );
	}
    }   
    
    /*
    ** Set up the audio state structure.
    */
    
    audio.channels 	= options.audioChannels;
    audio.width    	= 16;	/* Always use 16-bit samples */
    audio.rate     	= rate;
    audio.port	    	= NULL;	/* Filled in by StartAudio */
    audio.startTime	= 0;	/* Filled in by StartAudio */
    audio.framesToDrop	= 0;
}

/********
*
* The audio will start flowing into the ring buffer as soon as the
* port is created.
*
********/

void StartAudio
    (
    void
    )
{
    ALconfig config;
    ALerrfunc originalErrorHandler;
    
    /*
    ** Set up the configuration for the port.
    ** The audio library measures sample width in bytes rather than bits.
    */
    
    assert( audio.width % 8 == 0 );
    ANC( config = ALnewconfig() );
    AC(ALsetqueuesize(config, AUDIO_QUEUE_SIZE_PER_CHANNEL* audio.channels));
    AC( ALsetwidth( config, AUDIO_WIDTH / 8 ) );
    AC( ALsetchannels( config, audio.channels ) );
    AC( ALsetsampfmt( config, AL_SAMPFMT_TWOSCOMP ) );
    
    /*
    ** Open the port.
    */
    
    ANC( audio.port = ALopenport( "dmrecord", "r", config ) );

    /*
    ** Get the timestamp of the first audio frame. 
    */
    
    { 
	long long audioFrameNumber;
        long long firstAudioFrameNumber;
	long long audioFrameTime;
        int doesMSCUST = 1;     /* Assume we can do it, until we try and
                                 * find out otherwise! */
	while ( ALgetfilled( audio.port ) == 0 ) {
            sginap( 1 );
	}

	originalErrorHandler = ALseterrorhandler(0);
        if ( ALgetframetime( audio.port, 
			    (unsigned long long*) &audioFrameNumber,
			    (unsigned long long*) &audioFrameTime ) < 0 ) {
          doesMSCUST = 0;
        }

	ALseterrorhandler(originalErrorHandler);
        
        
        if ( doesMSCUST && ALgetframenumber( audio.port,
                              (unsigned long long*) &firstAudioFrameNumber ) 
            < 0 ) {
          doesMSCUST = 0;
        }

        if ( doesMSCUST ) {
          /*
          ** Now we use the MSC/UST pair in order to determine a time for the
          ** next piece of data we'll actually get (audioFrameNumber
          ** is not neccessarily the first one we'll read! It's simply
          ** part of the atomic pair! ALgetframenumber's result is the
          ** next MSC we'll see!)
          */
          
          audio.startTime = 
	    audioFrameTime - 
              (long long) ( ( audioFrameNumber - firstAudioFrameNumber ) *
                           (1e+9/audio.rate) );
        } else {
          dmGetUST( (unsigned long long*) &(audio.startTime) );
          /* Use the less accurate method (the "old way") in this case. */
        }
        
      }
    
    /*
    ** Free the configuration structure.
    */

    AC( ALfreeconfig( config ) );
}

/********
*
* TooMuchAudioQueuedUp
*
********/

Boolean TooMuchAudioQueuedUp
    (
    void
    )
{
    return ( ALgetfilled( audio.port ) > 
	     MAX_ALLOWED_AUDIO_QUEUE_SIZE_PER_CHANNEL * audio.channels ); 
}

	
/********
*
* CleanupAudio
*
********/

void CleanupAudio
    (
    void
    )
{
    ALcloseport( audio.port );
}


/**********************************************************************
*
* Video Stuff
*
**********************************************************************/


/********
*
* GetVideoRate
*
* Set the video rate based on the timing of the video device.
*
********/

double GetVideoRate
    (
    void
    )
{
    VLControlValue val;

    /*
    ** Get the timing from the video device
    */
    
    VC(vlGetControl(video.server,video.path,video.source,VL_TIMING,&val));
    video.timing = val.intVal;

/*
    VC(vlGetControl(video.server, video.path, video.source, VL_RATE, &val));
    return (double)(val.fractVal.numerator) / val.fractVal.denominator;
*/

    if ( ( video.timing == VL_TIMING_625_SQ_PIX ) ||
				(video.timing == VL_TIMING_625_CCIR601) ) {
	return 25.0;
    }
    else {
	return 29.97;
    }
} /* GetVideoRate */
	

/*
 * CheckImageSize
 *
 * Sets the image size based on the image size from the video source.
 */

void CheckImageSize
    (
    void
    )
{
    VLControlValue val;
    int h;

    val.intVal = VL_PACKING_YVYU_422_8;
    VC( vlSetControl(video.server, video.path, video.drain, VL_PACKING, &val) );

    if ( DO_FRAME )
    {
	val.intVal = VL_CAPTURE_INTERLEAVED;
    }
    else
    {
	val.intVal = VL_CAPTURE_NONINTERLEAVED;
    }
    VC(vlSetControl(video.server, video.path, video.drain, VL_CAP_TYPE, &val));

    if (options.nodeKind == VL_SCREEN) {
	val.intVal = 0;
        VC(vlSetControl(video.server,video.path,video.source,VL_MVP_FULLSCREEN,&val));
	val.intVal = 1;
        VC(vlSetControl(video.server,video.path,video.source,VL_FLICKER_FILTER,&val));
	val.xyVal.x = video.originX;
	val.xyVal.y = video.originY;
	VC(vlSetControl(video.server,video.path,video.source,VL_ORIGIN,&val));
	video.width = ((video.width + 15)/16)*16;
	h = ((video.height +15)/16)*16;
    } else {
	VC(vlGetControl(video.server,video.path,video.source,VL_SIZE,&val) );
	
	/* Video input source always give frame size. The following
	** calculation is based on frame size.
	*/

	/* Truncate width to multiples of 16 */
	video.width = ((val.xyVal.x)/16)*16;
	if (options.height != 0) {
            h = options.height;
	} else {
            h = val.xyVal.y;
	}

	/* h is frame height; rounded up to multiple of 16 */
	h = ((h + 15)/16)*16;
    }

    /* Set the drain's frame size */
    val.xyVal.x = video.width;
    if ( DO_FRAME )
    {
	val.xyVal.y = h;
    }
    else
    {
	val.xyVal.y = h / 2;
    }

    
    VC( vlSetControl(video.server, video.path, video.drain, VL_SIZE, &val) );

    /* set video.height to the frame or field height */
    if (DO_FRAME) {
	video.height = h;
    } else {
	video.height = h / 2;
    }
    video.xferbytes = video.width * video.height * 2;
   
    if ( options.verbose ) {
	if (DO_FRAME) {
	printf(
	"  Image (frame) height: %d\n", video.height);
	} else {
	printf(
	"  Image (field) height: %d\n", video.height);
	}
	printf(
        "           Image width: %d\n", video.width);
    }
    return;

} /* CheckImageSize */

	
/*
 * CreateVideoPath
 */
void CreateVideoPath
    (
    void
    )
{
    int vlerr;

    /* We will take the video signal at video source node */
    video.source = vlGetNode( video.server, VL_SRC, options.nodeKind, 
			       video.videoPort );
    if (video.source < 0) {
	vlPerror("Illegal video port");
	exit( EXIT_FAILURE );
    }

    /* Send video input to DMbuffer in memeory */
    video.drain = vlGetNode(video.server, VL_DRN, VL_MEM, VL_ANY);

    /*
     Create the path.
    */
    
    VC( video.path = vlCreatePath( video.server, video.device, 
				   video.source, video.drain ) );
    vlerr = vlSetupPaths( video.server, &(video.path),
			1, VL_SHARE, VL_SHARE );
    if ( vlerr ) {
	if ( vlerr == VLStreamBusy ) 
	    fprintf(stderr, "Video device is in use by another application.\n");
	else
	    vlPerror("Unable to set up video playback path");
	exit( EXIT_FAILURE );
    }

} /* CreateVideoPath */



/*
 * SetupVideo
 */
void SetupVideo
    (
    void
    )
{
    /*
    ** Open a connection to the video server.
    */
    
/* >>> YDL TEMPORARY FIX  */
/*
    VNC( video.server = vlOpenVideo( "" ) );
*/
/* <<< YDL TEMPORARY FIX */
    
    video.videoPort = options.videoPortNum;
    video.device = UseThisDevice.dev;

    /*
     * Set up the video path to direct input video to memory,
     * which will then be delivered to dmIC encoder (compressor).
     */
    CreateVideoPath();

    /* Check the image size */
    CheckImageSize();

    /* Get the video rate */
    if (options.nodeKind == VL_VIDEO) {
	video.rate = GetVideoRate();
    }

    video.waitPerBuffer = (long) ( 1000000.0 / video.rate );
    if (DO_FIELD)
	video.waitPerBuffer /= 2;

} /* SetupVideo */





/********
*
* StartVideo
*
********/

void StartVideo
    (
    void
    )
{
    /*
    ** Start the video transfer.
    */
  
    VC( vlBeginTransfer( video.server, video.path, 0, NULL ) );
}

/********
*
* CleanupVideo
*
********/

void CleanupVideo
    (
    void
    )
{
    vlEndTransfer(video.server, video.path);
    VC( vlDestroyPath( video.server, video.path ) );
}

/**********************************************************************
*
* Compression Stuff
*
**********************************************************************/


/*
 * SetupCompression
 */
void SetupCompression
    (
    void
    )
{
    DMparams* p;

    if (dmICCreate(UseThisdmIC, &video.ic) != DM_SUCCESS) {
	fprintf(stderr,"Cannot open an appropriate real-time compressor\n");
	exit(1);
    }

    dmParamsCreate(&p);

    /* set the params for dmIC source */
    DC(dmSetImageDefaults(p,video.width,video.height,DM_IMAGE_PACKING_CbYCrY));
    dmParamsSetEnum(p,DM_IMAGE_ORIENTATION,DM_IMAGE_TOP_TO_BOTTOM);
    dmParamsSetString(p, DM_IMAGE_COMPRESSION, DM_IMAGE_UNCOMPRESSED);


    if (options.avrFrameRate) {
        if (DO_FRAME) {
            dmParamsSetEnum(p, DM_IMAGE_INTERLACING, DM_IMAGE_NONINTERLACED);
        } else if (video.rate == 25.0) {
            dmParamsSetEnum(p, DM_IMAGE_INTERLACING, DM_IMAGE_INTERLACED_EVEN);
        } else {
            dmParamsSetEnum(p, DM_IMAGE_INTERLACING, DM_IMAGE_INTERLACED_ODD);
        }
    }



    DC( dmICSetSrcParams(video.ic, p) );

    /* set the params for dmIC destionation */
    /* these are the same as for dmIC source except for DM_IMAGE_COMPRESSION */
    dmParamsSetString(p, DM_IMAGE_COMPRESSION, DM_IMAGE_JPEG);

    /* for bitrate control */
    if (options.avrFrameRate) {
        dmParamsSetFloat(p, DM_IMAGE_RATE, video.rate);
        if (DO_FRAME) {
            dmParamsSetEnum(p, DM_IMAGE_INTERLACING, DM_IMAGE_NONINTERLACED);
        } else if (video.rate == 25.0) {
            dmParamsSetEnum(p, DM_IMAGE_INTERLACING, DM_IMAGE_INTERLACED_EVEN);
        } else {
            dmParamsSetEnum(p, DM_IMAGE_INTERLACING, DM_IMAGE_INTERLACED_ODD);
        }
    }

    DC( dmICSetDstParams(video.ic, p) );
    dmParamsDestroy(p);

    /*
     * Set the conversion parameters (q-factor)
     */
    dmParamsCreate(&p);

    if (!options.avrFrameRate)
      dmParamsSetFloat(p, DM_IMAGE_QUALITY_SPATIAL, (double)options.qualityFactor * DM_IMAGE_QUALITY_MAX / 100.0);

    /* for bitrate control */
    if (options.avrFrameRate) {
    	dmParamsSetInt(p, DM_IMAGE_BITRATE,options.avrFrameRate);
    }

    DC( dmICSetConvParams(video.ic, p) );
    dmParamsDestroy(p);

    /* Create buffer pool for input to dmIC */
    /* and tell VL to use this for its output */
    dmParamsCreate(&p);
    DC( dmBufferSetPoolDefaults(p,options.inbufs,video.xferbytes,DM_FALSE,DM_FALSE) );
    DC( dmICGetSrcPoolParams(video.ic, p) );
    DC( vlDMGetParams(video.server, video.path, video.drain, p) );
    DC( dmBufferCreatePool(p, &inpool) );
    VC( vlDMPoolRegister(video.server, video.path, video.drain, inpool) );

    dmParamsDestroy(p);

    /* Create buffer pool for dmIC compressor output */
    dmParamsCreate(&p);

/*
    DC(dmBufferSetPoolDefaults(p,options.outbufs,video.xferbytes,DM_FALSE,DM_TRUE));
*/
    DC(dmBufferSetPoolDefaults(p,options.outbufs,video.xferbytes,DM_TRUE,DM_TRUE));

    DC( dmICGetDstPoolParams(video.ic, p) );
    DC( dmBufferCreatePool(p, &outpool) );

    /* tell dmIC to use this for its output */
    DC( dmICSetDstPool(video.ic, outpool) );

    dmParamsDestroy(p);

    /* get file descriptor for the output from dmIC */
    video.icfd = dmICGetDstQueueFD(video.ic);

} /* SetupCompression */

/*
** ProcessEvent
*/
static void ProcessEvent()
{
    VLEvent ev;

    if (vlCheckEvent(video.server, VLAllEventsMask, &ev) == -1) return;

    switch (ev.reason)
    {
        case VLTransferFailed:
            fprintf(stderr,"Transfer failed.\n");
            break;

        default:
            break;
    }

}

/*
** InputToCompressor
*/
static void InputToCompressor(void* xp)
{
    int vidfd, maxfd, bufferfd;
    /* pid_t ppid; */ /* Parent's pid */
    int bufferNum = 0;
    DMbuffer buf;
    unsigned long long lastmsc, adjust;
    unsigned long long newmsc;
    USTMSCpair ustmsc;
    int maxcaptures;

    fd_set readfds, t_readfds, exceptfds, t_exceptfds;

#if 0 /* it looks like this is never used.  */
    /* Get parent's process id */
    ppid = getppid();
#endif
   
    /* Receive SIGHUP and quit if parent terminates */
    sigset(SIGHUP, SIG_DFL);
    sigset(SIGINT, SIG_IGN);
    (void)prctl(PR_TERMCHILD);

#define MASK (VLTransferCompleteMask|VLTransferFailedMask|VLSequenceLostMask)
    /* Set the events we will be looking out for */
    VC( vlSelectEvents(video.server, video.path, MASK) );

    bufferfd = vlNodeGetFd(video.server, video.path, video.drain);
    vidfd = vlGetFD(video.server);
    assert(bufferfd > 0 && vidfd > 0);

    if (vidfd > bufferfd) maxfd = vidfd + 1;
    else maxfd = bufferfd + 1;

    maxcaptures = video.frames;

    if (DO_FIELD) {
        if (maxcaptures <= (INT_MAX/2)) {
            maxcaptures *= 2;
        } else {
            maxcaptures = INT_MAX;
        }
    }

    FD_ZERO(&readfds);
    FD_SET(vidfd, &readfds);
    FD_SET(bufferfd, &readfds);
    FD_ZERO(&exceptfds);
    FD_SET(vidfd, &exceptfds);
    FD_SET(bufferfd, &exceptfds);

    while (bufferNum <= maxcaptures) {

        t_readfds = readfds;
        t_exceptfds = exceptfds;

        if (select(maxfd, &t_readfds, NULL, &t_exceptfds, NULL) == -1) {
            continue;
        }

	if (FD_ISSET(bufferfd, &t_exceptfds)) {
            fprintf(stderr,"Exception condition on ring buffer descriptor\n");
            break;
        }
        else if (FD_ISSET(vidfd, &t_exceptfds)) {
            fprintf(stderr, "Exceptional condition on VL connection\n");
            break;
        }

        if (FD_ISSET(vidfd, &t_readfds)) {
            ProcessEvent();
        }

	if (!FD_ISSET(bufferfd, &t_readfds)) {
	    continue;
	}

        VC( vlDMBufferGetValid(video.server, video.path, video.drain, &buf) );

        ustmsc = dmBufferGetUSTMSCpair(buf);
        newmsc = ustmsc.msc;

        if (bufferNum == 0) {

            /* The first buffer must be even numbered if F1 dominant,
             * odd if F2 dominant */
            if ((newmsc % 2) == 1) {
                    if (video.fdominance == 1)
                        continue;
            } else {
                    if (video.fdominance == 2)
                        continue;
            }
            /* store the timestamp of the very first buffer to be used in
	       InitialSync */
            firstVideoUSTMSC = ustmsc;
            adjust = newmsc - 1;
            lastmsc = newmsc - 1;
        }

        if (options.verbose && (newmsc != (lastmsc+1))) {
            fprintf(stderr, "\nExpecting buffer %llu from video, got buffer %llu instead\n\n", lastmsc+1-adjust, newmsc-adjust);
        }

        lastmsc = newmsc;

        /* renumber the msc's as 1, 2, 3, ... */
        bufferNum = ustmsc.msc = newmsc - adjust;
	dmGetUST((unsigned long long *)&ustmsc.ust);
        dmBufferSetUSTMSCpair(buf, ustmsc);

        /* give the buffer to the compressor */
        while (dmICSend(video.ic, buf, 0,(DMbuffer *)NULL) != DM_SUCCESS) {
                sginap(1);
        }

        dmBufferFree(buf);

    }
    sleep(10);
    /* Stop video transfer */
    vlEndTransfer(video.server, video.path);

} /* InputToCompressor */


/********
*
* StartCompression
*
********/

void StartCompression
	(
    void
	)
{
    /* Give the child whole access to our data space */
    unsigned inh = PR_SADDR;

    /* Spin off a thread to feed video input to compressor */
    if ((childPid = sproc(InputToCompressor, inh, video)) == -1) {
	perror("sproc() in Capture():");
	exit(1);
    }
} /* StartCompression */




/*
 * CleanupCompression
 */
void CleanupCompression
    (
    void
    )
{
    /* Terminate the child process */
    kill(childPid, SIGHUP);
    /* Close the compressor */
    dmICDestroy(video.ic);
} /* CleanupCompression */



/*
 * GetFirstVideoUST
 */
long long GetFirstVideoUST
    (
    void
    )
{
    while (firstVideoUST == 0)
	sginap(1);

    if (firstVideoUST == -1) {
	fprintf(stderr, "Failed to capture the very first video field\n");
	exit(1);
    }

    return firstVideoUST;
} /* GetFirstVideoUST */



/**********************************************************************
*
* Movie Stuff
*
**********************************************************************/


/********
*
* GetVideoAspect
*
********/

float GetVideoAspect
    (
    void
    )
{
    const char* timingName;
    float aspect;
    
    switch( video.timing )
    {
	case VL_TIMING_525_CCIR601:
	    timingName = "VL_TIMING_525_CCIR601";
	    aspect = 11.0 / 10.0;
	    break;
		    
	case VL_TIMING_625_CCIR601:
	    timingName = "VL_TIMING_625_CCIR601";
	    aspect = 54.0 / 59.0;
	    break;
		    
	case VL_TIMING_525_SQ_PIX:
	    timingName = "VL_TIMING_525_SQ_PIX";
	    aspect = 1.0;
	    break;
		    
	case VL_TIMING_625_SQ_PIX:
	    timingName = "VL_TIMING_625_SQ_PIX";
	    aspect = 1.0;
	    break;

	default:
	    assert( DM_FALSE );
	    timingName = "unknown";
	    aspect = 1.0;
	    break;
    }

    if ( options.verbose )
    {
	printf( "video timing = %s\n", timingName );
	printf( "DM_IMAGE_PIXEL_ASPECT = %f\n", aspect );
    }

    return aspect;
}

/********
*
* SetupMovie
*
********/

void SetupMovie
    (
    void
    )
{
    /*
    ** Create the movie.
    */
    
    {
	DMparams* movieSettings;
	DC( dmParamsCreate( &movieSettings ) );
	MC( mvSetMovieDefaults( movieSettings, movie.format ) );
	if ( options.movieTitle != NULL ) {
	    DC( dmParamsSetString( movieSettings, MV_TITLE, 
				   options.movieTitle ) );
	}
	MC( mvCreateFile( options.fileName, movieSettings, NULL, 
			  &(movie.movie) ) );
	dmParamsDestroy( movieSettings );
    }
    
    /*
    ** Create an image track in the movie.
    */
    
    {
  	int frameHeight;
	DMparams* imageSettings;

	if (DO_FRAME) {
	    frameHeight = video.height;
	} else {
	    frameHeight = video.height * 2;
	}
	DC( dmParamsCreate( &imageSettings ) );
	DC( dmSetImageDefaults( imageSettings, 
			        options.halfx? video.width/2:video.width,
				options.halfy? frameHeight/2: frameHeight,
			        DM_PACKING_RGBX ) );
	DC( dmParamsSetString( imageSettings, DM_IMAGE_COMPRESSION, 
			       DM_IMAGE_JPEG ) );
	DC( dmParamsSetFloat( imageSettings, DM_IMAGE_RATE, video.rate ) );
	DC( dmParamsSetEnum( imageSettings, DM_IMAGE_ORIENTATION, 
			     DM_TOP_TO_BOTTOM ) );
	DC( dmParamsSetFloat( imageSettings, DM_IMAGE_QUALITY_SPATIAL,
		 (double)options.qualityFactor/100.0));
	if (options.nodeKind == VL_VIDEO) {
	    DC( dmParamsSetFloat( imageSettings,
				  DM_IMAGE_PIXEL_ASPECT, 
				  GetVideoAspect() ) );
	}
	if (DO_FRAME) {
	    DC( dmParamsSetEnum( imageSettings, DM_IMAGE_INTERLACING,
				 DM_IMAGE_NONINTERLACED));
	} else if (video.rate == 25.0) {
	    DC( dmParamsSetEnum( imageSettings, DM_IMAGE_INTERLACING,
				DM_IMAGE_INTERLACED_EVEN ) );
	} else {
	    DC( dmParamsSetEnum( imageSettings, DM_IMAGE_INTERLACING,
				DM_IMAGE_INTERLACED_ODD ) );
	}

	MC( mvAddTrack( movie.movie, DM_IMAGE, imageSettings, NULL, 
		        &(movie.imageTrack) ) );

	if (NEWFORMAT) {
            if (options.use30000)
            {
	        movie.timeScale = 30000;
                frameTime = FRAMETIME_30000;
            }
            else
	        movie.timeScale = (MVtimescale)(video.rate*frameTime+0.5); 
	    MC(mvSetTrackTimeScale(movie.imageTrack, movie.timeScale));
	}

	dmParamsDestroy( imageSettings );
    }
    
    /*
    ** Create an audio track in the movie.
    */
    
    if ( options.audio ) {
	DMparams* audioSettings;
	DC( dmParamsCreate( &audioSettings ) );
	DC( dmSetAudioDefaults( audioSettings, audio.width, 
			        audio.rate, audio.channels ) );
	MC( mvAddTrack( movie.movie, DM_AUDIO, audioSettings, NULL, 
		        &(movie.audioTrack) ) );
	movie.audioFrameSize = dmAudioFrameSize( audioSettings );
	dmParamsDestroy( audioSettings );
    }
    else {
	movie.audioTrack = 0;
	movie.audioFrameSize = 0;
    }
    
    /*
    ** Finish filling in the structure.
    */
    
    movie.lastOddFieldSize = 0;
    movie.lastEvenFieldSize = 0;
} /* SetupMovie */

/********
*
* CleanupMovie
*
********/

void CleanupMovie
    (
    void
    )
{
    if ((mvClose( movie.movie ) != DM_SUCCESS) || (abortRecording)) 
    {
            if (!abortRecording) {
	        fprintf( stderr, "Unable to complete movie -- %s\n",
	    	    mvGetErrorStr(mvGetErrno()) );
            }
	    unlink( options.fileName );
    }
}

/**********************************************************************
*
* Capturing
*
**********************************************************************/

/********
*
* CaptureAudioFrame
*
********/

int CaptureAudioFrame
    (
    int         frame
    )
{
    int audioStart;
    int audioEnd;
    int audioFrameCount;
    int audioSampleCount;
    /*
    ** Determine which audio frames correspond to this image frame
    ** number.  This is the same comptation done by mvMapBetweenTracks.
    ** We can't use mvMapBetweenTracks because it will not return frame
    ** numbers beyond the current length of the track.
    **
    ** We can't just figure this out once because there may not be
    ** an integral number of audio frames per image frame. In this 
    ** case, the number of audio frames will vary by 1.
    */

    audioStart = (int) ( ( frame    ) * audioToImageRate);
    audioEnd   = (int) ( ( frame + 1) * audioToImageRate);
    audioFrameCount  = audioEnd - audioStart;
    audioSampleCount = audioFrameCount * audio.channels;

    /*
    ** Syncronization at the beginning of the movie: dropping samples.
    ** If we need to drop samples, we simply read them from the audio
    ** library before proceeding to read the samples that we really
    ** want.
    */
    
    if ( audio.framesToDrop > 0 ) {
	int samplesToDrop = audio.framesToDrop * audio.channels;
	GrowTempBuffer( audio.framesToDrop * movie.audioFrameSize );
	ALreadsamps( audio.port, tempBuffer, samplesToDrop );
	if ( options.verbose > 2 ) {
	    printf( "Skipped %d audio frames for syncronization.\n", 
		    audio.framesToDrop );
	}
	audio.framesToDrop = 0;
    }
    
    /*
    ** Syncronization at the beginning of the movie: inserting samples.
    ** If we need to insert samples it gets a little messier.  What we
    ** do is write the samples to the movie file and then reduce the
    ** number that will be read below.
    */
    
    if ( audio.framesToDrop < 0 ) {
	int framesToInsert = - audio.framesToDrop;
	if ( framesToInsert > audioFrameCount ) {
	    framesToInsert = audioFrameCount;
	}
	GrowTempBuffer( framesToInsert * movie.audioFrameSize );
	bzero( tempBuffer, framesToInsert * movie.audioFrameSize );
	MC( mvInsertFrames( movie.audioTrack, audioStart, framesToInsert,
			    tempBufferSize, tempBuffer ) );
	if ( options.verbose > 2 ) {
	    printf( "Inserted %d silent audio frames for syncronization.\n", 
		    framesToInsert );
	}
	audioFrameCount     -= framesToInsert;
	audioStart          += framesToInsert;
	audio.framesToDrop += framesToInsert;
	if ( audioFrameCount <= 0 ) {
	    return SUCCESS;
	}
    }
	
    /*
    ** Make sure that the audio buffer is not full.  If it is, we are
    ** losing samples.  The code in main that handles dropped video frames
    ** is supposed to ensure that the audio buffer never overflows.
    ** XX
    */
    
    if ( TooMuchAudioQueuedUp() ) {
        PrintAudioLevel();
	fprintf(stderr, "Audio buffer overflow (can't write movie file fast enough).\n");
	return FAILURE;
    }
    
    /*
    ** Read the audio frames.
    */
    
    GrowTempBuffer( audioFrameCount * movie.audioFrameSize );
    AC( ALreadsamps( audio.port, tempBuffer, audioSampleCount ) );

    /*
    ** Write the audio to the movie.
    */
  if (!options.nowrite) {
    if ( mvInsertFrames( movie.audioTrack, audioStart, audioFrameCount,
			 tempBufferSize, tempBuffer ) != DM_SUCCESS ) {
	fprintf( stderr, "Movie file is full\n" );
	return FAILURE;
    }
  }
  else {sginap(0);} /* really want about 0.07 ticks, i.e., 0.7 milliseconds */

    /*
    ** Tracing of audio data.
    */
    
    if ( options.verbose > 1 ) {
        printf("audio frames = %u, ", audioFrameCount );
    }

    /*
    ** All done.
    */
    
    return SUCCESS;
} /* CaptureAudioFrame */
		      

/********
*
* DroppedVideoFrame
*
********/

void DroppedVideoFrame
    (
    int		frameIndex
    )
{
    struct dummyFrame_t dummyFrame;
    
     /*
     ** Write a dummy video frame to the movie, which will be replaced
     ** later with a copy of the previous video frame.
     */
      
    dummyFrame.field1size = movie.lastOddFieldSize;
    dummyFrame.field2size = movie.lastEvenFieldSize;
    
    if (NEWFORMAT) {
	MC(mvInsertTrackDataFields(movie.imageTrack, (MVtime)(frameIndex*frameTime),
	     (MVtime)frameTime, movie.timeScale, sizeof(dummyFrame.field1size),
	     &(dummyFrame.field1size), sizeof(dummyFrame.field2size),
	     &(dummyFrame.field2size), MV_FRAMETYPE_KEY, 0));
    } else {
	MC( mvInsertCompressedImage( movie.imageTrack,
	     frameIndex, sizeof( dummyFrame ), &dummyFrame ) );
    }

    totalFramesDropped += 1;
} /* DroppedVideoFrame */
	




/********
*
* getdmICOutput
*
* num is the buffer number the caller is expecting,
* which is 1,2,3,...
*
********/

DMstatus getdmICOutput
    (
    int		num,
    DMbuffer	*dmbuf
    )
{
    fd_set fdset;
    int maxfd = video.icfd + 1;
    USTMSCpair ustmsc;
    static DMbuffer buffer = NULL;
    static int bufferNum = 0;
    struct timeval tout;


again:
    if (buffer == NULL) {
	/* We don't have any image field that we have already received
	 * from the compressor and not yet processed.
	 * Get a new field from the compressor.
	 */
	while (1) {
	    FD_ZERO(&fdset);
	    FD_SET(video.icfd, &fdset);
	    tout.tv_sec = 0;
	    tout.tv_usec = 10 * video.waitPerBuffer;
	    /* wait for output from dmIC compressor */
	    if ( select(maxfd, &fdset, NULL, NULL, &tout) <= 0) {
	        /* error or interrupted or timed-out */
		*dmbuf = NULL;
	        return DM_FAILURE;
	    }
	    if (FD_ISSET(video.icfd, &fdset)) {
		/* receive the buffer from compressor */
		DC (dmICReceive(video.ic, &buffer) );
		ustmsc = dmBufferGetUSTMSCpair(buffer);
		/* get the msc of the buffer */
		bufferNum = ustmsc.msc;
		break;
	    }
	    /* False alarm if falls throughr. Get back to waiting */
	}
    }

    if (bufferNum == num) {
	/*Got the field we are looking for */
	*dmbuf = buffer;
	buffer = NULL;
	return DM_SUCCESS;
    }
    fprintf(stderr, "\nWarning: Expecting buffer %d from compressor, got buffer %d instead\n\n", num, bufferNum);
    if (bufferNum > num) {
	/* Must have lost the buffer we are looking for.
	 * Retain the one we've got and return failure.
	 */
	*dmbuf = NULL;
	return DM_FAILURE;
    }
    /* If falls through, we've got a buffer with buffer number
     * smaller than what we are looking for.
     * Skip that buffer and get a new one.
     */
    dmBufferFree(buffer);
    buffer = NULL;
    goto again;

} /* getdmICOutput */



/********
*
* oldCaptureTwoFields
*
********/

int oldCaptureTwoFields
    (
    int		frame
    )
{
    int fldNum1 = frame + frame + 1;
    int fldNum2 = fldNum1 + 1;
    int size, fldSize1, fldSize2;
    char   *dataptr1, *dataptr2;
    DMbuffer buf1, buf2;

    /* Get the two fields for the next image frame */
    if ( (getdmICOutput(fldNum1, &buf1) != DM_SUCCESS) ||
	 (getdmICOutput(fldNum2, &buf2) != DM_SUCCESS) ) {
	if (buf1 != NULL) {
	    dmBufferFree(buf1);
	}
	if (goOn == 0)
	    return INTERRUPTED;
	if (options.critical) {
	    return FAILURE;
	} else {
	    /* Write a dummy frame, to be patched up later */
	    if (!options.nowrite) {
		DroppedVideoFrame(frame );
	    } else {
		totalFramesDropped++;
	    }
	    printf( "Dropped frame %d\n", frame );
	    return SUCCESS;
	}
    }
    if (options.nowrite) {
	dmBufferFree(buf1);
	dmBufferFree(buf2);
	totalFramesCaptured++;
	return SUCCESS;
    }

    /* Get the sizes of the fields and the sum */
    fldSize1 = dmBufferGetSize(buf1);
    fldSize2 = dmBufferGetSize(buf2);
    size = fldSize1 + fldSize2;

    /* Find the addresses of the actual data */
    dataptr1 = dmBufferMapData(buf1);
    dataptr2 = dmBufferMapData(buf2);


    /* Copy the data into one contiguous area */
    GrowTempBuffer( size );
    bcopy( dataptr1, tempBuffer, fldSize1);
    bcopy( dataptr2, (char *)tempBuffer + fldSize1, fldSize2);

#ifdef CACHEHACK
    cacheflush(dataptr1, fldSize1, DCACHE);
    cacheflush(dataptr2, fldSize2, DCACHE);
#endif

    /* Release the dmBuffers */
    dmBufferFree(buf1);
    dmBufferFree(buf2);

    /*
    ** Write the compressed data to the movie file.
    */
    
    if(mvInsertCompressedImage(movie.imageTrack,frame,size,(void *)tempBuffer) != DM_SUCCESS) {
	fprintf(stderr, "Unable to write movie -- %s\n", mvGetErrorStr(mvGetErrno()) );
	return FAILURE;
    }
    movie.lastOddFieldSize  = fldSize1;
    movie.lastEvenFieldSize = fldSize2;
    
    /*
    ** Statistics
    */
    if ( options.verbose > 1 ) {
	printf( "video frame size = %d\n", size);
    }
    
    totalFramesCaptured++;
    totalFrameSize      += size;
    
    return SUCCESS;
} /* oldCaptureTwoFields */
    



/********
*
* CaptureTwoFields
*
* Use the new movie library functions which enables two fields of
* a frame to be written and retrieved as individual chunks. This
* will save us the trouble of putting the two fields together
* when writing and the trouble of separating the two fields when
* reading.
********/

int CaptureTwoFields
    (
    int		frame
    )
{
    int fldNum1 = frame + frame + 1;
    int fldNum2 = fldNum1 + 1;
    int size;
    DMbuffer buf1, buf2;


    /* Get the two fields for the next image frame */
    if ( (getdmICOutput(fldNum1, &buf1) != DM_SUCCESS) ||
	 (getdmICOutput(fldNum2, &buf2) != DM_SUCCESS) ) {
	if (buf1 != NULL) {
	    dmBufferFree(buf1);
	}
	if (goOn == 0)
	    return INTERRUPTED;
	if (options.critical) {
	    return FAILURE;
	} else {
	    /* Write a dummy frame, to be patched up later */
	    if (!options.nowrite) {
		DroppedVideoFrame(frame );
	    } else {
		totalFramesDropped++;
		sginap(1); /* wants about 1.16 ticks, i.e., 11.6 milliseconds */
	    }
	    if (options.verbose) printf( "Dropped frame %d\n", frame );
	    return SUCCESS;
	}
    }
    if (options.nowrite) {
	dmBufferFree(buf1);
	dmBufferFree(buf2);
	totalFramesCaptured++;
	sginap(1);
	return SUCCESS;
    }

    /* Get the sizes of the fields and the sum */
    movie.lastOddFieldSize = dmBufferGetSize(buf1);
    movie.lastEvenFieldSize = dmBufferGetSize(buf2);
    size = movie.lastOddFieldSize + movie.lastEvenFieldSize;

    /*
    ** Write the two compressed fields to the movie file.
    */
    MC(mvInsertTrackDataFields(movie.imageTrack,(MVtime)(frame*frameTime),(MVtime)frameTime,
	 movie.timeScale, movie.lastOddFieldSize, dmBufferMapData(buf1),
	 movie.lastEvenFieldSize, dmBufferMapData(buf2), MV_FRAMETYPE_KEY, 0));
    
#ifdef CACHEHACK
    cacheflush(dmBufferMapData(buf1), movie.lastOddFieldSize, DCACHE);
    cacheflush(dmBufferMapData(buf2), movie.lastEvenFieldSize, DCACHE);
#endif

    dmBufferFree(buf1);
    dmBufferFree(buf2);

    /*
    ** Statistics
    */
    if ( options.verbose > 1 ) {
	printf( "video frame size = %d\n", size);
    }
    
    totalFramesCaptured++;
    totalFrameSize      += size;
    
    return SUCCESS;
} /* CaptureTwoFields */
    



/********
*
* CaptureOneFrame
*
* Capturing each frame as a single buffer rather than two
* separate fields.
********/

int CaptureOneFrame
    (
    int		frame
    )
{
    int framesize;
    char   *dataptr;
    DMbuffer buf;

    if (getdmICOutput(frame+1, &buf) != DM_SUCCESS) {
	if (goOn == 0)
	    return INTERRUPTED;
	if (options.critical) {
	    return FAILURE;
	} else {
	    /* Write a dummy frame, to be patched up later */
	    if (!options.nowrite) {
		DroppedVideoFrame(frame );
	    } else {
		totalFramesDropped++;
	    }
	    printf( "Dropped frame %d\n", frame );
	    return SUCCESS;
	}
    }
    if (options.nowrite) {
	dmBufferFree(buf);
	totalFramesCaptured++;
	return SUCCESS;
    }

    /* Find location and size of the compressed frame */
    dataptr = dmBufferMapData(buf);
    framesize = dmBufferGetSize(buf);


#ifdef CACHEHACK
    cacheflush(dataptr, framesize, DCACHE);
#endif

    /*
    ** Write the compressed data to the movie file.
    */
    
    if( mvInsertCompressedImage( movie.imageTrack, frame, framesize, dataptr) != DM_SUCCESS ) {
	fprintf( stderr, "Unable to write movie -- %s\n", mvGetErrorStr(mvGetErrno()) );
	return FAILURE;
    }

    dmBufferFree(buf);

    movie.lastOddFieldSize  = framesize;
    movie.lastEvenFieldSize = 0;
    
    /*
    ** Statistics
    */
    if ( options.verbose > 1 ) {
	printf( "video frame size = %d\n", framesize);
    }
    
    totalFramesCaptured++;
    totalFrameSize      += framesize;
    
    return SUCCESS;
} /* CaptureOneFrame */
    




/********
*
* oldReplicateVideoFrames
*
* This function scans the movie that has been produced for frames that
* were not recorded.  (They have a size == sizeof(dummyFrame)).  To
* create the frames that are filled in, the second field of the last
* valid frame is replicated twice to produce a frame.
*
* Just copying the previous frame would cause jitter in the image.
* But if we are doing per-frame capturing (as opposed to per-field),
* then that's what we will do anyway.
*
* This function can fail if there is not enough disk space to store the
* insterted images.
*
********/

DMstatus oldReplicateVideoFrames
    (
    void
    )
{
    MVid imageTrack = movie.imageTrack;
    int trackLength;	/* Number of frames in the image track. */
    int frameIndex;	/* current frame index */
    int framesDropped = 0;
    int size;               /* size of last good frame         */
    int	newSize;            /* size of field2 repeated frame   */
    char *origBuffer;       /* dynamicaly sized buffer         */
    int origBufferSize = 0;
    char *newBuffer;        /* dynamicaly sized buffer         */
    int newBufferSize = 0;
    int lastReplicated = -1; /* keep track of consecutive drops */
    struct dummyFrame_t dummyFrame;

    trackLength = mvGetTrackLength( imageTrack );
    for ( frameIndex = 0;  frameIndex < trackLength;  frameIndex++ ) {
        if ( mvGetCompressedImageSize( imageTrack, frameIndex ) 
                    == sizeof(struct dummyFrame_t) ) {
	    if (frameIndex <= 1)
		return DM_FAILURE;
	    if( lastReplicated == ( frameIndex - 1 ) ) {
	        /* this is easy, just insert the already built frame */
	        MC( mvDeleteFrames( imageTrack, frameIndex, 1) );
	        if ( mvInsertCompressedImage( imageTrack, frameIndex, 
		     newSize, newBuffer)  != DM_SUCCESS ) {
		    return DM_FAILURE;
		}
	    }
	    else { /* this is a new drop */
                MC(mvReadCompressedImage( imageTrack, frameIndex,
                    sizeof ( dummyFrame ), &dummyFrame) );
                                           /* size of newBuffer with 2 fields*/
                /* get the original good frame */
                size = mvGetCompressedImageSize( imageTrack, frameIndex - 1 );
                if ( origBufferSize < size ) { 
                                          /* dynamicaly size buffer as needed */
                    if ( origBufferSize )
                        free ( origBuffer );
                    origBuffer = malloc(size);
                    origBufferSize = size;
                }

                MC(mvReadCompressedImage( imageTrack, frameIndex-1, 
                                                         size, origBuffer) );
		if (DO_FRAME) {
		    newBuffer = origBuffer;
		    newSize = size;
		    MC(mvDeleteFrames( imageTrack, frameIndex, 1) );
		    if ( mvInsertCompressedImage( imageTrack, frameIndex,
			 newSize, newBuffer) != DM_SUCCESS ) {
			return DM_FAILURE;
		    }
		    continue;
		}

                newSize = 2*dummyFrame.field2size;
    
                /* make sure the marker data corresponds to the same 
                                                          last good frame */
                assert(size==(dummyFrame.field1size + dummyFrame.field2size));
    
                /* create a new buffer w/field2 duplicated */
                if(newBufferSize < newSize) { /* dynamicaly size buffer */
                    if(newBufferSize)
                	free(newBuffer);
                    newBuffer= malloc(newSize);
                    newBufferSize= newSize;
                }

                /* copy field2 into field1 of new buffer*/
                bcopy(origBuffer + dummyFrame.field1size, /*from beg of field2*/
                    newBuffer,                            /*to new field1     */
                    dummyFrame.field2size);               /*move all of field2*/

                /* copy field2 into field2 of new buffer */
                bcopy(origBuffer + dummyFrame.field1size, /*from beg of field2*/
                newBuffer + dummyFrame.field2size,        /*to new field2     */
                dummyFrame.field2size);                   /*move all of field2*/
    
                MC(mvDeleteFrames( imageTrack, frameIndex, 1) );
                if ( mvInsertCompressedImage( imageTrack, frameIndex,
				   newSize, newBuffer) != DM_SUCCESS ) {
		    return DM_FAILURE;
		}
            }

            lastReplicated= frameIndex;
            framesDropped++;
        }
    }
    return DM_SUCCESS;
} /* oldReplicateVideoFrames */




/********
*
* ReplicateVideoFrames
*
* This function scans the movie that has been produced for frames that
* were not recorded.  Such frames are indicated by having fields of
* length sizeof(dummyFrame.field1size).
* To create the frames that are filled in, the second field of the last
* valid frame is replicated twice to produce a frame.
*
* Just copying the previous frame would cause jitter in the image.
* But if we are doing per-frame capturing (as opposed to per-field),
* then that's what we will do anyway.
*
* This function can fail if there is not enough disk space to store the
* insterted images.
*
********/

DMstatus ReplicateVideoFrames
    (
    void
    )
{
    MVid imageTrack = movie.imageTrack;
    int trackLength;	/* Number of frames in the image track. */
    int frameIndex;	/* current frame index */
    int framesDropped = 0;
    char *Buffer1, *Buffer2;       /* dynamicaly sized buffer         */
    int Buffer1Size = 0, Buffer2Size = 0;
    int lastReplicated = -1; /* keep track of consecutive drops */
    struct dummyFrame_t dummyFrame;
    size_t f1size, f2size;
    off64_t gap;

    trackLength = mvGetTrackNumDataIndex( imageTrack );
    for ( frameIndex = 0;  frameIndex < trackLength;  frameIndex++ ) {
	MC(mvGetTrackDataFieldInfo(imageTrack, frameIndex, &f1size, &gap, &f2size));
	if ((f1size == sizeof(dummyFrame.field1size)) && (f2size == sizeof(dummyFrame.field2size))) {
	    if (frameIndex < 1)
		return DM_FAILURE;
	    if ( lastReplicated != ( frameIndex - 1 ) ) {
		/* this is a new drop; get the good frame before it */
                MC(mvReadTrackDataFields( imageTrack, frameIndex, f1size, &dummyFrame.field1size, f2size, &dummyFrame.field2size));

                /* get the original good frame */
		MC(mvGetTrackDataFieldInfo(imageTrack, frameIndex-1, &f1size, &gap, &f2size));
                /* make sure the marker data corresponds to the same 
                                                          last good frame */
		assert((f1size == dummyFrame.field1size) && (f2size == dummyFrame.field2size));
                if ( Buffer1Size < f1size ) { 
                    if ( Buffer1Size )
                        free ( Buffer1 );
                    Buffer1 = malloc(f1size);
                    Buffer1Size = f1size;
                }
                if ( Buffer2Size < f2size ) { 
                    if ( Buffer2Size )
                        free ( Buffer2 );
                    Buffer2 = malloc(f2size);
                    Buffer2Size = f2size;
                }

                MC(mvReadTrackDataFields( imageTrack, frameIndex-1, Buffer1Size, Buffer1, Buffer2Size, Buffer2));
	    }
	    MC(mvDeleteFramesAtTime( imageTrack, (MVtime)(frameIndex*frameTime), (MVtime)frameTime, movie.timeScale) );
	    MC(mvInsertTrackDataFields(imageTrack, (MVtime)(frameIndex*frameTime), (MVtime)frameTime, movie.timeScale, Buffer2Size, Buffer2, Buffer2Size, Buffer2, MV_FRAMETYPE_KEY, 0));
            lastReplicated= frameIndex;
            framesDropped++;
        }
    }
    return DM_SUCCESS;
} /* ReplicateVideoFrames */



    
/********
*
* CutFromFirstDroppedFrame
*
* This function scans the movie for the first dropped frame, and
* removes everything from that point on.  This is a last ditch effort
* to salvage the movie file when the disk gets full.
*
********/

void CutFromFirstDroppedFrame
    (
    void
    )
{
    MVid imageTrack = movie.imageTrack;
    MVid audioTrack = movie.audioTrack;
    int trackLength;	/* Number of frames in the image track. */
    int frameIndex;	/* current frame index */
    int firstDropped;
    int firstAudioToCut;

    /*
    ** Find the first dropped frame.
    */

    trackLength = mvGetTrackLength( imageTrack );
    for ( frameIndex = 0;  frameIndex < trackLength;  frameIndex++ ) {
        if ( mvGetCompressedImageSize( imageTrack, frameIndex ) 
                    == sizeof(struct dummyFrame_t) ) {
	    firstDropped = frameIndex;
	    break;
	}
    }

    /*
    ** Cut the audio track from that point.
    */

    if ( options.audio ) {
	mvMapBetweenTracks( imageTrack, audioTrack, firstDropped,
			    (MVframe*) &firstAudioToCut );
	MC( mvDeleteFrames( audioTrack, 
			    firstAudioToCut, 
			    mvGetTrackLength(audioTrack) - firstAudioToCut));
    }

    /*
    ** Cut the image track from that point.
    */

    MC( mvDeleteFrames( imageTrack, firstDropped, 
			trackLength - firstDropped ) );

    /*
    ** Let the user know what we did.
    */

    fprintf( stderr, "Removed frames %d to %d\n",
	     firstDropped, trackLength - firstDropped );
}
    
/********
*
* PrintTimingInformation
*
********/

void PrintTimingInformation
    (
    double videoRate
    )
{
    printf( "\nTiming information:\n" );
    if ( totalFramesDropped != 0 ) {
        printf( "Recording could not be done in real time.\n" );
	printf( "%d image frames were captured.\n", totalFramesCaptured );
        printf( "%d image frames replicated.\n", totalFramesDropped );
    }
    else {
	printf( "%d image frames = %.2f seconds of video captured.\n",
	        totalFramesCaptured,
                ((double)totalFramesCaptured)/ videoRate  );
    }
} /* PrintTimingInformation */

/********
*
* InitialSync
*
* Set the "framesToDrop" field in the audio state to compensate for
* the audio and video streams not starting at the same time.
*
********/

void InitialSync
    (
    void
    )
{
    long long videoStart;
    long long audioStart;
    
    /*
    ** Get the timestamp of the first video field.
    */
    
    videoStart = GetFirstVideoUST();

    /*
    ** Get the timestamp of the first audio frame.
    */
    
    audioStart = audio.startTime;
    
    /*
    ** Compute the number of frames that corresponds to this
    ** difference.  (The timestamps are measured in nanoseconds.)
    */
    
    audio.framesToDrop = 
	( videoStart - audioStart ) * ((int) audio.rate) / BILLION;
}

/********
*
* PrintStatistics
*
********/

void PrintStatistics
    (
    int width,
    int height,
    double rate
    )
{
    /*
    ** Print the number of frames that were dropped
    */
    
    if ( totalFramesDropped ) {
        fprintf(stderr, "\nWarning: failed to capture %d of %d image frames.\n",
           	totalFramesDropped, 
		totalFramesDropped + totalFramesCaptured);
    }
    
    /*
    ** Print compression ratio.
    */
    
    if ( totalFramesCaptured > 0) {
        double averageFrameSize = 
	    ((double)totalFrameSize) / totalFramesCaptured;
        double uncompressedFrameSize = width*height*2*2;
	double actualBitRate =  averageFrameSize * 8 * rate;
        printf( "\nCompression information:\n" );
        printf( "Average compressed frame size:\t %.1f bytes\n", averageFrameSize );
        printf( "Average compression ratio:\t %.1f : 1\n", uncompressedFrameSize/averageFrameSize );
	printf( "Bit rate achieved:\t\t %.1f bits/sec\n", actualBitRate);
    }
} /* PrintStatistics */

/********
*
* setscheduling
*
* if mode==1 set non-degrading priority use high priority
* else, set a benign degrading priotrity
*
* NOTE: this routine must be run as root (an err is printed otherwise)
*
********/

void setscheduling
    (
    int hiPriority
    )
{
   if(hiPriority)
      {
      if(schedctl(NDPRI, 0, NDPHIMIN) < 0) /* set non-degrade hi */
	 fprintf(stderr, "Run as root to enable real time scheduling\n");
      }
   else
      {
      if(schedctl(NDPRI, 0, 0) < 0)  /* return to normal priority */
	 fprintf(stderr, "Run as root to enable real time scheduling\n");
      }
}

/*
 * sigint
 *
 * Catches ^C as signal to stop
 */

static void sigint(void)
{
	goOn = 0;
}
/********
*
* capture
*
********/

void capture
    (
    void
    )
{
    typedef int (* ImageCaptureFNC)(int);
    ImageCaptureFNC CaptureImageFrame;
    int		framesToCapture;
    int 	frame;
    int ac, vc;

    /*
    ** Get things set up.
    **    - compression needs video info for image size
    **    - movie needs audio and video info for track formats
    */
    
    firstVideoUST = 0;
    firstVideoMSC = 0;

    if ( options.audio ) {
	SetupAudio();
    }
    SetupVideo();
    SetupCompression();
    SetupMovie();

    /*
    ** Figure out how many frames to capture.
    */
    if (options.frames != 0) {
      framesToCapture = options.frames;
    } else if ( options.seconds != 0 ) {
	framesToCapture = (int) ( options.seconds * video.rate + 0.5 );
    } else {
	framesToCapture = INT_MAX;  /* forever */
    }
    video.frames = framesToCapture;

    if (DO_FRAME) {
	CaptureImageFrame = CaptureOneFrame;
    } else if(NEWFORMAT) {
	CaptureImageFrame = CaptureTwoFields;
    } else {
	CaptureImageFrame = oldCaptureTwoFields;
    }
    /*
    ** Ready to start
    */
    
    if ( !options.batchMode) {
	printf ("\nHit the Enter key to begin recording... ");
	fflush(stdout);
	getchar();
    }

    if ( !options.seconds) {	/* record till user key stroke or <ctrl>-c. */
	printf("Hit <ctrl>-c to stop recording...\n");
    }

    /* Catch ^C as a signal to stop recording */
    sigset(SIGINT, sigint);

    /*
    ** Start the data flowing in.
    */
    
    imageRate = mvGetImageRate( movie.imageTrack );
    if ( options.audio ) {
	StartAudio();
	audioRate = mvGetAudioRate( movie.audioTrack );
	audioToImageRate = audioRate / imageRate;
    }
    StartCompression();
    StartVideo();
 
    /*
    ** Determine the audio adjustment requried to synchronize audio
    ** and video.  The audio state is updated so that when we start
    ** writing audio to the movie it will be offset correctly to line
    ** up with the video.
    */
    
    if ( options.audio ) {
	InitialSync();
    } else
	GetFirstVideoUST();
    
    /*
    ** Start recording.  Run until ^C or until we read the designated number of
    ** frames. 
    */
    
    for ( frame = 0;  (frame < framesToCapture) && goOn;  frame++ ) {
	if ( options.verbose > 1 ) {
	    printf("Frame %u: ", frame);
	}
	if ( options.audio ) {
	    ac = CaptureAudioFrame(frame);
	    if (ac == INTERRUPTED) {
		frame--;
		totalFramesCaptured--;
		break;
	    }
	    if ( ac != SUCCESS ){
		fprintf( stderr, "Error capturing audio for frame %d\n", frame);
		goto stop_early;
	    }
	}
	vc = CaptureImageFrame( frame );
	if (vc == INTERRUPTED) {
	    frame--;
	    totalFramesCaptured--;
	    break;
	}
	if ( vc != SUCCESS ){
	    fprintf( stderr, "Error capturing image frame %d\n", frame );
	    goto stop_early;
	}
    }
    kill(childPid, SIGKILL);
    assert( frame == totalFramesDropped + totalFramesCaptured );
    if ( totalFramesDropped == 0 ) {
	printf( "Recording was done in real time successfully.\n");
    }
stop_early:
    vlEndTransfer(video.server, video.path);

    /*
    ** drop back to a standard priority for the file cleanup
    */
    
    if (options.hiPriority)
	setscheduling(0);
    if ( ! abortRecording ) {
	printf( "Post-processing output file '%s' ... ", 
		basename(options.fileName));
    }
    fflush(stdout);

    /*
    ** Fill in the dropped frames by replicating the ones before them.
    ** If that fails (because of lack of disk space), remove all of the
    ** movie past the first dropped frame.
    */
    
  if (!options.nowrite) {
    if ( ! abortRecording ) {
	if ((NEWFORMAT && (ReplicateVideoFrames() != DM_SUCCESS)) ||
	     (OLDFORMAT && (oldReplicateVideoFrames() != DM_SUCCESS))) {
	    fprintf( stderr, "Could not fill in dropped frames.\n" );
	    CutFromFirstDroppedFrame();
	}
	PrintTimingInformation( video.rate );
    }
  }
    
    /*
    ** Clean up.  We have either finished normally or aborted.
    */

    if ( ! abortRecording ) {
	PrintStatistics( video.width, video.height, video.rate );
    }
    
    CleanupMovie();
    CleanupCompression();
    CleanupVideo();
    if ( options.audio ) {
    	CleanupAudio();
    }

} /* capture */
