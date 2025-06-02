/*******************************************************************************
 *
 * File:         aud-to-movie.c
 *
 * Usage:        aud-to-movie <audiofile> <moviefile>
 *
 * Description:  Simple program to add (or replace) a movie's audio track.
 *
 * Functions:    SGI Movie Library functions used: 
 *
 *               mvOpenFile()
 *               mvGetErrorStr()
 *               mvGetErrno()
 *               mvClose()
 *               mvFindTrackByMedium()
 *               mvRemoveTrack()
 *               mvAddTrack()
 *               mvGetAudioRate()
 *               mvGetImageRate()
 *               mvGetAudioWidth()
 *               mvGetTrackLength()
 *               mvInsertFrames()
 *
 *               SGI Digital Media functions used:
 *
 *               dmParamsCreate()
 *               dmSetAudioDefaults()
 *               dmParamsSetEnum()
 *
 */

/*
 * Standard Unix stuff.
 */

#include <sys/types.h>	/* For open(2)  */
#include <sys/stat.h>	/* For open(2)  */
#include <fcntl.h>	/* For open(2)  */
#include <stdlib.h>	/* For exit(2)  */
#include <stdio.h>	/* For printf(3S)  */
#include <unistd.h>	/* For close(2) */
#include <string.h>	/* For memset(3C) */

/*
 * Include the declarations for the movie library so we can create a
 * movie.
 */

#include <dmedia/moviefile.h>

/*
 * Include the declarations for the audio file library so we can 
 * read audio files.
 */

#include <audiofile.h>

/*
 * Forward declarations of functions that appear below.
 */
    
static void CloseFiles         ( MVid movie, AFfilehandle audioFile );
static DMstatus SetUpAudioTrack( MVid movie, AFfilehandle audioFile );
static DMstatus CopyAudio      ( AFfilehandle audioFile, MVid movie );

/*
 * Global program name variable for error handling.
 */

static char* programName;

/**********************************************************************
*
* main
*
**********************************************************************/

main( int argc, char** argv )
{
    MVid         movie;
    AFfilehandle audioFile = AF_NULL_FILEHANDLE;

    programName = argv[0];
    
    /*
     * Check usage.
     */
    
    if ( argc != 3 ) {
	printf( "Usage: %s audiofile moviefile\n", programName );
	exit( EXIT_FAILURE );
    }
    
    /*
     * Open the movie file.
     */

    if ( mvOpenFile(argv[2], O_RDWR, &movie ) == DM_FAILURE ) {
	fprintf( stderr, "%s: Unable to open movie file %s.\n",
                programName, argv[2] );
	fprintf( stderr,"    error = %s.\n", mvGetErrorStr( mvGetErrno() ) );
        exit( EXIT_FAILURE );
    }

    /*
     * Open the audio file.
     */
    
    audioFile = AFopenfile( argv[1], "r", AF_NULL_FILESETUP );
    if ( audioFile == AF_NULL_FILEHANDLE) {
	fprintf( stderr, "Unable to open audio file - %s.\n",
                programName, argv[1] );
        CloseFiles( movie, audioFile );
	exit( EXIT_FAILURE );
    }
    
    /*
     * Set up the audio track and then copy the audio samples from the
     * audio file to the movie.
     */

    if ( SetUpAudioTrack( movie, audioFile ) != DM_SUCCESS) {
        CloseFiles( movie, audioFile );
        exit( EXIT_FAILURE );
    }

    if ( CopyAudio( audioFile, movie ) != DM_SUCCESS ) {
        CloseFiles( movie, audioFile );
        exit( EXIT_FAILURE );
    }
    
    /*
     * All done!  Close the image and the movie.
     */
    
    CloseFiles( movie, audioFile );
    exit( EXIT_SUCCESS );
}

/*********
 *
 * CloseFiles - Little helper function for cleanup.
 *
 *********/

static void CloseFiles( MVid movie, AFfilehandle audioFile )
{
    mvClose( movie );
    if ( audioFile != AF_NULL_FILEHANDLE ) 
      AFclosefile( audioFile );
}

/*********
 *
 * SetUpAudioTrack
 *
 *********/

