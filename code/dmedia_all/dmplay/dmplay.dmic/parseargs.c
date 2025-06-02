/*
 * parseargs.c: command line parser for dmplay
 *
 */
#include "dmplay.h"

#include <libgen.h> /* for basename() */
#include <getopt.h>


/*
 * Prototype definitions
 */
static void usage(int);
static void process_port_opts(char *,_Options *,int , char *);
static void check_opts(_Options *opts);

static void
usage(int verbose)
{
    char display[256];
    char *au = "[-p audio]";
    int first = 1;

    strcpy(display, "[-p graphics");
    if (HASMVP) {
	strcat(display, "|video[,device=mvp][,engine=");
    } 
    else if (HASIMPACT) {
	strcat(display, "|video[,device=impact][,engine=");
    }
    else {
	strcat(display, "[,engine=");
	au = strdup("");
    }
    if (HASVICE) {
	strcat(display, VICE);
	first = 0;
    }
    else if (HASIMPACTCOMP) {
	strcat(display, IMPACT);
        first = 0;
    }
	
    if (HASDMICSW) {
	if (!first) {
	    strcat(display, "|");
	}
	first = 0;
	strcat(display, "sw");
    }
    strcat(display, "]]");    

    
    fprintf(stderr,
	"Usage:\n"
	"dmplay -h		- Display long help \n"
	"dmplay %s %s\n"
	"     [-f] [-g] [-z n/d] [-v] [-E auto|key] [-L none|repeat] file\n",
	display, au);

    if (verbose) {
	fprintf(stderr, "\n"
	"-p\n"
	"        graphics  play movie in a graphics window");
	if (HASNEITHER)
	    fprintf(stderr, " (default)\n");
	else
	    fprintf(stderr, "\n");
	if (HASMVP || HASIMPACT) {
	    fprintf(stderr,
	"        video     play movie to video display and a graphics window (default)\n");
	}
	first = 1;
	fprintf(stderr,
	"        engine    specify decompression engine:\n"
	"                  ");
	if (HASVICE) {
	    fprintf(stderr, " %s (default)", VICE);
	    first = 0;
	}
	else if (HASIMPACTCOMP) {
	    fprintf(stderr, " %s (default)", IMPACT);
            first = 0;
        }
	if (HASDMICSW) {
	    if (!first) {
		fprintf(stderr, ",");
	    }
	    first = 0;
	    if (USEDMICSW) {
		fprintf(stderr, " sw (default)");
	    } else {
		fprintf(stderr, " sw");
	    }
	}
	fprintf(stderr, "\n"
	"\n"
	"-f      forcibly change the underlying hardware's sample rate\n"
	"-g      supress graphics display\n"
	"-z x    zoom playback window\n"
	"-v      verbose\n"
	"-E\n"
	"        auto      Program stops when playback is finished\n"
	"        key       Wait for user input to terminate program (default)\n"
	"-L\n"
	"        none(spin1)  Play the sequence once. Repeat last field at end\n"
	"        spin2     Play the sequence once. Repeat last frame at end\n"
	"        repeat    Loop through sequence from beginning to end\n\n\n");
	}

	exit(EXIT_FAILURE);
}


/*
 * Process a port option which looks like this:
 *    -p port_desc,params
 *
 * Port option could eventually look like this:
 *    -p video,device=dev:0,port=0,track=1,params...
 *
 * This would mean: display image track 1 from movie using
 * port 0 on the 0th instance of video device 'dev'.
 * 
 */
static void
process_port_opts(char *options, _Options *opts, int port_type,
                            char *progname)
{
    char *myopts[] = {
            #define DEVICE       0   /* eg  device=ev1 */
            "device",
            #define PORT         1   /* eg  port=2   not supported yet */
            "port",
            #define TRACK	 2   /* eg  track=2  not supported yet */
            "track",
            #define ENGINE       3   /* decompression engine */
            "engine",
            NULL };
    char *path;
    char *value = NULL;

    progname = progname;  /* to get the compiler warning to shut up */
    path = options;

    while ( *options != '\0') {
        switch( getsubopt( &options, myopts, &value ) ) {
            case DEVICE:
                if (value == NULL) {
                    fprintf(stderr, "No value specified for device: '%s'\n",
                                          path);                        
                    usage(0);
                }
                if (port_type != V_PORT) {
                    fprintf(stderr, "Invalid device '%s'\n", value);
                    usage(0);
                }
		if (opts->vid_device == NULL) {
		    fprintf(stderr, "May not specify device with '-p graphics'\n");
		    usage(0);
		}
		if (strcasecmp(value, opts->vid_device) != 0) {
		    fprintf(stderr,"Invalid video device '%s'; must use '%s'\n",
			value, opts->vid_device);
		    usage(0);
		}
                break;

            case ENGINE:
                if (value == NULL) {
                    fprintf(stderr, "No value specified for engine: '%s'\n",
                               path);
                    usage(0);
                }
                if (port_type == A_PORT) {
                    fprintf(stderr, "Invalid engine argument: '%s'\n", path);
                    usage(0);
                }
		if ((strcasecmp(value, VICE) == 0) && HASVICE) {
		    UseThisdmICDec = hasViceDec;
		    useEngine = 1;
		    dmID.speed = DM_IC_SPEED_REALTIME;
		} else if ((strcasecmp(value, DMICSW) == 0) && HASDMICSW) {
		    UseThisdmICDec = hasdmICSWDec;
		    useEngine = 2;
		    dmID.speed = DM_IC_SPEED_NONREALTIME;
		} else if ((strcasecmp(value, IMPACT) == 0) && HASIMPACTCOMP) {
		    UseThisdmICDec = hasImpactDec;
		    useEngine = 3;
		    dmID.speed = DM_IC_SPEED_REALTIME;
		} else {
		    fprintf(stderr, "Specified engine '%s' is not available\n",
			 value);
		    usage(0);
		}
                opts->image_engine = strdup(value);
                break;

            default:
                /* process unknown token */
                fprintf(stderr, "Unknown display path suboption '%s'\n",
                                      value);
                usage(0);
                break;
        }
    }
}

