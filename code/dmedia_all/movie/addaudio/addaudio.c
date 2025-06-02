
#include <stdio.h>
#include <stdlib.h>
#include <movie.h>
#include <audiofile.h>

//
// InsertAFileIntoMovie
//
void InsertAFileIntoMovie( AFfilehandle AFile, MVid Movie, float fOffset )
{
    int 	AFfmt, AFwidth;
    double	AFrate;
    long	AFframes, AFchannels;
    MVid 	AudTrack;
    DMparams 	*pAudParams;
    void	*pBuf;
    int		i;
    MVtimescale	TrackTS;

    afGetSampleFormat( AFile, AF_DEFAULT_TRACK, &AFfmt, &AFwidth );
    AFrate= afGetRate( AFile, AF_DEFAULT_TRACK );
    AFframes   = afGetFrameCount( AFile, AF_DEFAULT_TRACK );
    AFchannels = afGetChannels( AFile, AF_DEFAULT_TRACK );
    
    dmParamsCreate( &pAudParams );

    dmSetAudioDefaults( pAudParams, AFwidth, AFrate, AFchannels );
    if (AFfmt == DM_AUDIO_UNSIGNED)
	dmParamsSetEnum( pAudParams, DM_AUDIO_FORMAT, DM_AUDIO_UNSIGNED );
    else if (AFfmt == DM_AUDIO_TWOS_COMPLEMENT)
	dmParamsSetEnum( pAudParams, DM_AUDIO_FORMAT, 
			 DM_AUDIO_TWOS_COMPLEMENT );
    else 
    {
	afSetVirtualSampleFormat( AFile, AF_DEFAULT_TRACK,
				  DM_AUDIO_TWOS_COMPLEMENT, AFwidth );
	dmParamsSetEnum( pAudParams, DM_AUDIO_FORMAT, 
			 DM_AUDIO_TWOS_COMPLEMENT );
    }

    if (mvAddTrack( Movie, DM_AUDIO, 
		    pAudParams, NULL, &AudTrack ) != DM_SUCCESS) 
    {
	dmParamsDestroy( pAudParams );
	fprintf( stderr, "MovieLib Error: %s\n",
		 mvGetErrorStr( mvGetErrno() ) );
	return;
    }
    pBuf = (void * )malloc( AFrate * dmAudioFrameSize( pAudParams ) );

    for (i = 0; i < AFframes;)
    {
	int	nFrRead = AFreadframes( AFile, AF_DEFAULT_TRACK, pBuf, AFrate );
	mvInsertFramesAtTime( AudTrack, i, nFrRead, AFrate, pBuf,
			      nFrRead * dmAudioFrameSize( pAudParams ),
			      pAudParams, 0 );
	i +=  nFrRead;
    }

    TrackTS = mvGetTrackTimeScale( AudTrack );
    mvSetTrackOffset( AudTrack, fOffset * TrackTS, TrackTS );

    dmParamsDestroy( pAudParams );
}


//
// main
//
int main( int argc, char *argv[] )
{
    MVid		Movie;
    AFfilehandle	AFile;

    if (argc < 3)
    {
	fprintf( stderr, "usage: %s <moviefile> <audiofile>\n",
		 argv[0] );
	return( 0 );
    }

    if (mvOpenFile( argv[1], O_RDWR, &Movie ) != DM_SUCCESS)
    {
	fprintf( stderr, "MovieLib Error: %s\n", 
		 mvGetErrorStr( mvGetErrno() ) );
	return( -1 );
    }

    AFile = afOpenFile( argv[2], "r", NULL );
    if (AFile == AF_NULL_FILEHANDLE)
    {
	fprintf( stderr, "Open AF File failed\n" );
	return( -1 );
    }

    if (argc > 3)
	InsertAFileIntoMovie( AFile, Movie, atof( argv[3] ) );
    else InsertAFileIntoMovie( AFile, Movie, 0 );

    mvClose( Movie );
    AFclosefile( AFile );

    {
	MVid	AudTrack;
	mvOpenFile( argv[1], O_RDONLY, &Movie );
	mvFindTrackByMedium( Movie, DM_AUDIO, &AudTrack );
	fprintf( stderr, "retrieved offset = %lld (TS = %d)\n",
		 mvGetTrackOffset( AudTrack, mvGetTrackTimeScale( AudTrack ) ),
		 mvGetTrackTimeScale( AudTrack ) );
	mvClose( Movie );
    }
    return( 0 );
}
