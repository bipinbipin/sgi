#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <libgen.h>		/* for basename() */
#include <limits.h>		/* for ULONGLONG_MAX */
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/time.h>

#include <dmedia/moviefile.h>
#include <dmedia/vl.h>
#include "handy.h"
#include "dmrecord.h"

/********
*
* usage
*
* if invoked with non-zero verbose parameter (via '-h' option),
* usage displays the whole banana, else just a lean, mean, terse
* ditty in the Great Unix Tradition we all know and love to hate.
*
********/

void usage
    (
    int verbose
    )
{
	fprintf(stderr, "Usage: \n\
%s -h  : print help message\n\n\
%s [-v][-B auto|key ][-C][-t secs] -p video_port [-p audio_port] [-f file_options] FILE\n",
		g_ProgramName,g_ProgramName);

	if (verbose) {
		fprintf(stderr, "\n\
-p video_port   : -p video[,device=name[,port=N][,comp=jpeg[,engine=name]\n\
                     [,quality=Q] [,brate=N] ]\n\
        device  : impact and ev1 are the supported video input devices\n\
        port    : video input port number\n\
        comp    : JPEG compression is the only supported scheme\n\
        engine  : compression engine (impact and cosmo are the supported JPEG engines)\n\
        quality : JPEG quality factor from 1-100 (default=75)\n\
	brate	: compression bit rate in bits per second (if omitted, record with no rate control\n\
\n\
-p audio_port   : -p audio[,channels=C]\n\
                     enable recording from audio input port\n\
                     (use Audio Control Panel to select\n\
                      source={mic,line,digital} and rate)\n\
        channels: 1 for mono, 2 for stereo\n\
\n\
-v      verbose\n\
-B      begin-recording mode\n\
            auto: recording begins as soon as possible\n\
            key : recording begins when user hits a key (default)\n\
-C      critical: abort recording, delete FILE if any video frames are dropped\n\
-t      recording duration in seconds (if omitted, record until <ctrl>-c)\n\
-f file_options: -f [title=movie_name]\n");

		fprintf(stderr,
"-s      Use old movie format (SGI-3). Default is to use the new format\n\
        (QuickTime). Old dmplay cannot play movies in the new format.\n\
-S      Use SGI 6.2 Quicktime format.  Default is to use a newer\n\
        QuickTime that is incompatible with 6.2 dmplay.\n");


		fprintf(stderr, "\n\
Example: record synchronized audio and JPEG video tracks\n\
%s -p audio -p video,quality=75 out.mv\n\
\n\
Example: record 30 seconds of high-quality JPEG video\n\
%s -t 30 -p video,quality=90 out.mv\n\
\n\
Example: record 30 seconds of best-quality JPEG video at 14000000 bits/second\n\
%s -t 30 -p video,brate=14m out.mv\n",
		g_ProgramName,g_ProgramName,g_ProgramName);
	}
	exit(EXIT_FAILURE+1);
} /* usage */

int getSpecifiedPort
    (
    Options *options
    )
{
    VLServer server;
    VLPath path;
    VLDev device = UseThisDevice.dev;
    VLNode devNode;
    VLControlValue val;
    int videoPort;
    VLNodeInfo *node;
    int i;
    char *portName = options->videoPortName;

    server = vlOpenVideo("");
    /* Use current default input port if none specified. */
    if (portName == NULL) {
	/* Port not specified. Find default port and use it */
	/* Set up a dummy path so we can use it to find default port */
	VC( devNode=vlGetNode( server, VL_DEVICE, 0, VL_ANY ) );
	VC( path=vlCreatePath( server, device, devNode, devNode ) );
	VC( vlSetupPaths(server, &path, 1, VL_SHARE, VL_READ_ONLY) );
	VC( vlGetControl( server, path, devNode, VL_DEFAULT_SOURCE, &val) );
	options->videoPort = videoPort = val.intVal;
	vlDestroyPath(server, path);
    } else if ( (*(portName) >= '0') && (*(portName) <= '9')) {
	options->videoPort = videoPort = atoi(portName);
    } else {
	goto nameUsed;
    }

    vlCloseVideo(server);
    /* Make sure that this is indeed an appropriate port number */
    for (i = 0; i < UseThisDevice.numNodes; i++) {
	node = &(UseThisNode[i]);
	if (node->number == videoPort) {
	    options->videoPortName = node->name;
	    return videoPort;
	}
    }
    return -1;

nameUsed:
    vlCloseVideo(server);
    /* A symbolic name is used for the port specification */
    /* Look for an appropriate port that matches */
    for (i = 0; i < UseThisDevice.numNodes; i++) {
	node = &(UseThisNode[i]);
	/* ignore case and allow shorthand */
	if (strncasecmp(node->name, portName, strlen(portName)) == 0) {
	    options->videoPortName = node->name;
	    return (options->videoPort = node->number);
	}
	if (strncasecmp(node->name, "node_", 5) ==0){
		if (strncasecmp(&node->name[5], portName, strlen(portName)) == 0) {
		    options->videoPortName = node->name;
		    return (options->videoPort = node->number);
		}
	}
    }
    return -1;
} /* getSpecifiedPort */

