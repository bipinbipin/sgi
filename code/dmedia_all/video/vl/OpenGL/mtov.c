/*
 * mtov.c -- command to use the SGI Movie Library to play a movie
 *    out to the SGI video port using the Video Library.
 *
 * Usage: mtov filename
 *
 * 5/24/94
 *
 */

#include <stdio.h>
#include <dmedia/moviefile.h>	/* LibMovie stuff */
#include <dmedia/vl.h>		/* Video Library inlcude */
#include <dmedia/dm_image.h>    /* dm_image stuff */
#include <string.h>		/* for strerror() */
#include <sys/errno.h>		/* for errno */
#include <sys/time.h>		/* for gettimeofday */
#include <bstring.h>		/* for bzero() */
#include <assert.h>		/* for debugging */

extern int errno;
int sginap( long );
static char * programName;
static char * sourceMovie = NULL;
static int SizeAndOffset = 0;
static int fastMode = 0;
static int lastFrame = 0;
static int statInterval = 0;	/* number of frames between reports */
static int frameCount = 0;

static void centerInputInOutput( unsigned long *, unsigned long, unsigned long,
				unsigned long *, unsigned long, 
				unsigned long, int );
static void setupVideoParams( VLServer svr, VLPath path, VLNode
				 srv, VLNode drn, DMinterlacing interlace,
				 unsigned long xSize, unsigned long ySize,
				 int frameRate);
static void RGBToYUV422_nofilter(int width, int height, unchar *rgb, unchar *uyvy);
static void fatal( const char * message, const char * error );
static void videoWarning( const char * message );

void processCmdArgs( int, char ** );
const char * getSourceMovieName( void );
const char * getProgramName( void );
void usage( void );



/* Get the start time (for frame rate measurement) */
void
initStatistics(struct timeval *tv_save)
{
    gettimeofday(tv_save);
}

/* Calculate and display the frame rate */
void
reportStatistics(int frameCount, struct timeval *lasttime)
{
    double rate;
    struct timeval tv;
    int delta_t;

    gettimeofday(&tv);

    /* Delta_t in microseconds */
    delta_t = (tv.tv_sec*1000 + tv.tv_usec/1000) -
              (lasttime->tv_sec*1000 + lasttime->tv_usec/1000);
   
    rate = frameCount*1000.0/delta_t;

    fprintf(stderr, "%5.2f frames/sec\n", rate);
    lasttime->tv_sec = tv.tv_sec;
    lasttime->tv_usec = tv.tv_usec;
}

