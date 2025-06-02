////////////////////////////////////////////////////////////////////////
//
// mvfilter.c++
//	
//	A program to run a movie through an image-processing filter.
//
// Copyright 1995, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//
////////////////////////////////////////////////////////////////////////

#include <dmedia/PremiereXAPI.h>
#include <dmedia/fx_buffer.h>
#include <dmedia/fx_plugin_mgr.h>

#include <assert.h>
#include <bstring.h>
#include <dlfcn.h>	// dlopen(), dlsym()
#include <stdio.h>
#include <stdlib.h>	// exit()
#include <string.h>	// strcmp()

#include <dmedia/moviefile.h>

////////////////////////////////////////////////////////////////////////
//
// Utility Functions
//
////////////////////////////////////////////////////////////////////////

////////
//
// sameString
//
////////

static inline DMboolean sameString(
    const char* a,
    const char* b
    )
{
    return strcmp( a, b ) == 0;
}

////////
//
// mvErrCheck
//
////////

static void mvErrCheck(
    DMstatus status,
    const char* message,
    const char* detail
    )
{
    if ( status != DM_SUCCESS )					
    {								
	fprintf( stderr, message, detail );
	fprintf( stderr, "      %s\n", mvGetErrorStr( mvGetErrno() ) );
	exit( 1 );							
    }								
}

static void mvErrCheck(
    DMstatus status,
    const char* message
    )
{
    if ( status != DM_SUCCESS )					
    {								
	fprintf( stderr, message );
	fprintf( stderr, "      %s\n", mvGetErrorStr( mvGetErrno() ) );
	exit( 1 );							
    }								
}

////////
//
// dmErrCheck
//
////////

static void dmErrCheck(
    DMstatus status
    )
{
    if ( status != DM_SUCCESS )
    {
	int errornum;
	char error_detail[DM_MAX_ERROR_DETAIL];
	const char* error = dmGetError( &errornum, error_detail );
	fprintf( stderr, "Error %d: %s\n", errornum, error );
	fprintf( stderr, "%s\n", error_detail );
	exit( 1 );
    }
}

////////////////////////////////////////////////////////////////////////
//
// Globals
//
////////////////////////////////////////////////////////////////////////

const char*	programName;

#define MAX_OPS 50

DMplugmgr*	plugInManager	= NULL;

int		stackDepth	= 0;

int		opCount		= 0;

DMprocessop	ops	[MAX_OPS];
DMboolean	setup;

DMboolean	haveOutput	= DM_FALSE;
const char*	outputFileName;
MVid		outputMovie;
MVid		outputTrack;

MVtimescale	timeScale	= 600;
MVtime		frameDuration	= 20;	// 30 fps
MVtime		movieDuration	= 0;

DMimageinterlacing	interlacing	= DM_IMAGE_NONINTERLACED;

////////////////////////////////////////////////////////////////////////
//
// Command-line Parsing
//
////////////////////////////////////////////////////////////////////////

////////
//
// usage
//
////////

const char* usageMessage =
"Usage: mvfilter [-r frameRate] [-s]\n"
"                [-frame | -even | -odd]\n"
"                [ -i movie | -f filter | -t transition ]*\n"
"                -o outputMovie\n"
"\n"
"The arguments on the command-line list the input movie(s), and the\n"
"operations to be performed on them.  The operations are postfix (like\n"
"PostScript): filters apply to the previous movie and transitions apply\n"
"to the previous two.\n"
"\n"
"The optional '-s' causes the settings dialog box for every filter and\n"
" transition to be displayed, so that settings can be adjusted before\n"
"the filter or transition is applied.\n"
"\n"
"The duration of the output movie is the duration of the shortest input\n"
"movie.  The frames of the output movie will all have the same\n"
"duration; the rate is set with the '-r' option, and defaults to 30.\n"
"The format (image size, compression, file format) of the output movie\n"
"will be the same as that of the first input movie.\n"
"\n"
"By default, the movie is processed as frames.  When the '-even' option\n"
"is used, each frame is broken into two field, and the fields are processed\n"
"separately, with the even field treated as the dominant field.  The even\n" 
"field is the one that contains the first scanline of each frame (line 0).\n"
"The '-odd' option does the same thing, with the other field being dominant.\n"
"\n";