/********
*
* processPathOptions
*
********/

void processPathOptions 
    (
    char* arg,
    Options* options
    )
{
    char* videoOpts[] = {
	#define DEVICE 0
	"device",
	#define PORT 1
	"port",
	#define TRACK 2		/* not supported yet */
	"track",		
	#define COMPRESSION 3
	"comp",
	#define ENGINE 4
	"engine",
	#define QUALITY 5
	"quality",
	#define HEIGHT 6
	"height",
	#define HALFX 7
	"halfx",
	#define HALFY 8
	"halfy",
	#define BRATE 9
	"brate",
	NULL };
    
    char* audioOpts[] = {
	#define CHANNELS 0
	"channels",
	NULL };
    
    char *value = NULL;
    
    /*
    ** check to see if it is audio or video.
    */
	
    if ( strncmp ( "audio", arg, strlen( "audio" ) ) == 0 ) {
	options->audio = 1;
	arg += strlen("audio");
	if ( *arg != ',' ) {
	    return;
	}
	arg++;	/* skip past the comma */
	while ( *arg != '\0' ) {
	    switch( getsubopt( &arg, audioOpts, &value ) ) {
	    case CHANNELS:
		if ( value == NULL ) {
		    usage(0);
		}
		options->audioChannels = atoi( value );
		if (options->audioChannels != 1 && 
		    options->audioChannels != 2) {
		    fprintf(stderr,
			    "channels = %d is out of range [1,2]", 
			    options->audioChannels);
		    fprintf(stderr, " Resetting to 2\n");
		    options->audioChannels = 2;
		}
		break;
	    default:
		/* process unknown token */
		fprintf(stderr,
			"Unknown -p (input path) suboption '%s'\n",value);
		usage(0);
		break;
	    } /* switch */
	} /* while */
	return;
    } /* if audio */
    
    else if ( strncmp ("video", arg, strlen( "video" ) != 0 ) ) {
	fprintf( stderr, "Unknown input option %s\n", arg);
	usage(0);
    }
    
    /* this is video, process all the fancy suboptions */
    options->video = 1;
    arg += strlen("video");
    if (arg == '\0') return;
    if ( *arg != ',' ) {
	fprintf( stderr, "No video options specified\n" );
	usage(0);
    }
    arg++;	/* skip past the comma */
    while ( *arg != '\0' ) {
	switch( getsubopt( &arg, videoOpts, &value ) ) {
	case DEVICE:
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for device\n" );
		usage(0);
	    }
	    options->videoDevice = strdup( value );
	    if (!strcmp(value, "impact")) {
		options->videoDrain = VL_CODEC;
		options->codecPort = -1;
	    }
	    if (!strcmp(value, "ev1")) {
		options->videoDrain = VL_VIDEO;
		options->codecPort = 2;
	    }
	    break;
	case PORT:
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for port\n" );
		usage(0);
	    }
	    options->videoPortName=strdup(value);
#if 0
	    options->videoPort = atoi( value );
	    if (!strcmp(options->videoDevice, "ev1")) {
	        if ( (options->videoPort < 0) || (options->videoPort > 2) ) {
		    fprintf( stderr, 
			"port %d is out of range [0-2]. ",
			options->videoPort);
		    fprintf( stderr, "Resetting value to 0\n");
		    options->videoPort = 0;
		}
	    }
	    if (!strcmp(options->videoDevice, "impact")) {
	        if ((options->videoPort != 0) && 
		    (options->videoPort != 1) &&
		    (options->videoPort != 4)) {
		    fprintf( stderr, 
			"invalid port %d, resetting to 4\n",
			options->videoPort);
		    options->videoPort = 4;
		}
	    }
#endif
	    break;
	case COMPRESSION :
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for compression\n" );
		usage(0);
	    }
	    options->compressionScheme = strdup( value );
	    break;
	case ENGINE :
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for engine\n" );
		usage(0);
	    }
	    options->compressionEngine = strdup( value );
	    if (!strcmp(value, "impact")) {
		options->videoDrain = VL_CODEC;
		options->codecPort = -1;
	    }
	    if (!strcmp(value, "cosmo")) {
		options->videoDrain = VL_VIDEO;
		options->codecPort = 2;
	    }
	    break;
	case QUALITY :
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for quality\n" );
		usage(0);
	    }
	    options->qualityFactor = atoi( value );
	    if (options->qualityFactor < 1 || options->qualityFactor > 100) {
		fprintf(stderr,
			"quality of %d is out of range ", 
			options->qualityFactor);
		fprintf(stderr,
			"[1,100]. Resetting quality to 75\n");
		options->qualityFactor = 75;
	    }
	    break;
	case HEIGHT:
	    if (value != NULL) {
		options->height = atoi( value );
	    }
	    break;
	case HALFX:
	    options->halfx = 1;
	    break;
	case HALFY:
	    options->halfy = 1;
	    break;
	case BRATE :
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for brate\n" );
		usage(0);
	    }