main ( int argc, char **argv )
{
    MVid theSourceMovie;
    MVid theVideoTrack;
    DMparams *sourceParams;
    size_t mvFrameSize;		/* The frame size of the movie */
    void * mvBuffer;		/* the buffer for the movie reading */
    float frameRate;		/* the frame Rate at which we should */
				/* play */
    DMorientation orientation;	/* Bottom to top or top to bottom? */
    unsigned long mvFrames;	/* number of frams in the image track */
    DMinterlacing mvInter;	/* the movie's interleaving */
    struct timeval tv_save;	/* for statistics */
    
    
    VLServer svr;
    VLPath MemToVidPath;
    VLNode src, drn;
    VLBuffer buf;
    unsigned long transferSize;	/* VL Frame Size */
    VLControlValue val;
    
    unsigned long frameNum;	/* The frame number we're doing */
    unsigned long xOutSize, yOutSize, xInSize, yInSize;
    
    processCmdArgs( argc, argv );

    /* 
     * First, open the source movie file...
     *
     */
    
    if ( mvOpenFile( getSourceMovieName(), O_RDONLY,
		    &theSourceMovie ) != DM_SUCCESS )
	fatal( "Could not open movie file", getSourceMovieName() );
    
    if ( mvFindTrackByMedium( theSourceMovie, DM_IMAGE,
			     &theVideoTrack ) != DM_SUCCESS )
	fatal( "Could not find image track in movie file",
	      getSourceMovieName() );
    
    sourceParams = mvGetParams( theVideoTrack );
    
    mvFrameSize = dmImageFrameSize( sourceParams );
    xInSize = dmParamsGetInt( sourceParams, DM_IMAGE_WIDTH );
    yInSize = dmParamsGetInt( sourceParams, DM_IMAGE_HEIGHT);
    frameRate = dmParamsGetFloat ( sourceParams, DM_IMAGE_RATE );
    orientation = (DMorientation) dmParamsGetEnum( sourceParams, 
					DM_IMAGE_ORIENTATION );
    mvFrames = dmParamsGetInt ( sourceParams, MV_TRACK_LENGTH );
    mvInter = (DMinterlacing) dmParamsGetEnum ( sourceParams, 
					DM_IMAGE_INTERLACING );
    /* Note that the movie library gets this backwards. */
    
    if ( orientation == DM_BOTTOM_TO_TOP ) 
	fprintf(stderr,"%s: Source movie %s requires flipping.\n",
		getProgramName(), getSourceMovieName() );
    
    if ( (mvBuffer = malloc( mvFrameSize * sizeof(long) ) ) == NULL )
	fatal( "Can't allocate buffer for movie reading\n",
	      strerror(errno) );
    
    /*
     * Now, Open Video Device 
     *
     */
    
    if ( ( svr = vlOpenVideo( "" ) ) == NULL )
	fatal( "Error opening the video output server",
	      vlStrError(vlGetErrno()) );
    
    src = vlGetNode( svr, VL_SRC, VL_MEM, VL_ANY );
    drn = vlGetNode( svr, VL_DRN, VL_VIDEO, VL_ANY );
    
    if ( src == -1 || drn == -1 )
	fatal( "can't get node:", vlStrError(vlGetErrno()) );
    
    if ( ( MemToVidPath = vlCreatePath(svr, VL_ANY, src, drn ) ) == -1 )
	fatal( "can't create path:",  vlStrError(vlGetErrno()) );
	
    if ( vlSetupPaths(svr, (VLPathList)&MemToVidPath, 1, 
		      VL_SHARE, VL_SHARE) < 0) 
	fatal( "can't setup path:", vlStrError(vlGetErrno()));

    /* 
     * And Set up the video parameters! 
     *
     */
    
    setupVideoParams( svr, MemToVidPath, src, drn,
		     mvInter,
		     xInSize, yInSize,
		     frameRate );

    /*
     * And check to see what video size we get to work with
     *
     */
    
    vlGetControl( svr, MemToVidPath, src, VL_SIZE, &val);
    xOutSize = val.xyVal.x;
    yOutSize = val.xyVal.y;

    transferSize = vlGetTransferSize(svr,MemToVidPath);
    buf = vlCreateBuffer(svr, MemToVidPath, src, (int) (frameRate/4)+1 );
    /* Buffer up 1/4 second ahead, before starting the video running */
    
    vlRegisterBuffer( svr, MemToVidPath, src, buf);
    
    if (vlBeginTransfer( svr, MemToVidPath, 0, NULL) != 0 ) 
	fatal( "Unable to start transfer!", vlStrError(vlGetErrno()) );
    
    /*
     * The loop where all the actual data transfer from the 
     * movie to the video port gets done.
     *
     * Since this is a demo program, it ignores looping, and
     * simply plays each frame once, and then stops.
     */

    if (statInterval)
	initStatistics(&tv_save);
    
    if (!fastMode)
    	for ( frameNum = 0; frameNum < mvFrames; frameNum += 1 ) 
	{
	    unsigned long * outBuffer;
	    VLInfoPtr info;
	    
	    /* Read the frame of the movie */
	    if ( mvReadFrames( theVideoTrack, frameNum, 1,
			      mvFrameSize, mvBuffer ) != DM_SUCCESS )
		fatal("Error reading movie frames", "");
	    
	    /* Wait for a free buffer in the ring buffer */
	    do {
		info = vlGetNextFree( svr, buf, transferSize);
		sginap( 1 );	/* nap to let others run while we wait */
	    } while (!info);
	    
	    /* get the pointer to it's data space */
	    outBuffer = vlGetActiveRegion(svr,buf,info);
	    
	    /* copy the data in */
	    centerInputInOutput( mvBuffer, xInSize, yInSize,
				outBuffer, xOutSize, yOutSize, 
				orientation == DM_BOTTOM_TO_TOP );
	    
	    fprintf(stderr, "Frame %03d\r",frameNum);
	    fflush(stderr);
	    
	    /* Send the data out */
	    vlPutValid( svr, buf );
	    frameCount++;
	    if (statInterval && (frameCount == statInterval)) {
		reportStatistics(frameCount, &tv_save);
		frameCount = 0;
	    }
	}
    else 
    {
	int outImageSize = xOutSize*yOutSize/2;	/* YUV pixels only */
        unsigned long 	*outBuffer, 
			*tmpBuffer,
			*imageBuffer,
	 		*imagePointer;
	VLInfoPtr info;

	if (lastFrame == 0)	/* not set, use default mv length */
		lastFrame = mvFrames;
	else if (lastFrame > mvFrames)	
		lastFrame = mvFrames;

	/* limit the number of frames to something reasonable ? */
	if (lastFrame*outImageSize > 32*1024*1024)  {
		fprintf(stderr, "Rediculous number of frames %d\n", lastFrame);
		exit(1);
	}
	    
	/* allocate space and start consuming and converting frames */
	tmpBuffer = malloc (outImageSize * sizeof(long));
	imageBuffer = malloc ((lastFrame+1)*outImageSize*sizeof(long));

	for (frameNum = 0; frameNum < lastFrame; frameNum++)
	{
		/* Read the frame of the movie */
		fprintf(stderr, "Unpacking Frame %d\r", frameNum);
		fflush(stderr);
		if ( mvReadFrames( theVideoTrack, frameNum, 1, mvFrameSize,
				mvBuffer) != DM_SUCCESS )
			fatal("Error reading movie frames", "");
		

		/* center the data into the array */
		centerInputInOutput( mvBuffer, xInSize, yInSize,
				tmpBuffer, xOutSize, yOutSize, 
				orientation == DM_BOTTOM_TO_TOP );

		imagePointer = imageBuffer+(frameNum*outImageSize);

		/* convert to correct image space, bypassing colorspace converter */
		RGBToYUV422_nofilter(xOutSize, yOutSize, (unchar *)tmpBuffer,
					(unchar *)imagePointer);
	}

	free(tmpBuffer);
	    
	while (1)
		for (frameNum = 1; frameNum < lastFrame; frameNum++)
		{
	    		/* Wait for a free buffer in the ring buffer */
	    		do {
				info = vlGetNextFree( svr, buf, transferSize);
				sginap( 1 );	/* nap to let others run while we wait */
	    		} while (!info);

	    		fprintf(stderr,"Frame %03d\r",frameNum);
			fflush (stderr);

	    		imagePointer = imageBuffer+frameNum*outImageSize;

	    		/* get the pointer to it's data space */
	    		outBuffer = vlGetActiveRegion(svr,buf,info);
		
	    		bcopy(imagePointer, outBuffer, outImageSize*sizeof(long));
		
	    		/* Send the data out */
	    		vlPutValid( svr, buf );
	    		frameCount++;
	    		if (statInterval && (frameCount == statInterval)) {
				reportStatistics(frameCount, &tv_save);
				frameCount = 0;
	    		}
		}
    }
    
}