static DMstatus SetUpAudioTrack( MVid movie, AFfilehandle audioFile )
{
    MVid	audioTrack;
    DMparams*	atParams;
    long	fileWidth;
    long	fileFormat;
    double	fileRate;
    long	fileChannels;
    
    /*
     * Remove the audio track if there is one already.
     */

    if ( mvFindTrackByMedium( movie, DM_AUDIO, &audioTrack ) == DM_SUCCESS ) {
	if ( mvRemoveTrack( movie, audioTrack ) != DM_SUCCESS ) {
            fprintf( stderr, "%s: Couldn't remove previous audio track.\n",
                    programName );
            return DM_FAILURE;
        }
    }

    /*
     * The properties that must always be set for an audio track are:
     * sample width, sample rate, sample format, number of channels.
     */
    
    AFgetsampfmt( audioFile, AF_DEFAULT_TRACK, &fileFormat, &fileWidth );
    
    fileRate     = AFgetrate    ( audioFile, AF_DEFAULT_TRACK );
    fileChannels = AFgetchannels( audioFile, AF_DEFAULT_TRACK );
    
    if ( dmParamsCreate( &atParams ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Couldn't create audio params.\n", programName ); 
        return DM_FAILURE;
    }

    if ( dmSetAudioDefaults( atParams, fileWidth, fileRate, 
                            fileChannels ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Couldn't set audio defaults.\n", programName ); 
        return DM_FAILURE;
    }
    
    if ( dmParamsSetEnum( atParams, DM_AUDIO_FORMAT, 
                         fileFormat ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Couldn't set audio format.\n", programName ); 
        return DM_FAILURE;
    }
    
    /*
     * Add an audio track to the movie.
     */

    if ( mvAddTrack( movie, DM_AUDIO, atParams, NULL, 
        &audioTrack ) == DM_FAILURE) {
	fprintf( stderr, "%s: Unable to add audio track to movie.\n",
                programName );
	return DM_FAILURE;
    }
    return DM_SUCCESS; 
}

/*********
 *
 * CopyAudio
 *
 *********/

static DMstatus CopyAudio( AFfilehandle audioFile, MVid movie )
{
    MVid	audioTrack;
    MVid	imageTrack;
    double	audioRate;
    double	imageRate;
    int		chunkSize;
    int  	sampleSize;
    void*	buffer;
    int		i;

    /*
     * We know that the movie has an image track and an audio track,
     * so we can compute the number of audio samples per image frame.
     * Writing the audio in chunks that are this size is not
     * necessary, but it is a convenient size to use.
     */
    
    if ( mvFindTrackByMedium( movie, DM_AUDIO, &audioTrack ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Couldn't find audio track.\n", programName ); 
        return DM_FAILURE;
    }
    if ( mvFindTrackByMedium( movie, DM_IMAGE, &imageTrack ) != DM_SUCCESS ) {
        fprintf( stderr, "%s: Couldn't find image track.\n", programName ); 
        return DM_FAILURE;
    }
	
    audioRate = mvGetAudioRate( audioTrack );
    imageRate = mvGetImageRate( imageTrack );
    chunkSize = ( int ) ( audioRate / imageRate );

    /*
     * Allocate a buffer big enough to hold that many audio samples.
     */
    
    sampleSize = mvGetAudioWidth( audioTrack );
    buffer = malloc( chunkSize * sampleSize );
    if ( buffer == NULL ) {
        fprintf( stderr, "%s: Couldn't allocate audio buffer.\n", programName );
        return DM_FAILURE;
    }
    
    /*
     * For each image in the movie, copy the corresponding audio.
     */
    
    for ( i = 0;  i < mvGetTrackLength( imageTrack );  i++ ) {
	memset( buffer, 0, chunkSize * sampleSize );
	AFreadframes( audioFile, AF_DEFAULT_TRACK, buffer, chunkSize );
	if ( mvInsertFrames( audioTrack, i * chunkSize, chunkSize,
                           ( size_t ) chunkSize * sampleSize, 
                           buffer ) != DM_SUCCESS ) {
             fprintf( stderr, "%s: Couldn't write audio data frame.\n",
                     programName );
             free( buffer );
             return DM_FAILURE;
        }
		       
    }
    free( buffer );
    return DM_SUCCESS;
}
