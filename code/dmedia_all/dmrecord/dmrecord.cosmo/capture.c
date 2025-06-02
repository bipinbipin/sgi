/**********************************************************************
*
* File: cosmo_capture.c
*
* This is an example program that capture audio and video to a movie
* file using Cosmo to capture compressed JPEG data.
*
**********************************************************************/

#include <assert.h>
#include <bstring.h>	/* for bcopy */
#include <math.h>	/* for floor */
#include <stdlib.h>	/* for exit, atoi */
#include <string.h>	/* for strcmp */
#include <unistd.h>	/* for sginap */
#include <sys/statvfs.h>
#include <sys/schedctl.h>
#include <libgen.h>	/* for basename */

#include <dmedia/audio.h>
#include <dmedia/vl.h>
#ifdef	COSMO
#include <dmedia/vl_ev1.h>
#endif	/* COSMO */
#include <dmedia/cl.h>
#include <dmedia/cl_impactcomp.h>
#include <dmedia/cl_cosmo.h>
#include <dmedia/moviefile.h>
#include <dmedia/dm_image.h>
#include <dmedia/dmedia.h>

#include "dmrecord.h"
#include "handy.h"

#define BILLION		1000000000.0

/********
*
* Global Variables
*
********/

static int abortRecording; 
                     /*set when a frame is dropped during critical recording*/

static long      totalFramesDropped;
static long      totalFramesCaptured;	/* Number of frames not */
					/* dropped */
static long long totalFrameSize;	/* Total compressed size of */
					/* all frames not dropped. */
    
typedef struct dummyFrame_t{
	int field1size;
	int field2size;
	} dummyFrame_s;


/**********************************************************************
*
* Utilities
*
**********************************************************************/

/********
*
* tempBuffer
*
* This is a temporary buffer used by several of the routines in this
* module. 
*
********/

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
	    fprintf( stderr, "%s: Out of memory.", g_ProgramName );
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

typedef struct
{
    int		channels;
    int		width;
    double	rate;
    ALport	port;
    long long	startTime;	/* The time at which the first audio */
				/* frame was captured. */
    int		framesToDrop;	/* For syncronization at the beginning */
				/* of the movie.  This is the number */
				/* of audio frames to drop before */
				/* recording begins.  Can be negative */
				/* to indicate that silent frames */
				/* should be inserted. */
} AudioState;

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
    AudioState* audio
    )
{
    int samplesInQueue;
    samplesInQueue = ALgetfilled( audio->port );
    printf( "audio queue = %d samples %.1f%% full\n",
	    samplesInQueue,
	    (((float)samplesInQueue/(float)audio->channels)/(float)AUDIO_QUEUE_SIZE_PER_CHANNEL)*100);
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
    AudioState* audio,
    Options*    options
    )
{
    double rate;

    /*
    ** Ask the audio device what its input sampling rate is.
    */
    
    {
 int need_to_recompute_rate_empirically = FALSE;

  for(;;)
    {
      long buffer[6];

      /* acquire state variables of audio hardware */
      buffer[0] = AL_INPUT_RATE;
      buffer[2] = AL_INPUT_SOURCE;
      buffer[4] = AL_DIGITAL_INPUT_RATE;
      ALgetparams(AL_DEFAULT_DEVICE, buffer, 6);

      /* mic/line input and no AES slave - easy */

      if ((buffer[3] != AL_INPUT_DIGITAL) && (buffer[1] > 0))
        { rate = buffer[1]; break; }

      /* we are clocked off of AES input.  see if this AL will tell us
       * what the real rate is.  We're using ALgetdefault as an
       * "is it implemented" test to see whether our retrieved
       * value of AL_DIGITAL_INPUT_RATE is valid or not.
       */
      if (ALgetdefault(AL_DEFAULT_DEVICE, AL_DIGITAL_INPUT_RATE) < 0)
        { need_to_recompute_rate_empirically = TRUE; break; }

      if (buffer[5] > 0)
        { rate = buffer[5]; break; }
      else if (buffer[5] == AL_RATE_UNDEFINED)
        { need_to_recompute_rate_empirically = TRUE; break; }
      else if (buffer[5] == AL_RATE_NO_DIGITAL_INPUT)
        {
          /* XXX we could also flag this as an error */
          need_to_recompute_rate_empirically = TRUE;
          break;
        }
      else if (buffer[5] == AL_RATE_UNACQUIRED)
        {
          sginap(10); /* we will SPIN AROUND until rate is available */
        }
      else
        {
	 if ( options->verbose ) {
	 	fprintf(stderr,"Unexpected response from audio about rate\n");
	  }
          need_to_recompute_rate_empirically = TRUE;
          break;
        }
    }

    if (need_to_recompute_rate_empirically) {

	fprintf(stderr,"Can not determine audio rate so audio recording will not work.\n");
	exit(1);
	}





	if ( options->verbose ) {
	    printf( "            Audio rate: %dhz\n", (int) rate );
	}
    }   
    
    /*
    ** Set up the audio state structure.
    */
    
    audio->channels 	= options->audioChannels;
    audio->width    	= 16;	/* Always use 16-bit samples */
    audio->rate     	= rate;
    audio->port	    	= NULL;	/* Filled in by StartAudio */
    audio->startTime	= 0;	/* Filled in by StartAudio */
    audio->framesToDrop	= 0;
}

/********
*
* The audio will start flowing into the ring buffer as soon as the
* port is created.
*
********/