/*
 * centerInputInOutput --
 *
 * Takes pointers to the input buffer and output buffer,
 * values for the input size and output size, and a boolean
 * representing whether a flip is needed.
 * 
 * This functions copies the Input Buffer into the center of
 * the output buffer, blacking out the rest. It flips the buffer
 * while copying if needed.
 *
 */

static void
    centerInputInOutput(unsigned long * inBuf, 
			unsigned long xInSize, 
			unsigned long yInSize,
			unsigned long * outBuf,
			unsigned long xOutSize,
			unsigned long yOutSize,
			int flipBottomAndTop )
{
    unsigned long xIndent, xLen, yIndent, yLen;
    unsigned long y;
    
    bzero( outBuf, xOutSize * yOutSize * sizeof(long) );
    /*
     * I could be a bit more efficient by doing this while
     * I copy in the useful data, but, this is just a demo program,
     * so I don't care if I hit xLen*yLen*sizeof(long) bytes twice 
     */
    
    if ( xInSize > xOutSize )
	xLen = xOutSize;
    else
	xLen = xInSize;
    
    xIndent = (xOutSize - xLen) / 2 ;
    
    if ( yInSize > yOutSize )
	yLen = yOutSize;
    else
	yLen = yInSize;
    
    yIndent = (yOutSize - yLen) / 2;
    
    if ( flipBottomAndTop ) {
	outBuf += ( yIndent * xOutSize ) + xIndent + (xOutSize * (yLen-1));
	for ( y = 0; y < yLen; y += 1 ) {
	    memcpy( outBuf, inBuf, xLen * sizeof( long ) );
	    outBuf -= xOutSize;
	    inBuf += xInSize;
	}
    } else {
	outBuf += ( yIndent * xOutSize ) + xIndent;
	for ( y = 0; y < yLen; y += 1 ) {
	    memcpy( outBuf, inBuf, xLen * sizeof( long ) );
	    outBuf += xOutSize;
	    inBuf  += xInSize;
	}
    }
}