/*
 * parseargs: parses the command line options from the user.
 */
void parseargs (int argc, char *argv[]) 
{
    int ch;

    _Options *opts = &options;


    if (argc < 2) {
	usage(0);
    }

    while ((ch = getopt (argc, argv, "fhz:vE:P:L:p:I:O:MTsY:V:A:gH")) != EOF) {
	switch (ch) {
            case 'p':
                opts->use_default_ports = 0;
                if (!strncmp( optarg, "audio", strlen("audio"))) {
                    if (opts->audio_port_specified) {
                        fprintf(stderr, 
                          "Please specify only one audio port option.\n");
                        usage(0);
                    } else {
                        opts->audio_port_specified = 1;
                    }
                    opts->playAudioIfPresent = 1; 
                    while (*optarg != ',' && *optarg != '\0') optarg++;
                    if (*optarg != '\0') {
                        optarg++; /* skip comma */
                        process_port_opts( optarg ,opts, A_PORT, argv[0]);
                    }
                } else if (!strncmp( optarg, "video", strlen("video"))) {
		    if (HASNEITHER) {
			fprintf(stderr, "Invalid '-p video'; only '-p graphics' is allowed on this machine\n");
			usage(0);
		    }
                    if (opts->display_port_specified) {
                        fprintf(stderr, 
                          "Please specify only one display port option.\n");
                        usage(0);
                    } else {
                        opts->display_port_specified = 1;
                    }
		    image.display = VIDEO_2_FIELD;
                    while (*optarg != ',' && *optarg != '\0') optarg++;
                    if (*optarg != '\0') {
                        optarg++; /* skip comma */
                        process_port_opts( optarg ,opts, V_PORT, argv[0]);
                    }
                }
                else if (!strncmp( optarg, "graphics", strlen("graphics"))) {
                    if (opts->display_port_specified) {
                        fprintf(stderr, 
                          "Please specify only one display port option.\n");
                        usage(0);
                    } else {
                        opts->display_port_specified = 1;
                    }
		    image.display = GRAPHICS;
		    opts->vid_device = NULL;
		    useVideoDevice = 0;
		    opts->videoOnly = 0;
                    while (*optarg != ',' && *optarg != '\0') optarg++;
                    if (*optarg != '\0') {
                        optarg++; /* skip comma */
                        process_port_opts( optarg ,opts, G_PORT, argv[0]);
                    }
                }
		else if (!strncmp(optarg, "none", strlen("none"))) {
		    image.display = NO_DISPLAY;
		}
                else {
                    fprintf(stderr, "Unrecognized port type '%s'\n", optarg);
                    usage(0);
                }
	        break;

            case 'E':
                if (!strcmp(optarg, "auto") ) {
                    opts->autostop = 1;
                }
                else if (!strcmp(optarg, "key") ) {
                    opts->autostop = 0;
                }
                else {
                    fprintf(stderr, "Unknown stop mode '%s'\n", optarg);
                    usage(0);
                }
                break;

            case 'P':
                if (!strcmp(optarg, "cont") ) {
                    opts->initialPlayMode=PLAY_CONT;
                }
                else if (!strcmp(optarg, "step") ) {
                    opts->initialPlayMode= PLAY_STEP;
                }
                else {
                    fprintf(stderr, "Unknown play mode '%s'\n", optarg);
                    usage(0);
                }
                break;

	    case 'L':
                if (!strcmp(optarg, "repeat") ) 
		    opts->initialLoopMode = LOOP_REPEAT;
		else if (!strcmp(optarg, "none") || !strcmp(optarg, "spin1") ) 
		    opts->initialLoopMode = LOOP_NONE;
		else if (!strcmp(optarg, "spin2") )
		    opts->initialLoopMode = LOOP_FRAME;
		else {
                    fprintf(stderr, "Unknown loop mode '%s'\n", optarg);
                    usage(0);
                }
	        break;

            case 'z':
		{
		int denominator;
		char *p;

		opts->zoom = atof(optarg);

		if (p = strchr(optarg,'/')) {
		    denominator = atoi(p+1);
		    if (!denominator) {
			fprintf(stderr,"badly formed zoom argument\n");
			usage(0);
		    }
		    opts->zoom /= denominator; 
		}

		if (opts->zoom == 0)
		    opts->zoom = 1;
		}
                break;
                
	    case 'v':
                opts->verbose++;
                break;

	    case 'h':
		usage (1);
		break;

	    case 'I':
		/* number of buffers in input pool */
		dmID.inbufs = atoi(optarg);
		break;
	    case 'O':
		/* number of buffers in output pool */
		dmID.outbufs = atoi(optarg);
		break;
	    case 'M':
		/* mute: no audio */
		opts->playAudioIfPresent = 0;
		break;
	    case 'T':
		/* print timing information after playing */
		playstate.timing = 1;
		break;
	    case 's':
		/* Turn off speed control, which is normally used  to
		 * keep graphics display from running too fast. Has
		 * no effects if playing to video.
		 */
		playstate.speedcontrol = 0;
		break;
	    case 'Y':
		/* how often to check sync: in frames */
		playstate.syncInterval = atoi(optarg);
		break;
	    case 'V':
		/* number of video preroll frames */
	 	video.preroll = atoi(optarg);
		break;
	    case 'A':
		/* number of audio preroll frames */
		audio.preroll = atoi(optarg);
		break;
	    case 'f':
		/* Force sample rate to correct value */
		opts->forceSampleRate = 1;
		break;
	    case 'g':
		/* video only, no graphics */
		opts->videoOnly = 1;
		break;
	    case 'H':
		/* set non-degradable high-priority */
		opts->hiPriority = 1;
		break;

	    case '?':
	        usage(0);
	        break;

	    default:
	        break;
	}
    }

    if (optind == argc) {
        fprintf(stderr, "No input file specified\n");
        usage(0);
    }
    if (optind < argc-1) {
        fprintf(stderr, "Multiple file arguments not supported\n");
        usage(0);
    }
    opts->ifilename = strdup(argv[optind]);
 
    check_opts(opts);

}