void usage(
    )
{
    fprintf( stderr, "%s", usageMessage );
    exit( 1 );
}

////////
//
// checkOpCount
//
////////

static void checkOpCount(
    )
{
    if ( MAX_OPS <= opCount )
    {
	fprintf( stderr, "%s: too many arguments.\n", programName );
	exit( 1 );
    }
}

////////
//
// addInputMovie
//
////////

static void addInputMovie(
    const char* fileName
    )
{
    DMstatus status;

    //
    // Open the movie file.
    //

    MVid movie;
    status = mvOpenFile( fileName, O_RDONLY, &movie );
    mvErrCheck( status, "Could not open input movie: %s\n", fileName );
    
    //
    // Store the operation.
    //

    checkOpCount();
    ops[opCount].type		= DM_MOVIE;
    ops[opCount].movie		= movie;
    ops[opCount].startTime	= 0;
    opCount++;
    
    //
    // Update the output duration.
    //

    if ( movieDuration == 0 || 
	 mvGetMovieDuration( movie, timeScale ) < movieDuration )
    {
	movieDuration = mvGetMovieDuration( movie, timeScale );
	printf( "duration = %f\n", float(movieDuration) /
		float(timeScale) );
    }
    
    //
    // Update the stack info.
    //

    stackDepth++;
}

////////
//
// addFilter
//
////////

static void addFilter(
    const char* plugInName
    )
{
    //
    // Make sure there is enough input.
    //

    if ( stackDepth < 1 || haveOutput )
    {
	usage();
    }
    
    //
    // Find the plug-in with the given name.
    //
    
    DMeffect* effect = NULL;
    DMplugin* plugin = NULL;
    {
	for ( int i = 0;  i < dmPMGetPluginCount( plugInManager );  i++ )
	{
	    plugin = dmPMGetPlugin(plugInManager, i);
	    
	    if ( sameString( dmPMGetName( plugin ), plugInName ) &&
		 dmPMGetType( plugin ) == DM_VIDEO_FILTER )
	    {
		effect = dmPMCreateEffect( plugInManager, plugin );
		// dmPMSetVideoCallback( effect, getFrameCallback, NULL);
		break;
	    }
	}
	if ( effect == NULL )
	{
	    fprintf( stderr, "%s: Could not find filter: %s\n", 
		     programName, plugInName );
	    exit( 1 );
	}
    }
    
    //
    // Store the operation
    //

    checkOpCount();
    ops[opCount].type		= DM_FILTER;
    ops[opCount].effect		= effect;
    ops[opCount].totalFrames	= ( movieDuration + frameDuration - 1 ) /  
                                   	frameDuration;
    ops[opCount].startFrame	= 0;
    opCount++;
}

////////
//
// addTransition
//
////////

static void addTransition(
    const char* plugInName
    )
{
    //
    // Make sure there is enough input.
    //

    if ( stackDepth < 2 || haveOutput )
    {
	usage();
    }
    
    //
    // Find the plug-in with the given name.
    //
    
    DMeffect* effect = NULL;
    DMplugin* plugin = NULL;
    {
	for ( int i = 0;  i < dmPMGetPluginCount( plugInManager );  i++ )
	{
	    plugin = dmPMGetPlugin(plugInManager, i);
	    
	    if ( sameString( dmPMGetName( plugin ), plugInName ) &&
		 dmPMGetType( plugin ) == DM_VIDEO_TRANSITION )
	    {
		effect = dmPMCreateEffect( plugInManager, plugin );
		// dmPMSetVideoCallback( effect, getFrameCallback, NULL);
		break;
	    }
	}
	if ( effect == NULL )
	{
	    fprintf( stderr, "%s: Could not find transition: %s\n", 
		     programName, plugInName );
	    exit( 1 );
	}
    }
    
    //
    // Store the operation
    //

    checkOpCount();
    ops[opCount].type		= DM_TRANSITION;
    ops[opCount].effect		= effect;
    ops[opCount].totalFrames	= ( movieDuration + frameDuration - 1 ) /
	                              frameDuration;
    ops[opCount].startFrame	= 0;
    opCount++;
    stackDepth--;
}