/*
 * setupVideoParams--
 *
 * This function sets up the video parameters on the 
 * server/path/source/drain combination of svr, path, src, drn.
 * 
 * - sets the interlace mode as requested,
 * - sets the frame rate as close as possible to the requested value.
 * - comes as close to the requested size as possible (note that
 *   the caller of setupVideoParams should afterwards CHECK what size
 *   was set up. Note also that this only happens if SizeAndOffset()
 *   is true.
 *
 */

static void 
    setupVideoParams( VLServer svr,
		     VLPath path,
		     VLNode src,
		     VLNode drn,
		     DMinterlacing interlace,
		     unsigned long xSize,
		     unsigned long ySize,
		     int frameRate )
{
    VLControlValue val, val2, offsets;
    int interlaced=0;
    
    /* Set up whether the output should be interlaced or not */
    /* Note that the value passed in comes from the movie library and */
    /* is backwards. */
    if ( interlace == DM_IMAGE_NONINTERLEAVED ) {
	val.intVal = VL_CAPTURE_INTERLEAVED;
	interlaced = 1;
    } else if ( interlace == DM_IMAGE_INTERLEAVED ) {
	val.intVal = VL_CAPTURE_NONINTERLEAVED;
	interlaced = 0;
    }
    if ( vlSetControl( svr, path, src, VL_CAP_TYPE, &val) ) 
	videoWarning( "Unable to set VL_CAP_TYPE" );
    
    /* Set up the frame Rate based on the information the movie file */
    /* contained */
    val.fractVal.numerator = frameRate;
    val.fractVal.denominator = 1;
    if ( vlSetControl( svr, path, src, VL_RATE, &val ) )
	videoWarning( "Unable to set VL_RATE" );
    
    vlGetControl( svr, path, src, VL_RATE, &val2 );
    if ( val.fractVal. numerator != val2.fractVal.numerator ||
	val.fractVal.denominator != val2.fractVal.denominator )
	fprintf(stderr,"Frame rate couldn't be set to %d; set to %d/%d instead\n",
		frameRate, val2.fractVal.numerator, val2.fractVal.denominator);
    
    /* Set up the VL_PACKING to match the output of the movie library, */
    /* so that we don't need to do colorspace conversion */

    if (fastMode)
    	val.intVal = VL_PACKING_YVYU_422_8;
    else
    	val.intVal = VL_PACKING_RGB_8;

    if ( vlSetControl( svr, path, src, VL_PACKING, &val ) ) 
	videoWarning( "Unable to set VL_PACKING" );

    /* If the user wants a non-standard video output size, set up the */
    /* size and offset to center the output in the screen. */
    if ( SizeAndOffset ) {
	val.xyVal.x = xSize;
	val.xyVal.y = ySize;
	if ( vlSetControl( svr, path, src, VL_SIZE, &val ) )
	    videoWarning( "(unimportant) Unable to set VL_SIZE" );
	
	vlGetControl( svr, path, src, VL_SIZE, &val );
	if ( val.xyVal.x < xSize || val.xyVal.y < ySize ) {
	    fatal( "Oops. the size got too small!", "" );
	}
	
	vlGetControl( svr, path, drn, VL_SIZE, &val );
	
	offsets.xyVal.x = ( val.xyVal.x - xSize ) / 2;
	offsets.xyVal.y = ( val.xyVal.y - ySize ) / ( interlaced ? 2 : 4 );
	if ( vlSetControl( svr, path, VL_ANY, VL_OFFSET, &offsets) )
	    videoWarning( "(unimportant) Unable to set VL_OFFSET" );
	
	vlGetControl( svr, path, VL_ANY, VL_OFFSET, &offsets );
	fprintf( stderr,
		"offsets set to %d, %d\n",offsets.xyVal.x, offsets.xyVal.y );
    }
}