#define MAX_BIT_RATE  100000000
	    {
	    char *cp;
	    double d;

	    d = strtod(value, &cp);
	    switch(*cp) {
		case 'm':
		case 'M':
		    d *= 1000000;
		    break;
		case 'k':
		case 'K':
		    d *= 1000;
		    break;
	    }
	    options->avrFrameRate = (int)d;
	    }
	    if (options->avrFrameRate < 0 || options->avrFrameRate > MAX_BIT_RATE ) {
		fprintf(stderr,
			"Bit rate of %d is out of range. ", 
			options->avrFrameRate);
		fprintf(stderr,
			"Resetting value to zero\n");
		options->avrFrameRate = 0;
	    }
	    break;
	default:
	    /* process unknown token */
	    fprintf(stderr,
		    "Unknown -p (input path) suboption '%s'\n",value);
	    usage(0);
	    break;
	} /* switch */
    } /* while */
} /* processPathOptions */

/********
*
* processFileOptions
*
********/

void processFileOptions
    (
    char* arg,
    Options* options
    )
{
    char* myopts[] = {
	#define TITLE 0
	"title",
	NULL };

    char* value = NULL;
    while ( *arg != '\0' ) {
	switch( getsubopt( &arg, myopts, &value ) ) {
	case TITLE:
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for title\n" );
		usage(0);
	    }
	    options->movieTitle = strdup( value );
	    break;
	default:
	    /* process unknown token */
	    fprintf(stderr,
		    "Unknown -f (file option) suboption `%s'\n",value);
	    usage(0);
	    break;
	} /* switch */
    } /* while */
} /* processFileOptions */

/********
*
* parseArgs
*
********/

void parseArgs
    (
    int argc, 
    char **argv,
    Options* options
    )
{
    extern char* optarg;
    extern int   optind;
    int	         c;
    int          res;
    
    /*
    ** Parse the command line arguments.
    */
    
    while ((c = getopt(argc, argv, "B:t:vhp:f:CsS")) != EOF) {
	switch (c) {
	case 'B':
	    if ( strcmp( optarg, "auto" ) == 0 ) {
		options->batchMode = 1;
	    }
	    else if ( strcmp( optarg, "key" ) == 0 ) {
		options->batchMode = 0;
	    }
	    else {
		usage(0);
	    }
	    break;
	    
	case 'C':
	    options->critical = 1;
	    break;
	    
	case 't':
	    options->seconds = atoll(optarg);
	    if ( options->seconds <= 0 ) {
		fprintf(stderr,
			"-t opt: invalid \"seconds\" arg (%d)\n",
			options->seconds);
		usage(0);
	    }
	    break;

	case 'v':
	    options->verbose++;
	    break;
	    
	case 'f':
	    processFileOptions( optarg, options );
	    break;
	    
	case 'p':
	    processPathOptions( optarg, options );
	    break;
	    
	case 'h':	/* display the detailed usage message */
	    usage(1);
	    /*NOTREACHED*/;
	    
	case '?':
	    usage(0);
	    /*NOTREACHED*/;
        case 's':
            /* use old SGI-3 format */
            options->format = MV_FORMAT_SGI_3;
	    options->noNewInfo= 1;
            break;
	    /*NOTREACHED*/;
        case 'S':
	    options->format= MV_FORMAT_QT;
	    options->noNewInfo= 1;
            break;
	    /*NOTREACHED*/;
	    
	} /* switch */
    } /* while */
    
    if (optind == argc) {
	/* missing file name */
	fprintf(stderr, "Missing filename\n");
	usage(0);
    }
    else {
	options->fileName = argv[optind];
    }
    
    if ( options->verbose > 1 ) {
	printf("%s: args ", g_ProgramName);
	for (res = 0; res < argc; res++)
	    printf("%s ", argv[res]);
	printf("\n");
    }
}