////////
//
// addOutputMovie
//
////////

static void addOutputMovie(
    const char* fileName
    )
{
    DMstatus status;
    
    outputFileName = fileName;
    
    
    //
    // Make sure that we're down to one thing on the stack.
    //

    if ( stackDepth != 1 || haveOutput )
    {
	usage();
    }
    
    //
    // The first op ought to be a movie.  Use its format for the
    // output movie.
    //

    assert( ops[0].type == DM_MOVIE );
    MVid inMovie = ops[0].movie;
    MVid imageTrack;
    status = mvFindTrackByMedium( inMovie, DM_IMAGE, &imageTrack );
    if ( status != DM_SUCCESS )
    {
	fprintf(
	    stderr, 
	    "%s: first input movie has no image track\n", 
	    programName );
	exit( 1 );
    }
    
    //
    // Create the output movie.
    //

    status = mvCreateFile( fileName, mvGetParams(inMovie), NULL, &outputMovie);
    mvErrCheck( status, "Could not create output movie: %s\n", fileName );
    status = mvSetMovieTimeScale( outputMovie, timeScale );
    mvErrCheck( status, "Could not set movie time scale.\n" );
    
    //
    // Set up the image params for the output movie track.
    //

    DMparams* origFormat = mvGetParams(imageTrack);

    DMparams* jpeg;
    status = dmParamsCreate( &jpeg );
    dmErrCheck( status );
    status = dmSetImageDefaults( 
	jpeg, 
	dmParamsGetInt( origFormat, DM_IMAGE_WIDTH ),
	dmParamsGetInt( origFormat, DM_IMAGE_HEIGHT ),
	DM_IMAGE_PACKING_ABGR
	);
    dmErrCheck( status );
    status = dmParamsSetEnum( jpeg, DM_IMAGE_ORIENTATION, DM_TOP_TO_BOTTOM );
    dmErrCheck( status );
    status = dmParamsSetString( jpeg, DM_IMAGE_COMPRESSION, DM_IMAGE_UNCOMPRESSED );
    dmErrCheck( status );
    status = dmParamsSetEnum( jpeg, DM_IMAGE_INTERLACING, interlacing ); 
    dmParamsSetFloat( jpeg, DM_IMAGE_QUALITY_SPATIAL, 0.75 );
    
    //
    // Create an image track.
    //

    status = mvAddTrack( outputMovie, DM_IMAGE, jpeg,
			 NULL, &outputTrack );
    mvErrCheck( status, "Could not add image track to output movie: %s\n",
		fileName );
    status = mvSetTrackTimeScale( outputTrack, timeScale );
    mvErrCheck( status, "Could not set track time scale.\n" );
    
    dmParamsDestroy( jpeg );

    stackDepth--;
    haveOutput = DM_TRUE;
}

////////
//
// updatePartAndTotal
//
////////

static void updatePartAndTotal(
    )
{
    for ( int i = 0;  i < opCount;  i++ )
    {
	if ( ops[i].type == DM_FILTER || ops[i].type == DM_TRANSITION )
	{
	    ops[i].totalFrames	= ( movieDuration + frameDuration - 1 ) /
		                      frameDuration;
	    printf( "Total frames = %d\n", ops[i].totalFrames );
	}
    }
}

////////
//
// parseArgs
//
////////

#define BUMP								\
    {									\
	i++;								\
	if ( argc <= i )						\
	{								\
	    usage();							\
	}								\
    }