/*
 * check the command-line options for consistency
 * set the global control variables for the program
 */
static void 
check_opts(_Options *opts)
{

    if (opts->use_default_ports) {
        if (opts->verbose) {
            fprintf(stderr, "No port (-p) options specified: using defaults\n");
        }
    }
    if ((dmID.speed != DM_IC_SPEED_REALTIME) || (opts->initialPlayMode == PLAY_STEP)) {
	opts->playAudioIfPresent = 0;
	if (opts->verbose) {
	    fprintf(stderr,"Audio playback disabled when not playing back in real-time.\n");
	}
    }

    if ((opts->initialPlayMode==PLAY_STEP) && (opts->playAudioIfPresent)) {
	opts->playAudioIfPresent = 0;
        fprintf(stderr,"Audio playback disabled during single-step playback\n");
    }

    if (opts->audio_port_specified) {
	if (USEGFX) {
	    fprintf(stderr, "'-p audio' cannot be used with display on graphics\n");
	    usage(0);
	}

	/* Software non-realtime decoding */
	if (UseThisdmICDec == hasdmICSWDec) {
	    fprintf(stderr,"'-p audio' may only be used with non-sw engine\n");
	    usage(0);
	}
    }

    if ((image.display == VIDEO_2_FIELD) && !USEVICE && !USEIMPACT) {
        fprintf(stderr, "Must use 'engine=%s' when display is 'video'\n", 
		HASMVP?VICE:IMPACT);
        usage(0);
    }

    if (opts->verbose) {
        printf("input file: %s\n", opts->ifilename);
        printf("Initial playback options:\n");
        printf("	looping      = %s\n", 
                         opts->initialLoopMode==LOOP_REPEAT?"repeat":
	       opts->initialLoopMode==LOOP_FRAME?"spin2":"none(spin1)");
        printf("	play mode    = %s\n", opts->initialPlayMode==PLAY_STEP
                                         ?"single-step": "continuous");
        printf("Display port:\n");
        printf("	display      = %s\n", USEGFX ? "graphics" : "video");
        printf("	device       = %s\n", HASMVP?MVP:IMPACT);
	printf("	engine	     = %s\n", USEVICE?VICE:(USEIMPACT?IMPACT:DMICSW));
        printf("Audio port:\n");
        printf("	%s if soundtrack present\n", 
                         opts->playAudioIfPresent?"enable playback":"mute");
    }

    if (playstate.syncInterval < 10) {
	playstate.syncInterval = 10;
    }

    movie.filename = strdup(opts->ifilename);
}