void StartAudio
    (
    AudioState* audio
    )
{
    ALconfig config;
    ALerrfunc originalErrorHandler;
    
    /*
    ** Set up the configuration for the port.
    ** The audio library measures sample width in bytes rather than bits.
    */
    
    assert( audio->width % 8 == 0 );
    ANC( config = ALnewconfig() );
    AC( ALsetqueuesize( config, AUDIO_QUEUE_SIZE_PER_CHANNEL* audio->channels ) );
    AC( ALsetwidth( config, AUDIO_WIDTH / 8 ) );
    AC( ALsetchannels( config, audio->channels ) );
    AC( ALsetsampfmt( config, AL_SAMPFMT_TWOSCOMP ) );
    
    /*
    ** Open the port.
    */
    
    ANC( audio->port = ALopenport( "dmrecord", "r", config ) );

    /*
    ** Get the timestamp of the first audio frame. 
    */
    
    { 
	long long audioFrameNumber;
        long long firstAudioFrameNumber;
	long long audioFrameTime;
        int doesMSCUST = 1;     /* Assume we can do it, until we try and
                                 * find out otherwise! */
	while ( ALgetfilled( audio->port ) == 0 ) {
            sginap( 1 );
	}

	originalErrorHandler = ALseterrorhandler(0);
        if ( ALgetframetime( audio->port, 
			    (unsigned long long*) &audioFrameNumber,
			    (unsigned long long*) &audioFrameTime ) < 0 ) {
          doesMSCUST = 0;
        }
	ALseterrorhandler(originalErrorHandler);
        
        
        if ( doesMSCUST && ALgetframenumber( audio->port,
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
          
          audio->startTime = 
	    audioFrameTime - 
              (long long) ( ( audioFrameNumber - firstAudioFrameNumber ) *
                           (1e+9/audio->rate) );
        } else {
          dmGetUST( (unsigned long long*) &(audio->startTime) );
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
    AudioState* audio
    )
{
    return ( ALgetfilled( audio->port ) > 
	     MAX_ALLOWED_AUDIO_QUEUE_SIZE_PER_CHANNEL * audio->channels ); 
}

	
/********
*
* CleanupAudio
*
********/

void CleanupAudio
    (
    AudioState* audio
    )
{
    ALcloseport( audio->port );
}


/**********************************************************************
*
* Video Stuff
*
**********************************************************************/

typedef struct
{
    VLServer	server;
    VLDev	device;
    VLPath	path;
    VLNode	source;
    VLNode	drain;
    int		width;
    int		height;
    int		real_height;
    double	rate;
} VideoState;

/********
*
* FindImpactVidDevice
*
* Finds the "impact" (Galileo or IndyVideo) video device.  Returns TRUE
* if such a device exists.
*
********/

Boolean FindVidDevice
    (
    VLServer	server,
    VLDev*	returnDevice,
    char *devname
    )
{
    int i;
    VLDevList devlist;
    
    /*
    ** Get a list of all video devices from the server.
    */
    
    VC( vlGetDeviceList( server, &devlist ) );
    
    /* Search for the requested one */
    
    for (i = 0; i < devlist.numDevices; i++) {
        if (strcmp( devlist.devices[i].name, devname) == 0) {
	    *returnDevice = devlist.devices[i].dev;
	    return TRUE;
        }
    }
    
    return FALSE;
}
	
/********
*
* CheckImageSize
*
* Sets the image size based on the image size from the video source,
* which can be overridden by the command-line argument.s
*
********/

void CheckImageSize
    (
    VideoState*	video,
    Options*	options
    )
{
    VLControlValue val;
    int w, h;
    
    /*
    ** Get the size from the source node in the video path and round
    ** it up to a multiple of the JPEG block size.  JPEG block sizes are
    ** multiples of 16 in X, and multiples of 8 in Y.
    **
    ** NOTE: the default VL_SIZE returned by VL_VIDEO nodes is frame size.
    **       the default returned by VL_CODEC nodes is field size.
    **       that is the reason 'h' below halves the VL_SIZE returned
    **       from the video node.  if it had done the vlGetControl on
    **       the VL_CODEC node, the size would not be halved.
    */
    
    VC( vlGetControl( video->server, video->path, video->source, VL_SIZE, &val ) );
    w =  (val.xyVal.x/16)*16;
    h = ((val.xyVal.y+15)/16)*8;

    /*
    ** Validate the options.
    */
    
    /*
    ** Set the width and height for the video.  (this will be used by
    ** the movie setup).
    */
    video->width = w;
    if ( options->height != 0 ) {
	video->height = options->height;
    }
    else { 
	video->height = h;
    }

    if ( video->height % 8 )
	video->height = ((video->height +7) / 8) * 8;

    if ( options->halfx && (video->width % 32))
	video->width = (video->width / 32) * 32;

    if ( options->halfy && (video->height % 16))
	video->height = ((video->height +15) / 16) * 16;

    if ( options->height == 0 ) {
	     video->real_height=video->height;
     } else {
	     video->real_height=options->height;
     } 

    /*
    ** Now that we know the size of the uncompressed data that we
    ** will actually be capturing, notify the VL_CODEC node of that
    ** expected size.
    ** (only applies to IMPACT Compression)
    */
    if (!strcmp(options->compressionEngine, "impact")) {
    	val.xyVal.x = video->width;
    	val.xyVal.y = video->height;
    	VC( vlSetControl( video->server, video->path, video->drain, VL_SIZE, &val ) );
    }
    
    /*
    ** Let the user know what we did.
    */
    
    if ( options->verbose ) {
	printf( "          Image height: %d\n"
		"           Image width: %d\n", video->real_height, video->width);
    }
} /* CheckImageSize */

/********
*
* GetVideoRate
*
* Set the video rate based on the timing of the video device.
*
********/

double GetVideoRate
    (
    VideoState* video
    )
{
    VLControlValue val;

    /*
    ** Get the timing from the video device.
    */
    
    VC( vlGetControl( video->server, video->path, VL_ANY, VL_TIMING, &val ) );

    if ( ( val.intVal == VL_TIMING_625_SQ_PIX ) ||
				(val.intVal == VL_TIMING_625_CCIR601) ) {
	return 25.0;
    }
    else {
	return 29.97;
    }
} /* GetVideoRate */
	
/********
*
* CreateVideoToCompressPath
*
********/

void CreateVideoToCompressPath
    (
    VideoState* video,
    Options* options
    )
{
    int vlerr;
    /*
    ** We will take the video signal at video source node
    */
    
    video->source = vlGetNode( video->server, VL_SRC, VL_VIDEO, 
			       options->videoPort );

    video->drain = vlGetNode( video->server, VL_DRN, options->videoDrain, 
				options->codecPort);
    /*
    ** Create the path.
    */
    
    VC( video->path = vlCreatePath( video->server, video->device, 
				   video->source, video->drain ) );
    vlerr = vlSetupPaths( video->server, &(video->path),
			1, VL_SHARE, VL_SHARE );
    if ( vlerr ) {
	if ( vlerr == VLStreamBusy ) 
	    fprintf(stderr, "Video device is in use by another application.\n");
	else
	    vlPerror("Unable to set up video playback path");
	exit( EXIT_FAILURE );
    }
}

/********
*
* SetupVideoSync
*
* Set the Galileo timing to an appropriate sync mode.
* This can only be done if there are no other paths on the board.
*
* For cosmo
* if we're capturing off of the analog port (0), then we want the
*   video board to be in slave mode
* if we're capturing off of a digital port (!= 0), then we want the
*   video board to be in internal mode
*
********/

void SetupVideoSync
    (
    VideoState* video,
    Options*    options
    )
{
    int videoPort;
    VLNode devNode;
    VLPath controlPath;
    VLControlValue val;
    int desiredSetting;

    /* 
     * impact chooses reaonable sync defaults.
     */
    if (strcmp(options->videoDevice, "ev1") != 0)
	return;
    /*
    ** Set up a dummy path that we can use to set the controls on the
    ** device. 
    */
    
    VC( devNode=vlGetNode( video->server, VL_DEVICE, 0, VL_ANY ) );
    VC( controlPath=vlCreatePath( video->server, video->device, 
				  devNode, devNode ) );
    VC( vlSetupPaths(video->server, &controlPath, 1, VL_SHARE, VL_READ_ONLY) );

    
    /*
    ** first let's see what port we're actually going to be using
    ** (if the user didn't specify on the command line)
    */
    
    videoPort = options->videoPort;
    if ( videoPort == VL_ANY ) {
	VC( vlGetControl( video->server, controlPath, devNode, 
			  VL_DEFAULT_SOURCE, &val) );
	videoPort = val.intVal;
    }

    /*
    ** Determine the desired setting and change the control on the
    ** device as needed.
    */
    
#ifdef	COSMO
    if ( videoPort == 0 && !strcmp(options->videoDevice,"ev1")) {  /* analog port */
	desiredSetting = VL_EV1_SYNC_SLAVE;
    }
    else { /* digital port */
#endif	/* COSMO */
	desiredSetting = VL_SYNC_INTERNAL;
#ifdef	COSMO
    }
#endif	/* COSMO */
    VC( vlGetControl(video->server, controlPath, devNode, VL_SYNC, &val ) );
    if ( val.intVal != desiredSetting && val.intVal != VL_SYNC_GENLOCK) {
	val.intVal = desiredSetting;
	if (vlSetControl( video->server, controlPath, devNode, VL_SYNC, &val))
	    fprintf(stderr, 
		"%s: unable to set video timing mode (video sync to slave)\n", 
                        g_ProgramName);
    }
    vlDestroyPath(video->server, controlPath);
} /* SetupVideoSync */

/********
*
* SetupVideo
*
********/

void SetupVideo
    (
    VideoState* video,
    Options*    options
    )
{
    
    /*
    ** Open a connection to the video server.
    */
    
    VNC( video->server = vlOpenVideo( "" ) );
    
    /*
    ** Find the video device.
    */
    
    if (!FindVidDevice(video->server, &video->device, options->videoDevice)) {
        fprintf( stderr,
		    "%s: Galileo/IndyVideo is not installed on this machine.\n",
		    g_ProgramName );
        exit( EXIT_FAILURE );
    }

    /*
    ** Set up the video sync and port information
    */

    SetupVideoSync( video, options );

    /*
    ** For cosmo this (nominally) simple program will not support capturing 
    ** from the second digital port.  when capturing from the analog or 
    ** digital 1 ports, timing information is sent to the cosmo board out of 
    ** the digital 2 port ... but when capturing from the digital 2 port, we
    ** cannot send the timing information (the port is already in use).
    **
    ** it is possible to have cosmo capture from the digital 2 port, but
    ** the changes necessary would be pervasive in this example application.
    ** (timing information would not be generated to the cosmo board)
    */

    if (!strcmp(options->videoDevice, "cosmo") && options->videoPort == 2) {
	fprintf(stderr, "%s: capture from digital port 2 is not supported.\n",
		g_ProgramName );
	exit( EXIT_FAILURE );
    }
    
    /*
    ** Set up the video path to deliver data to the compressor.
    */
    
    CreateVideoToCompressPath( video, options );

    /*
    ** Check the image size.
    */
    
    CheckImageSize( video, options );

    /*
    ** Get the image rate.
    */
    
    video->rate = GetVideoRate( video );
}

/********
*
* StartVideo
*
********/

void StartVideo
    (
    VideoState* video
    )
{
    /*
    ** Start the video transfer.
    */
    
    VC( vlBeginTransfer( video->server, video->path, 0, NULL ) );
}

/********
*
* CleanupVideo
*
********/

void CleanupVideo
    (
    VideoState* video
    )
{
    VC( vlDestroyPath( video->server, video->path ) );
}

/**********************************************************************
*
* Compression Stuff
*
**********************************************************************/

#define QUEUE_BUFFER_SIZE  ( 512 * 1024 )

typedef struct
{
    CLcompressorHdl	compressor;
    CLbufferHdl		dataBuffer;
    Boolean		haveImageInfo;
    CLimageInfo		imageInfo;
} CompState;


/*
* Open compressor
*/
/*ARGSUSED*/
void OpenCompression
    (
    CompState*  comp,
    VideoState* video,
    Options*    options
    )
{
    int status;
    int scheme;

    /* Open the compressor */

    scheme = clQuerySchemeFromName( CL_ALG_VIDEO, options->compressionEngine );
    if (scheme < 0) {
	fprintf(stderr,"Couldn't find scheme for name >%s<\n",
	    options->compressionEngine);
	exit(1);
    }
    
    status = clOpenCompressor( scheme, &comp->compressor );
    if ( status != SUCCESS) {
	if (status == CL_SCHEME_NOT_AVAILABLE) {
	    fprintf(stderr, "%s Compression is not installed on this machine.\n", options->compressionEngine);
	}
	else if (status == CL_SCHEME_BUSY) {
	    fprintf(stderr, "%s Compression is in use by another application.\n", options->compressionEngine);
	}
	exit(1);
    }

    if (options->codecPort == -1)
        options->codecPort = clGetParam( comp->compressor, CL_IMPACT_VIDEO_INPUT_CONTROL );
}

/********
*
* SetupCompression
*
* Open the compressor and fill in the compression state structure.
*
********/

void SetupCompression
    (
    CompState*	comp,
    VideoState* video,
    Options*	options
    )
{

    /*
    ** Set the parameters of the compressor.
    */
    
    {
	int paramBuf[100];
	int n = 0;
	paramBuf[n++] = CL_IMAGE_WIDTH;
	paramBuf[n++] = video->width;
	if (options->halfx) {
	    paramBuf[n++] = CL_INTERNAL_IMAGE_WIDTH;
	    paramBuf[n++] = video->width/2;
	}
	paramBuf[n++] = CL_IMAGE_HEIGHT;
	paramBuf[n++] = video->height;
	if (options->halfy) {
	    paramBuf[n++] = CL_INTERNAL_IMAGE_HEIGHT;
	    paramBuf[n++] = video->height/2;
	}
	paramBuf[n++] = CL_JPEG_QUALITY_FACTOR;
	paramBuf[n++] = options->qualityFactor;
	paramBuf[n++] = CL_ENABLE_IMAGEINFO;
	paramBuf[n++] = 1;

	if(options->avrFrameRate){
		paramBuf[n++] = CL_BITRATE;
		paramBuf[n++] = options->avrFrameRate;
	}

	CC( clSetParams( comp->compressor, paramBuf, n ) );
    }
    
    /*
    ** Create the ring buffer into which the compressed data will be
    ** written. 
    */

    CNC( comp->dataBuffer = 
	 clCreateBuf( comp->compressor, CL_DATA, 1, QUEUE_BUFFER_SIZE, 0 ) );

    /*
    ** Fill in the rest of the structure.
    */
    
    comp->haveImageInfo = FALSE;
    bzero( &(comp->imageInfo), sizeof(comp->imageInfo) );
}

/********
*
* StartCompression
*
********/

void StartCompression
    (
    CompState* comp
    )
{
    CC (clCompress( comp->compressor, CL_CONTINUOUS_NONBLOCK, 
		    CL_EXTERNAL_DEVICE, 0, NULL ))
}

/********
*
* CleanupCompression
*
********/

void CleanupCompression
    (
    CompState* comp
    )
{
    CC( clCloseCompressor( comp->compressor ) );
}

/********
*
* PeekImageInfo
*
* Get the image info for the next field.  PeekImageInfo will keep
* returning the info for the same field until AdvanceImageInfo is
* called.
*
********/

void PeekImageInfo
    (
    CompState* comp,
    CLimageInfo* returnInfo
    )
{
    int status; 

    if ( ! comp->haveImageInfo ) {
	status = clGetNextImageInfo( comp->compressor,
			             &(comp->imageInfo),
			             sizeof(CLimageInfo) );
	while ( status == CL_NEXT_NOT_AVAILABLE ) {
	    sginap( 1 );
	    status = clGetNextImageInfo( comp->compressor,
					 &(comp->imageInfo),
					 sizeof(CLimageInfo) );
	}
	CC( status );
	comp->haveImageInfo = TRUE;

/*
printf("imageinfo:  size %d, ust %lld, count %d, status %d\n",
comp->imageInfo.size,comp->imageInfo.ustime,comp->imageInfo.imagecount,
comp->imageInfo.status);
*/

    }
    
    bcopy( &(comp->imageInfo), returnInfo, sizeof(CLimageInfo) );
}

/********
*
* AdvanceImageInfo
*
********/

void AdvanceImageInfo
    (
    CompState* comp
    )
{
    assert( comp->haveImageInfo );
    comp->haveImageInfo = FALSE;
}
     

/**********************************************************************
*
* Movie Stuff
*
**********************************************************************/

typedef struct
{
    MVid	movie;
    MVid	imageTrack;
    MVid	audioTrack;
    int		audioFrameSize;
    int		lastOddFieldSize;
    int		lastEvenFieldSize;
} MovieState;

/********
*
* SetupMovie
*
********/

void SetupMovie
    (
    MovieState* movie,
    AudioState* audio,
    VideoState* video,
    Options*    options
    )
{
    /*
    ** Create the movie.
    */
    
    {
	DMparams* movieSettings;
	DC( dmParamsCreate( &movieSettings ) );
	MC( mvSetMovieDefaults( movieSettings, options->format ) );
	if ( options->movieTitle != NULL ) {
	    DC( dmParamsSetString( movieSettings, MV_TITLE, 
				   options->movieTitle ) );
	}
	MC( mvCreateFile( options->fileName, movieSettings, NULL, 
			  &(movie->movie) ) );
	dmParamsDestroy( movieSettings );
    }
    
    /*
    ** Create an image track in the movie.
    */
    
    {
	DMparams* imageSettings;
	DC( dmParamsCreate( &imageSettings ) );
	DC( dmSetImageDefaults( imageSettings, 
			        options->halfx? video->width/2:video->width,
				options->halfy? video->real_height:2*video->real_height, 
			        DM_PACKING_RGBX ) );
	DC( dmParamsSetString( imageSettings, DM_IMAGE_COMPRESSION, 
			       DM_IMAGE_JPEG ) );
	DC( dmParamsSetFloat( imageSettings, DM_IMAGE_RATE, video->rate ) );
	DC( dmParamsSetEnum( imageSettings, DM_IMAGE_ORIENTATION, 
			     DM_TOP_TO_BOTTOM ) );
	if ( (video->width == 768 || video->width == 768/2) ||  /* PAL */
	     (video->width == 720 &&                            /* CCIR(625) */
		(video->height == 576 || video->height == 576/2)) ) {
	    DC( dmParamsSetEnum( imageSettings, DM_IMAGE_INTERLACING, 
				 DM_IMAGE_INTERLACED_EVEN ) );
	    if (video->width==720){
	    
		DC( dmParamsSetFloat(imageSettings, DM_IMAGE_PIXEL_ASPECT,
                             ((double) 54.0 / (double) 59.0)) );

	    } else {
	        DC( dmParamsSetFloat(imageSettings, DM_IMAGE_PIXEL_ASPECT, 1.0) );
  	    }
	} else {
	    DC( dmParamsSetEnum( imageSettings, DM_IMAGE_INTERLACING, 
				 DM_IMAGE_INTERLACED_ODD ) );
	    if (video->width==720){
		DC( dmParamsSetFloat(imageSettings, DM_IMAGE_PIXEL_ASPECT,
                             ((double) 11.0 / (double) 10.0)) );

	    } else {
		DC( dmParamsSetFloat(imageSettings, DM_IMAGE_PIXEL_ASPECT, 1.0));
	    }
	}
	MC( mvAddTrack( movie->movie, DM_IMAGE, imageSettings, NULL, 
		        &(movie->imageTrack) ) );
	dmParamsDestroy( imageSettings );
    }
    
    /*
    ** Create an audio track in the movie.
    */
    
    if ( options->audio ) {
	DMparams* audioSettings;
	DC( dmParamsCreate( &audioSettings ) );
	DC( dmSetAudioDefaults( audioSettings, audio->width, 
			        audio->rate, audio->channels ) );
	MC( mvAddTrack( movie->movie, DM_AUDIO, audioSettings, NULL, 
		        &(movie->audioTrack) ) );
	movie->audioFrameSize = dmAudioFrameSize( audioSettings );
	dmParamsDestroy( audioSettings );
    }
    else {
	movie->audioTrack = 0;
	movie->audioFrameSize = 0;
    }
    
    /*
    ** Finish filling in the structure.
    */
    
    movie->lastOddFieldSize = 0;
    movie->lastEvenFieldSize = 0;
} /* SetupMovie */

/********
*
* CleanupMovie
*
********/

void CleanupMovie
    (
    MovieState* movie,
    Options* options
    )
{
    if ((mvClose( movie->movie ) != DM_SUCCESS) || (abortRecording)) 
    {
            if (!abortRecording) {
	        fprintf( stderr, "%s: unable to complete movie -- %s\n",
	    	    g_ProgramName, mvGetErrorStr(mvGetErrno()) );
            }
	    unlink( options->fileName );
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

DMstatus CaptureAudioFrame
    (
    AudioState* audio,
    MovieState* movie,
    Options*    options,
    int         frame
    )
{
    double imageRate;
    double audioRate;
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

    imageRate = mvGetImageRate( movie->imageTrack );
    audioRate = mvGetAudioRate( movie->audioTrack );
    audioStart = (int) ( ( frame    ) * audioRate / imageRate );
    audioEnd   = (int) ( ( frame + 1) * audioRate / imageRate );
    audioFrameCount  = audioEnd - audioStart;
    audioSampleCount = audioFrameCount * audio->channels;

    /*
    ** Syncronization at the beginning of the movie: dropping samples.
    ** If we need to drop samples, we simply read them from the audio
    ** library before proceeding to read the samples that we really
    ** want.
    */
    
    if ( audio->framesToDrop > 0 ) {
	int samplesToDrop = audio->framesToDrop * audio->channels;
	GrowTempBuffer( audio->framesToDrop * movie->audioFrameSize );
	ALreadsamps( audio->port, tempBuffer, samplesToDrop );
	if ( options->verbose > 2 ) {
	    printf( "Skipped %d audio frames for syncronization.\n", 
		    audio->framesToDrop );
	}
	audio->framesToDrop = 0;
    }
    
    /*
    ** Syncronization at the beginning of the movie: inserting samples.
    ** If we need to insert samples it gets a little messier.  What we
    ** do is write the samples to the movie file and then reduce the
    ** number that will be read below.
    */
    
    if ( audio->framesToDrop < 0 ) {
	int framesToInsert = - audio->framesToDrop;
	if ( framesToInsert > audioFrameCount ) {
	    framesToInsert = audioFrameCount;
	}
	GrowTempBuffer( framesToInsert * movie->audioFrameSize );
	bzero( tempBuffer, framesToInsert * movie->audioFrameSize );
	MC( mvInsertFrames( movie->audioTrack, audioStart, framesToInsert,
			    tempBufferSize, tempBuffer ) );
	if ( options->verbose > 2 ) {
	    printf( "Inserted %d silent audio frames for syncronization.\n", 
		    framesToInsert );
	}
	audioFrameCount     -= framesToInsert;
	audioStart          += framesToInsert;
	audio->framesToDrop += framesToInsert;
	if ( audioFrameCount <= 0 ) {
	    return DM_SUCCESS;
	}
    }
	
    /*
    ** Make sure that the audio buffer is not full.  If it is, we are
    ** losing samples.  The code in main that handles dropped video frames
    ** is supposed to ensure that the audio buffer never overflows.
    ** XX
    */
    
    if ( TooMuchAudioQueuedUp( audio ) ) {
        PrintAudioLevel( audio );
	fprintf(stderr, "Audio buffer overflow (can't write movie file fast enough).\n");
	return DM_FAILURE;
    }
    
    /*
    ** Read the audio frames.
    */
    
    GrowTempBuffer( audioFrameCount * movie->audioFrameSize );
    AC( ALreadsamps( audio->port, tempBuffer, audioSampleCount ) );

    /*
    ** Write the audio to the movie.
    */
    
    if ( mvInsertFrames( movie->audioTrack, audioStart, audioFrameCount,
			 tempBufferSize, tempBuffer ) != DM_SUCCESS ) {
	fprintf( stderr, "Movie file is full\n" );
	return DM_FAILURE;
    }

    /*
    ** Tracing of audio data.
    */
    
    if ( options->verbose > 1 ) {
        printf("audio frames = %u, ", audioFrameCount );
    }

    /*
    ** All done.
    */
    
    return DM_SUCCESS;
} /* CaptureAudioFrame */
		      
/********
*
* GetFieldSizes
*
* This is where the odd fields and even fields are paired.  Each frame
* in a movie has an odd/even pair (in that order).  The first field
* coming from the compressor will be number 1:
*
*        movie frame         video fields
*        -----------         ------------
*             0                 1, 2
*             1                 3, 4
*             2                 5, 6
*            etc.                etc.
*
* This function consumes the "imageinfo" from clGetNextImageInfo, but
* leaves the compressed data in the data buffer.
*
********/

DMstatus GetFieldSizes
    (
    CompState*	comp,
    int         frame,
    int*	returnOddFieldSize,
    int*	returnEvenFieldSize
    )
{
    CLimageInfo imageInfo;
    int oddField  = frame * 2 + 1;
    int evenField = frame * 2 + 2;
    
    /*
    ** Get the info for the odd field.  If the field number is wrong,
    ** we must have lost it.  In this case, we drop the even field if
    ** it was there and admit failure.  We must ensure that the
    ** compressed image stream is advanced far enough for the next
    ** time we are called.
    */
    
    PeekImageInfo( comp, &imageInfo );
    assert( oddField <= imageInfo.imagecount );
    if ( imageInfo.imagecount != oddField ) {
	if ( imageInfo.imagecount == evenField ) {
	    clUpdateTail( comp->dataBuffer, imageInfo.size );
	    AdvanceImageInfo( comp );
	}
	return DM_FAILURE;
    }
    *returnOddFieldSize = imageInfo.size;
    AdvanceImageInfo( comp );
    
    /*
    ** Get the info for the even field.
    */
    
    PeekImageInfo( comp, &imageInfo );
    assert( evenField <= imageInfo.imagecount );
    if ( imageInfo.imagecount != evenField ) {
	clUpdateTail( comp->dataBuffer, *returnOddFieldSize );
	return DM_FAILURE;
    }
    *returnEvenFieldSize = imageInfo.size;
    AdvanceImageInfo( comp );
    
    /*
    ** All done.
    */
    
    return DM_SUCCESS;
}

/********
*
* DroppedVideoFrame
*
********/

void DroppedVideoFrame
    (
    MovieState*	movie,
    int		frameIndex
    )
{
    struct dummyFrame_t dummyFrame;
    
     /*
     ** Write a dummy video field to the movie, which will be replaced
     ** later with a copy of the previous video field.
     */
      
    dummyFrame.field1size = movie->lastOddFieldSize;
    dummyFrame.field2size = movie->lastEvenFieldSize;
    
    MC( mvInsertCompressedImage( movie->imageTrack,
				 frameIndex,
				 sizeof( dummyFrame ), 
				 &dummyFrame ) );

    totalFramesDropped += 1;
} /* DroppedVideoFrame */
	
/********
*
* CaptureImageFrame
*
********/

DMstatus CaptureImageFrame
    (
    CompState*	comp,
    MovieState*	movie,
    Options*    options,
    int		frame
    )
{
    int     oddFieldSize;
    int     evenFieldSize;
    int     size;
    int     wrap;
    int     unwrappedSize;
    void*   dataPtr = NULL;
    Boolean tailNeedsUpdate;
    
    /*
    ** Get the sizes of the two fields for this frame.  If there is a
    ** problem, GetFieldSizes will flush out any junk from the compressed
    ** data buffer.
    */
    
    if ( GetFieldSizes( comp, frame, &oddFieldSize, &evenFieldSize ) != DM_SUCCESS){
	if ( options->critical ) {
	    return DM_FAILURE;
	}
	else {
	    DroppedVideoFrame( movie, frame );
	    printf( "Dropped frame %d\n", frame );
	    return DM_SUCCESS;
	}
    }
    size = oddFieldSize + evenFieldSize;
    
    /*
    ** Ask the compressor where the data is.
    */
    
    CC( unwrappedSize = clQueryValid( comp->dataBuffer, size, &dataPtr, &wrap ) );

    /*
    ** Need to un-wrap the frame if it wrapped.  In this case, the two
    ** halves of the wrapped data are copied into a buffer so that
    ** they will be contiguous for writing to the movie.
    **/
    
    if ( unwrappedSize < size ) {
	int wrappedSize = size - unwrappedSize;
	GrowTempBuffer( size );
	bcopy( dataPtr, tempBuffer, unwrappedSize );
	CC( clUpdateTail( comp->dataBuffer, unwrappedSize ) );
	CC( clQueryValid( comp->dataBuffer, wrappedSize, &dataPtr, &wrap ) );
	assert( wrap == 0 );
	bcopy( dataPtr, ((char*)tempBuffer) + unwrappedSize, wrappedSize );
	CC( clUpdateTail( comp->dataBuffer, wrappedSize ) );
	dataPtr = tempBuffer;
	tailNeedsUpdate = FALSE;
    }
    else {
	tailNeedsUpdate = TRUE;
    }

    /*
    ** Write the compressed data to the movie file.
    */
    
    if( mvInsertCompressedImage( movie->imageTrack, frame, size, dataPtr ) 
				!= DM_SUCCESS ) {
	fprintf( stderr, "%s: unable to write movie -- %s\n",
                g_ProgramName, mvGetErrorStr(mvGetErrno()) );
	return DM_FAILURE;
    }
    movie->lastOddFieldSize  = oddFieldSize;
    movie->lastEvenFieldSize = evenFieldSize;
    
    if (options->noNewInfo == 0)
        mvSetTrackDataFieldInfo(movie->imageTrack, frame, oddFieldSize,
    		0, evenFieldSize);
    
    /*
    ** Let the CL know that we are done with the data.
    */

    if ( tailNeedsUpdate ) {
	CC( clUpdateTail( comp->dataBuffer, size ) );
    }

    /*
    ** Statistics
    */
    if ( options->verbose > 1 ) {
	printf( "video frame size = %d\n", oddFieldSize+evenFieldSize );
    }
    
    totalFramesCaptured += 1;
    totalFrameSize      += size;
    
    return DM_SUCCESS;
}
    
/********
*
* ReplicateVideoFrames
*
* This function scans the movie that has been produced for frames that
* were not recorded.  (They have a size == sizeof(dummyFrame)).  To
* create the frames that are filled in, the second field of the last
* valid frame is replicated twice to produce a frame.
*
* Just copying the previous frame would cause jitter in the image.
*
* This function can fail if there is not enough disk space to store the
* insterted images.
*
********/

DMstatus ReplicateVideoFrames
    (
    MovieState* movie,
    Options* 	options
    )
{
    MVid imageTrack = movie->imageTrack;
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
    	    assert ( frameIndex > 1 );
	    
	    if( lastReplicated == ( frameIndex - 1 ) ) {
	        /* this is easy, just insert the already built frame */
	        MC( mvDeleteFrames( imageTrack, frameIndex, 1) );
	        if ( mvInsertCompressedImage( imageTrack, frameIndex, 
			   newSize, newBuffer)  != DM_SUCCESS ) {
		    return DM_FAILURE;
		}
		if (options->noNewInfo == 0)
		    mvSetTrackDataFieldInfo(imageTrack,frameIndex,
			dummyFrame.field2size,
			0,
			dummyFrame.field2size);
	    }
	    else { /* this is a new drop */
                MC(mvReadCompressedImage( imageTrack, frameIndex,
                    sizeof ( dummyFrame ), &dummyFrame) );
                                           /* size of newBuffer with 2 fields*/
                newSize = 2*dummyFrame.field2size;
                size = mvGetCompressedImageSize( imageTrack, frameIndex - 1 );
    
                /* make sure the marker data corresponds to the same 
                                                          last good frame */
                assert(size==(dummyFrame.field1size + dummyFrame.field2size));
    
                if ( origBufferSize < size ) { 
                                          /* dynamicaly size buffer as needed */
                    if ( origBufferSize )
                        free ( origBuffer );
                    origBuffer = malloc(size);
                    origBufferSize = size;
                }

                /* get the original good frame */
                MC(mvReadCompressedImage( imageTrack, frameIndex-1, 
                                                         size, origBuffer) );

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
		if (options->noNewInfo == 0)
		    mvSetTrackDataFieldInfo(imageTrack,frameIndex,
			dummyFrame.field2size,
			0,
			dummyFrame.field2size);
            }

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

DMstatus CutFromFirstDroppedFrame
    (
    MovieState* movie,
    Options* options
    )
{
    MVid imageTrack = movie->imageTrack;
    MVid audioTrack = movie->audioTrack;
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

    if ( options->audio ) {
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
   return(DM_SUCCESS);
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
    AudioState* audio,
    CompState*  comp
    )
{
    long long videoStart;
    long long audioStart;
    
    /*
    ** Get the timestamp of the first field.
    */
    
    {
	CLimageInfo info;
	PeekImageInfo( comp, &info );
	videoStart = info.ustime;
    }
    
    /*
    ** Get the timestamp of the first audio frame.
    */
    
    audioStart = audio->startTime;
    
    /*
    ** Compute the number of frames that corresponds to this
    ** difference.  (The timestamps are measured in nanoseconds.)
    */
    
    audio->framesToDrop = 
	( videoStart - audioStart ) * ((int) audio->rate) / BILLION;
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
	 fprintf(stderr, "%s: run as root to enable real time scheduling\n",
                  g_ProgramName);
      }
   else
      {
      if(schedctl(NDPRI, 0, 0) < 0)  /* return to normal priority */
	 fprintf(stderr, "run as root to enable real time scheduling\n");
      }
}

/********
*
* cosmo_capture
*
********/

void capture
    (
    Options* 	options
    )
{
    AudioState	audio;
    VideoState	video;
    CompState	comp;
    MovieState	movie;

    int		framesToCapture;
    int 	frame;

    /*
    ** Get things set up.
    **    - compression needs video info for image size
    **    - movie needs audio and video info for track formats
    */
    
    OpenCompression( &comp, &video, options );

    if ( options->audio ) {
	SetupAudio( &audio, options );
    }
    SetupVideo( &video, options );
    SetupCompression( &comp, &video, options );
    SetupMovie( &movie, &audio, &video, options );

    /*
    ** Wait for go ahead from the main program.
    */
    
    goAhead();

    /*
    ** Start the data flowing in.
    */
    
    if ( options->audio ) {
	StartAudio( &audio );
    }
    StartVideo( &video );
    StartCompression( &comp );
    
    /*
    ** Figure out how many frames to capture.
    */
    
    if ( options->seconds != 0 ) {
	framesToCapture = (int) ( options->seconds * video.rate + 0.5 );
    }
    else {
	framesToCapture = INT_MAX;  /* forever */
    }
    
    /*
    ** Determine the audio adjustment requried to synchronize audio
    ** and video.  The audio state is updated so that when we start
    ** writing audio to the movie it will be offset correctly to line
    ** up with the video.
    */
    
    if ( options->audio ) {
	InitialSync( &audio, &comp );
    }
    
    /*
    ** Start recording.  Run until the done() function in main.c says
    ** that we should stop or until we read the designated number of
    ** frames. 
    */
    
    for ( frame = 0;  (frame < framesToCapture) && !done();  frame++ ) {
	if ( options->verbose > 1 ) {
	    printf("Frame %u: ", frame);
	}
	if ( options->audio ) {
	    if ( CaptureAudioFrame(&audio,&movie,options,frame) != DM_SUCCESS ){
		fprintf( stderr, "Error capturing audio for frame %d\n", frame);
		goto stop_early;
	    }
	}
	if ( CaptureImageFrame( &comp, &movie, options, frame ) != DM_SUCCESS ){
	    fprintf( stderr, "Error capturing image frame %d\n", frame );
	    goto stop_early;
	}
    }
    assert( frame == totalFramesDropped + totalFramesCaptured );
    if ( totalFramesDropped == 0 ) {
	printf( "Recording was done in real time successfully.\n");
    }
stop_early:

    /*
    ** drop back to a standard priority for the file cleanup
    */
    
    setscheduling(0);
    if ( ! abortRecording ) {
	printf( "Post-processing output file '%s' ... ", 
		basename(options->fileName));
    }
    fflush(stdout);

    /*
    ** Fill in the dropped frames by replicating the ones before them.
    ** If that fails (because of lack of disk space), remove all of the
    ** movie past the first dropped frame.
    */
    
    if ( ! abortRecording ) {
	if ( ReplicateVideoFrames( &movie, options ) != DM_SUCCESS ) {
	    fprintf( stderr, "Could not fill in dropped frames.\n" );
	    CutFromFirstDroppedFrame( &movie, options );
	}
	PrintTimingInformation( video.rate );
    }
    
    /*
    ** Clean up.  We have either finished normally or aborted.
    */

    if ( ! abortRecording ) {
	PrintStatistics( video.width, video.height, video.rate );
    }
    
    CleanupMovie( &movie, options );
    CleanupCompression( &comp );
    CleanupVideo( &video );
    if ( options->audio ) {
    	CleanupAudio( &audio );
    }
} /* capture */