/*
 * fatal--
 *
 * prints the error message and reason, and exits.
 *
 */

static void 
    fatal( const char * message, const char * error )
{
    fprintf( stderr, "%s: %s %s\n", getProgramName(),
	    message, error );
    exit( EXIT_FAILURE );
}

/*
 * videoWarning--
 *
 * Prints the warning message and the current vl Error String
 *
 */

static void
    videoWarning( const char * message )
{
    fprintf( stderr," %s: %s (%s)\n", getProgramName(), message,
	    vlStrError(vlGetErrno()) );
}

/*
 * Utility functions for dealing with command line arguments
 *
 */

void processCmdArgs( int argc, char **argv )
{
    int ch;
    
    programName = argv[0];
    
    if ( argc < 2 ) {
	usage();
    }
    
    sourceMovie = argv[ --argc ];
    
    while ( ( ch = getopt( argc, argv, "sfn:i:" ) ) != -1 ) {
	switch ( ch ) {
	case 's':
	    SizeAndOffset = 1;
	    break;
	case 'n': lastFrame = atoi(optarg);
	case 'f': fastMode = 1;
		  break;
	case 'i': statInterval = atoi(optarg);
		  break;
	case '?':
	default:
	    usage();
	}
    }
}

const char * getSourceMovieName()
{ return sourceMovie; }

const char * getProgramName()
{ return programName; }

void usage() 
{
  fprintf(stderr,
	  "Usage: mtov [-s] moviefile [-f] [-i] count [-n] count\n"
   	  " [ -s ]  Use VL_SIZE (and VL_OFFSET) on src.\n"
   	  "         The default is not to, but instead black"
	  "         the unused screen area.\n"
	  " [ -f ]  Fast mode. Unpacks movie to memory.\n"
	  " [ -n ]  Number of frames to play (implies -f)\n"
   	  " [ -i ]  Report statistics every n frames.\n");
  exit( EXIT_FAILURE );
}

static void
RGBToYUV422_nofilter(int width, int height, unchar *rgb, unchar *uyvy)
{
    int w, h;
    int red1, green1, blue1, red2, green2, blue2;
    int u, v, y1, y2;

    for (h = height; h; h--) {
        for (w = width >> 1; w; w--) {
            rgb++;
            blue1  = *rgb++;
            green1 = *rgb++;
            red1   = *rgb++;

            rgb++;
            blue2  = *rgb++;
            green2 = *rgb++;
            red2   = *rgb++;

/*  U, V must be cosited with Y1 for digital video,         */
/*  strangely enough it must be centered for compression    */
/*  this library is digital video so we cosite the chroma.  */


            *uyvy++ = ((19071*(blue1 - green1) + 9714*(blue1 - red1)
                         + 8388608 + 32768) >> 16); /*U1*/
            *uyvy++ = ((16829*red1 + 33039*green1 + 6416*blue1
                         + 1048576 + 32750) >> 16);/*Y1*/
            *uyvy++ = ((24103*(red1-green1) - 4681*(blue1-red1)
                         + 8388608 + 32767) >> 16); /*U2*/
            *uyvy++ = ((16829*red2 + 33039*green2 + 6416*blue2
                         + 1048576 + 32750) >> 16);/*Y2*/
        }
    }
}

