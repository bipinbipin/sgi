/**********************************************************************
*
* File: main.c
*
* JPEG movie capture program.
*
**********************************************************************/

#include <stdio.h>
#include <errno.h>
#include <limits.h>		/* for ULONGLONG_MAX */

#include "dmrecord.h"
#include "handy.h"

extern void setscheduling (int hiPriority);
extern int setDefaults(void);

static char defaultString[] = "Default";

/*
**	global variables
**
*/

Options options;

/********
*
* main
*
********/

int main
    (
    int argc, 
    char** argv
    )
{
#if 0
    /*
    ** Set high non-degrading priority for realtime
    */
    
    setscheduling( 1 );

    /*
    ** switch to running as joe user (instead of root) if run setuid 
    */
	
    setreuid( getuid(), getuid() );
    setregid( getgid(), getgid() );
#endif
    /*
    ** Set the default options.
    */

    setDefaults();
    if (!HAS_GOOD_DEVICE) {
	fprintf(stderr, "No appropriate input video device (mvp or ev1) available\n");
	exit(1);
    }
    if (!HAS_GOOD_NODES) {
	fprintf(stderr,"No appropriate input video ports available\n");
	exit(1);
    }
    if (USEDMIC) {
	if (!HAS_GOOD_DMIC) {
	    fprintf(stderr, "Real-time JPEG compressor not available\n");
	    exit(1);
	}
    } else if (!HAS_GOOD_COSMO) {
	fprintf(stderr, "Cosmo JPEG compressor not available\n");
	exit(1);
    }

    /*
    ** Use the command-line arguments to override the defaults.
    */

    parseArgs( argc, argv);
    
    /*
    ** check if every thing has been correctly specified.
    */
	
    if ( options.videoDevice  == NULL ) {
	options.videoDevice = strdup(UseThisDevice.name);
    } else if (strcmp ( options.videoDevice, UseThisDevice.name ) != 0)  {
	fprintf(stderr,"Unsupported video device '%s'\n",options.videoDevice);
	usage(0);
    }
    
    if ( options.compressionScheme  == NULL ) {
	options.compressionScheme = strdup("jpeg"); 
	if ( options.verbose > 1) {
	    printf("Using default video compression scheme:  jpeg\n");
	}
    } else if ( strcasecmp( options.compressionScheme, "jpeg" ) !=0 ) {
	fprintf ( stderr, "Unsupported compression scheme '%s'\n", options.compressionScheme);
	usage(0);
    }
    
    if ( options.compressionEngine  == NULL ) {
	options.compressionEngine = UseThisEngine;
    } else if (strcasecmp( options.compressionEngine, UseThisEngine) != 0) {
	fprintf ( stderr, "Unsupported compression engine '%s'\n", options.compressionEngine);
	usage(0);
    }

    if ((options.nodeKind == VL_VIDEO) && (getSpecifiedPort() == -1)) {
	fprintf(stderr, "Unsupported input video port %s\n", options.videoPortName);
	usage(0);
    }
    
    if ( options.verbose ) {
	printf( "Options:\n");
	if ( options.audio )
	    printf("        Audio channels: %d\n", options.audioChannels);
	printf(
		   "          Video device: %s\n"
		   "            Input port: '%s' (%d)\n"
		   "    Compression scheme: %s\n"
		   "    Compression engine: %s\n",
		   options.videoDevice, 
		   options.videoPortName, options.videoPortNum,
		   options.compressionScheme, 
		   options.compressionEngine);
	if ( options.avrFrameRate )
	    printf("              Bit Rate: %d bits/sec\n", 
		   options.avrFrameRate);
	else
	    printf("               Quality: %d\n", 
		   options.qualityFactor);

	if ( options.movieTitle)
	    printf("File options:\n"
		   "                 Title: %s\n", options.movieTitle);
    } /* if verbose */
    
    if (options.hiPriority) {
	setscheduling( 1 );
    }
    setreuid( getuid(), getuid() );
    setregid( getgid(), getgid() );
    capture();
    
    exit( EXIT_SUCCESS );
    return 0;  /* make compiler happy */
} /* main */

