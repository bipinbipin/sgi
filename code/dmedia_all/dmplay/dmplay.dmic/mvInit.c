/*
 * mvInit.c: Does libmovie (movie library) initialization
 *
 * 
 * Silicon Graphics Inc., June 1994
 */

#include "dmplay.h"

char *imageBuffer; /* we rely on malloc to make it point to int boundary */

static void
mvErr(char *s)
{
    fprintf(stderr, "%s -- %s\n", s, mvGetErrorStr(mvGetErrno()));
}

void mvInit(void)
{
    const char *compressionScheme;
    
    /*
     * Open the movie file.
     */

    if (strlen(movie.filename) > PATH_MAX){
      fprintf(stderr,"Movie path too long\n");
      exit(1);
    }

    if ( mvOpenFile(movie.filename, O_RDONLY, &movie.mv) != DM_SUCCESS ) {
        mvErr("unable to open movie file");
	exit(1);
    }


    /*
     * Find the image track.
     */
    if (mvFindTrackByMedium(movie.mv,DM_IMAGE,&movie.imgTrack) != DM_SUCCESS ) {
	mvErr("movie does not contain an image track");
	exit(1);
    }

    /*
     * Put information about the image track into the "image"
     * structure.
     */
    image.isTrack      = 1; /*Currently, video should always be there.*/
    image.frameRate    = mvGetImageRate( movie.imgTrack );
    image.width        = mvGetImageWidth(movie.imgTrack);
    image.height       = mvGetImageHeight(movie.imgTrack);
    image.interlacing  = mvGetImageInterlacing(movie.imgTrack);
    image.numberFrames = mvGetTrackLength(movie.imgTrack);
    image.orientation  = mvGetImageOrientation(movie.imgTrack);
    image.timeScale    = mvGetTrackTimeScale(movie.imgTrack);

/*
    p = mvGetParams(movie.imgTrack);
    if (dmParamsIsPresent(p, DM_IMAGE_PIXEL_ASPECT) == DM_TRUE) {
	image.pixelAspect = dmParamsGetFloat(p, DM_IMAGE_PIXEL_ASPECT);
if(options.verbose)printf("DM_IMAGE_PIXEL_ASPECT is set at %f\n", image.pixelAspect);
    } else if ((image.width != 720) && (image.width != 360)) {
*/
    if ((image.width != 720) && (image.width != 360)) {
	image.pixelAspect = 1.0;
if(options.verbose)printf("DM_IMAGE_PIXEL_ASPECT not set; guessed to be 1.0\n");
    } else {
if(options.verbose)printf("DM_IMAGE_PIXEL_ASPECT not set, guessed to be != 1.0\n");
	image.pixelAspect = 0.9;
    }
    
    imageBuffer = malloc(image.width*image.height*2);
    if (imageBuffer == NULL) {
	ERROR("malloc failed");
    }


    /*
     * Put information about the movie into the "movie" structure.
     */
    movie.loopMode = mvGetLoopMode(movie.mv);
    movie.format = mvGetFileFormat(movie.mv);
    image.hasFieldLen = 0;
    if (DO_FIELD && (mvTrackDataHasFieldInfo(movie.imgTrack, 0) == DM_TRUE)) {
	image.hasFieldInfo = 1;
    } else {
	image.hasFieldInfo = 0;
    }

    {
    const char *p;

    p = mvGetTitle(movie.mv);

    if ( p && *p )
	movie.title = strdup(p);
    else
	movie.title = basename(movie.filename);
    }

    /*
     * This program currently only supports Cosmo JPEG movies.
     */
    compressionScheme = mvGetImageCompression( movie.imgTrack );
    if ( strcmp(compressionScheme, DM_IMAGE_JPEG) != 0 ) {
	if ( options.verbose ) {
	    fprintf( stderr, "\"%s\" uses %s compression scheme. \n",
		     basename(movie.filename), 
		     compressionScheme );   
	}
	fprintf( stderr, "Only JPEG movies are supported.");
	exit(1);
    }


    /*
     * set up clipping
     */
    if (image.height > 480 && image.height <= 496 || image.height > 570) {
	image.cliptop = 3;
	image.clipbottom = 3;
	if (image.display == GRAPHICS && image.height == 496)
	    image.clipbottom += 10;
    }


    /*
     * Print information about the image track. 
     */
    if ( options.verbose ) {
        printf("Image track parameters: '%s'\n", basename(movie.filename));
	printf("        length       = %d images\n", image.numberFrames);
        printf("	width        = %d\n", image.width);
        printf("	height       = %d\n", image.height);
	switch (image.interlacing) {
	    case DM_IMAGE_NONINTERLACED:
		printf("	interlace    = none\n");
		break;
	    case DM_IMAGE_INTERLACED_EVEN:
		printf("	interlace    = even\n");
		break;
	    case DM_IMAGE_INTERLACED_ODD:
		printf("	interlace    = odd\n");
		break;
	}
        printf("	orientation  = %s\n",
                                      image.orientation==DM_BOTTOM_TO_TOP?
                                          "bottom to top":"top to bottom");
    }

    /*
     * Find the audio track.
     */
    if (mvFindTrackByMedium(movie.mv,DM_AUDIO, &movie.audTrack) != DM_SUCCESS) {
	if ( options.verbose ) {
	    fprintf(stderr, "There is no audio track in the movie.\n");
	}
	audio.isTrack = 0;
	return;
    }
    
    /*
     * Put information about the audio track into the "audio"
     * structure. 
     */
    audio.isTrack      = 1; 
    audio.frameCount   = mvGetTrackLength(movie.audTrack);
    audio.sampleWidth  = mvGetAudioWidth (movie.audTrack);
    audio.frameRate    = mvGetAudioRate (movie.audTrack);
    audio.channelCount = mvGetAudioChannels (movie.audTrack);
    audio.frameSize    = dmAudioFrameSize( mvGetParams( movie.audTrack ) );
    
    audio.blockSize = audio.frameRate / image.frameRate;
    
    /*
     * Audio queue is sized to be 2 seconds long
     */
    audio.queueSize = audio.frameRate * audio.channelCount * 2;

    if ( options.verbose ) {
	printf ("Audio Queue Size = %d\n", audio.queueSize);
	printf ("Audio Frame Count = %d\n", audio.frameCount);
	printf ("Audio Frame Rate = %f\n", audio.frameRate);
    }

}