static void parseArgs(
    int argc,
    char** argv
    )
{
    for ( int i = 1;  i < argc;  i++ )
    {
	if ( sameString( argv[i], "-r" ) )
	{
	    BUMP;
	    frameDuration = timeScale / atoi( argv[i] );
	}
	else if ( sameString( argv[i], "-i" ) )
	{
	    BUMP;
	    addInputMovie( argv[i] );
	}
	else if ( sameString( argv[i], "-f" ) )
	{
	    BUMP;
	    addFilter( argv[i] );
	}
	else if ( sameString( argv[i], "-t" ) )
	{
	    BUMP;
	    addTransition( argv[i] );
	}
	else if ( sameString( argv[i], "-s" ) )
	{
	    setup = True;
	}
	else if ( sameString( argv[i], "-o" ) )
	{
	    BUMP;
	    addOutputMovie( argv[i] );
	}
	else if ( sameString( argv[i], "-frame" ) )
	{
	    interlacing = DM_IMAGE_NONINTERLACED;
	}
	else if ( sameString( argv[i], "-even" ) )
	{
	    interlacing = DM_IMAGE_INTERLACED_EVEN;
	}
	else if ( sameString( argv[i], "-odd" ) )
	{
	    interlacing = DM_IMAGE_INTERLACED_ODD;
	}
	else
	{
	    usage();
	}
    }
    
    if ( stackDepth != 0  || ! haveOutput )
    {
	usage();
    }
    
    updatePartAndTotal();
}

////////////////////////////////////////////////////////////////////////
//
// Top-level Control
//
////////////////////////////////////////////////////////////////////////

static DMboolean progress(
    float fraction,
    void* /*clientData*/
    )
{
    printf( "%3d%% complete\n", int( fraction * 100 + 0.5 ) );
    return DM_TRUE;
}


////////
//
// cleanupOps
//
////////

static void cleanupOps(
    )
{
    DMstatus status;

    for ( int i = 0;  i < opCount;  i++ )
    {
	DMprocessop* op = &(ops[i]);
	
	switch( op->type )
	{
	    case DM_MOVIE:
		status = mvClose( op->movie );
		mvErrCheck( status, "could not close input movie\n" );
		break;
		
	    case DM_FILTER:
		dmPMDestroyEffect( op->effect );
		op->effect = NULL;
		break;

	    case DM_TRANSITION:
		dmPMDestroyEffect( op->effect );
		op->effect = NULL;
		break;

	    default:
		assert( FALSE );
		break;
	}
    }
}

////////
//
// main
//
////////

int main(
    int argc,
    char** argv
    )
{
    DMstatus status;
    
    //
    // Set up the plug-in manager.
    //
    
    plugInManager = dmPMCreateManager( NULL );
    if ( plugInManager == NULL )
    {
	fprintf( stderr, "Could not create plug-in manager.\n" );
	exit( 1 );
    }
    {
	dmPMInitDirectory( plugInManager, NULL, DM_ALL_PLUGIN_TYPES );
	if ( dmPMGetPluginCount(plugInManager) == 0 )
	{
	    fprintf( stderr, "Could not load any plugins from default directory\n" );
	    exit( 1 );
	}
    }

    //
    // Parse the command-line arguments.
    //

    programName = argv[0];
    parseArgs( argc, argv );
    
    //
    // Produce the output movie.
    //

    status = dmPMProcessImageClipWithProgress(
	ops,
	opCount,
	setup,
	timeScale,
	frameDuration,
	movieDuration,
	outputTrack,
	0,		// output start time
	progress,
	NULL
	);
    dmErrCheck( status );
    
    //
    // Clean up.
    //

    status = mvClose( outputMovie );
    mvErrCheck( status, "Could not close output movie: %s\n", outputFileName );
    
    cleanupOps();

    dmPMDestroyManager( plugInManager );

    return 0;
}
