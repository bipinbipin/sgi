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
	int i;
	VLNodeInfo *p;

	fprintf(stderr, "Usage: \n\
dmrecord -h  : print help message\n\n\
dmrecord [-v] [-B auto|key] [-C] [-t secs] [-p video_port|screen_port]\n\
	 [-p audio_port] [-f file_opt] [-s] [-2] FILE\n");

	if (verbose) {
		fprintf(stderr, "\n\
-p video_port   : -p video[,device=%s][,port=P][,comp=jpeg][,engine=%s]\n\
                     [,quality=Q][,brate=N]\n\
		  Recording of video is always assumed unless screen_port\n\
		  is specified.\n",
		     UseThisDevice.name, UseThisEngine);
		fprintf(stderr, "\
        port    : Port number or name (which may be abbreviated, e.g,\n\
		    svid for 'Svideo Input'):\n");
		for (i = 0, p = UseThisNode; i < UseThisDevice.numNodes; i++, p++) {
		    fprintf(stderr, "\
		  '%s' or %d\n", p->name, p->number);
		}
		fprintf(stderr, "\
		  If omitted, default port (which can be selected using\n\
		  the VideoPanel) is used.\n\
        quality : JPEG quality factor from 1-100 (default=75)\n\
	brate	: compression bit rate in bits per second (if omitted, record\n\
		    with no rate control\n\n");
		fprintf(stderr, "\
-p screen_port  : -p screen,x=X,y=Y,width=W,height=H,timing=T\n\
		  Capture the rectangular area of screen with (X,Y) as its \n\
		  upper left-hand corner, width W and height H. The timing\n\
		  setting must be either ntsc or pal.\n\
		  NOTE: Be sure to use the command 'xscreen' to set the \n\
		  monitor's timing to 60Hz for ntsc or 50Hz for pal before\n\
		  you use dmrecord to do screen capture.\n\n");
		fprintf(stderr, "\
-p audio_port   : -p audio[,channels=C]\n\
                  Enable recording from audio input port\n\
                    (use Audio Control Panel to select\n\
                    source={mic,line,digital} and rate)\n\
        channels: 1 for mono, 2 for stereo\n\
\n\
-v      verbose\n\
-B      begin-recording mode\n\
            auto: Recording begins as soon as possible\n\
            key : Recording begins when user hits a key (default)\n\
-C      critical: Abort recording, delete FILE if any video frames are dropped\n\
-t      Recording duration in seconds (if omitted, record until <ctrl>-c)\n\
-f      file_opt: -f [title=movie_name]\n\
-s	Use old movie format (SGI-3). Default is to use the new format\n\
	(QuickTime). Old dmplay cannot play movies in the new format.\n\
-a      Use AVI movie format.\n\
-2	Input stream is F2 dominant. F1 dominance is assumed by default.\n");

		fprintf(stderr, "\n\
Example: record synchronized audio and video tracks using default video port\n\
dmrecord -p audio  out.mv\n\
\n\
Example: record synchronized audio with the upper left-hand section of screen\n\
	 (width=640 pixels, height=480 pixels) as an ntsc movie\n\
dmrecord -p screen,x=0,y=0,width=640,height=480,timing=ntsc -p audio out.mv\n\
\n\
Example: record synchronized audio and video tracks using video port 1\n\
dmrecord -p audio  -p video,port=1 out.mv\n\
\n\
Example: record 30 seconds of high-quality video using default video port\n\
dmrecord -t 30 -p video,quality=90 out.mv\n\
\n\
Example: record 30 seconds of best-quality video at 14000000 bits/second\n\
dmrecord -t 30 -p video,brate=14m out.mv\n");
	}
	exit(EXIT_FAILURE+1);
} /* usage */

/********
*
* processPathOptions
*
********/