void deinterlaceImage( void* vfrom, void* vto)
{
    /* Format is XBGR, 4 bytes/pixel */
    char* from = vfrom;
    char* to   = vto;
    
    int replicate,odd,flip;
    int height, width;
    int i;

    /*
     * Flip and de-interlace
     */
    odd       = (DO_FRAME ? 0 : 1);
    height    = image.height;
    width     = image.width * 4;
    replicate = 0;  /* turned off */
    flip      = 0;  /* turned off */

    if (replicate)
        for ( i = 0; i < height/2; i++)
            if (flip) {
                bcopy(from+width*i, to+width*(height-1-2*i), width);
                bcopy(from+width*i, to+width*(height-1-2*i+1), width);
            } else {
                bcopy(from+width*i, to+width*2*i, width);
                bcopy(from+width*i, to+width*(2*i+1), width);
            }
    else /* interleave the two fields */ {
        /* First field */
        for ( i = 0; i < height/2; i++)
            if (flip)
                bcopy(from+width*i, to+width*(height-1-(2*i+odd)), width);
            else
                bcopy(from+width*i, to+width*(2*i+odd), width);
        odd = !odd;

        /* Second field */
        for ( i = 0; i < height/2; i++)
            if (flip)
                bcopy(from+width*(i+(height/2)),
                                to+width*(height-1-(2*i+odd)), width);
            else
                bcopy(from+width*(i+(height/2)), to+width*(2*i+odd), width);
    }
} /* deinterlaceImage */

void deinterlaceField( void* vfrom, void* vto, int fieldNum)
{
    /* Format is CbYCrY, 4 bytes/2 pixels */
    char* from = vfrom;
    char* to   = vto;
    int odd;
    int height, width;
    int i;

    height    = image.height / 2;
    width     = image.width * 2;
    odd       = (DO_FRAME ? 0 : 1);

    if ((fieldNum & 1) == 1) {
	odd = !odd;
    }
    for ( i = 0; i < height; i++) {
	bcopy(from+width*i, to+width*(2*i+odd), width);
    }

} /* deinterlaceField */