void processPathOptions 
    (
    char* arg
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

    char *screenOpts[] = {
	#define SCREENX 0
	"x",
	#define SCREENY 1
	"y",
	#define SCREENWIDTH 2
	"width",
	#define SCREENHEIGHT 3
	"height",
	#define SCREENTIMING 4
	"timing",
	NULL};

    
    char *value = NULL;
    char *p;
    
    /*
    ** check to see if it is audio or video.
    */
	
    if ( strncmp ( "audio", arg, strlen( "audio" ) ) == 0 ) {
	options.audio = 1;
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
		options.audioChannels = atoi( value );
		if (options.audioChannels != 1 && 
		    options.audioChannels != 2) {
		    fprintf(stderr,
			    "channels = %d is out of range [1,2]", 
			    options.audioChannels);
		    fprintf(stderr, " Resetting to 2\n");
		    options.audioChannels = 2;
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
    else if (strncmp("screen", arg, strlen("screen")) == 0) {
	options.nodeKind = VL_SCREEN;
	options.videoPortName = screenPortName;
	options.videoPortNum = screenPortNum;
	video.originX = -1;
	video.originY = -1;
	video.width = -1;
	video.height = -1;
	video.rate = -1.0;
	arg += strlen("screen");
	if (*arg++ != ',') {
	    fprintf(stderr, "Error in screen_port specification\n");
	    usage(0);
	}
	while (*arg != '\0') {
	    switch (getsubopt(&arg, screenOpts, &value)) {
	    case SCREENX:
		if (value == NULL) {
		    fprintf(stderr, "No value for x in screen_port\n");
		    usage(0);
		}
		video.originX = atoi(value);
		break;
	    case SCREENY:
		if (value == NULL) {
		    fprintf(stderr, "No value for y in screen_port\n");
		    usage(0);
		}
		video.originY = atoi(value);
		break;
	    case SCREENWIDTH:
		if (value == NULL) {
		    fprintf(stderr, "No value for width in screen_port\n");
		    usage(0);
		}
		video.width = atoi(value);
		break;
	    case SCREENHEIGHT:
		if (value == NULL) {
		    fprintf(stderr, "No value for height in screen_port\n");
		    usage(0);
		}
		video.height = atoi(value);
		break;
	    case SCREENTIMING:
		if (value == NULL) {
		    fprintf(stderr, "No value for timing in screen_port\n");
		    usage(0);
		}
		if (strncasecmp("ntsc", value, strlen("ntsc")) == 0) {
		    video.rate = 29.97;
		} else if (strncasecmp("pal", value, strlen("pal")) == 0) {
		    video.rate = 25.0;
		} else {
		    fprintf(stderr,"Illegal value for timing in screen_port\n");
		    usage(0);
		}
		break;
	    default:
		fprintf(stderr, "Error in screen_port specification\n");
		usage(0);
	    }
	}
	if ((video.originX == -1) || (video.originY == -1) ||
	     (video.width == -1) || (video.height == -1) || (video.rate == -1.0)) {
	    fprintf(stderr, "All 5 parameters (x, y, width, height, timing) must be specified in screen_port\n");
	    usage(0);
	}
	return;
    }
    else if ( strncmp ("video", arg, strlen( "video" ) != 0 ) ) {
	fprintf( stderr, "Unknown input option %s\n", arg);
	usage(0);
    }
    
    /* this is video, process all the fancy suboptions */
    options.video = 1;
    arg += strlen("video");
    if (*arg == '\0')
	return;
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
	    options.videoDevice = strdup( value );
	    break;
	case PORT:
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for port\n" );
		usage(0);
	    }
	    options.videoPortName = strdup(value);
	    break;
	case COMPRESSION :
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for compression\n" );
		usage(0);
	    }
	    options.compressionScheme = strdup( value );
	    break;
	case ENGINE :
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for engine\n" );
		usage(0);
	    }
	    options.compressionEngine = strdup( value );
	    break;
	case QUALITY :
	    if ( value == NULL ) {
		fprintf( stderr, 
			"No value specified for quality\n" );
		usage(0);
	    }
	    options.qualityFactor = atoi( value );
	    if (options.qualityFactor < 1 || options.qualityFactor > 100) {
		fprintf(stderr,
			"quality of %d is out of range ", 
			options.qualityFactor);
		fprintf(stderr,
			"[1,100]. Resetting quality to 75\n");
		options.qualityFactor = 75;
	    }
	    break;
	case HEIGHT:
	    if (value != NULL) {
		options.height = atoi( value );
	    }
	    break;
	case HALFX:
	    options.halfx = 1;
	    break;
	case HALFY:
	    options.halfy = 1;
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
	    options.avrFrameRate = (int)d;
	    }
	    if (options.avrFrameRate < 0 || options.avrFrameRate > MAX_BIT_RATE ) {
		fprintf(stderr,
			"Bit rate of %d is out of range. ", 
			options.avrFrameRate);
		fprintf(stderr,
			"Resetting value to zero\n");
		options.avrFrameRate = 0;
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
    char* arg
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
	    options.movieTitle = strdup( value );
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
    char **argv
    )
{
    extern char* optarg;
    extern int   optind;
    int	         c;
    int          res;
    int		it;
    
    /*
    ** Parse the command line arguments.
    */
    
    while ((c = getopt(argc, argv, "B:t:vhp:f:aCs2#:U:I:O:NTH")) != EOF) {
	switch (c) {
	case 'B':
	    if ( strcmp( optarg, "auto" ) == 0 ) {
		options.batchMode = 1;
	    }
	    else if ( strcmp( optarg, "key" ) == 0 ) {
		options.batchMode = 0;
	    }
	    else {
		usage(0);
	    }
	    break;
	    
	case 'C':
	    options.critical = 1;
	    break;
	    
	case 't':
	    options.seconds = atoll(optarg);
	    if ( options.seconds <= 0 ) {
		fprintf(stderr,
			"-t opt: invalid \"seconds\" arg (%d)\n",
			options.seconds);
		usage(0);
	    }
	    break;

	case 'v':
	    options.verbose++;
	    break;
	    
	case 'f':
	    processFileOptions( optarg);
	    break;
	    
	case 'p':
	    processPathOptions( optarg);
	    break;

	case '3':
	    options.use30000 = 1;
	    break;
	    
	case 'h':	/* display the detailed usage message */
	    usage(1);
	    /*NOTREACHED*/;
	    
	case '?':
	    usage(0);
	    /*NOTREACHED*/;

	case 's':
	    /* use old SGI-3 format */
            if (movie.format != MV_FORMAT_AVI)
            {
	        movie.format = MV_FORMAT_SGI_3;
            }
            else
            {
                fprintf(stderr, "Ignoring -s option after -a.  Movie will be AVI format\n");
            }
	    break;

	case 'a':
	    /* use old AVI format */
            if (movie.format != MV_FORMAT_SGI_3)
            {
	        movie.format = MV_FORMAT_AVI;
                options.unit = UNIT_FRAME;
            }
            else
            {
                fprintf(stderr, "Ignoring -a option after -s.  Movie will be SGI format\n");
            }
	    break;

	case '2':
	    /* make it f1 dominant */
	    video.fdominance = 2;
	    break;

	case '#':
	    /* number of frames to capture */
	    options.frames = atoi(optarg);
	    break;

	case 'U':
	    /* select frame movie or field movie */
	    if ( strcmp( optarg, "frame" ) == 0 ) {
		options.unit = UNIT_FRAME;
	    } else if (strcmp(optarg, "field") == 0 && movie.format != MV_FORMAT_AVI) {
		options.unit = UNIT_FIELD;
	    } else {
		fprintf(stderr,"-U %s : illegal parameter, ignored\n", optarg);
	    }
	    break;
	case 'I':
	    /* number of buffers in input pool */
	    options.inbufs = atoi(optarg);
	    break;
	case 'O':
	    /* number of buffers in output pool */
	    options.outbufs = atoi(optarg);
	    break;
	case 'N':
	    /* don't write to file */
	    options.nowrite = 1;
	    break;
	case 'T':
	    /* gather and print timing information */
	    options.timing = 1;
	    break;
	case 'H':
	    /* set non-degradable high priority */
	    options.hiPriority = 1;
	    break;
	    
	} /* switch */
    } /* while */
    
    if (optind == argc) {
	/* missing file name */
	fprintf(stderr, "Missing filename\n");
	usage(0);
    }
    else {
	options.fileName = argv[optind];
    }
    
    if ( options.verbose > 1 ) {
	printf("args ");
	for (res = 0; res < argc; res++)
	    printf("%s ", argv[res]);
	printf("\n");
    }
}
