/**********************************************************************
*
* File: main.c
*
* JPEG movie capture program.
*
**********************************************************************/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <libgen.h>		/* for basename() */
#include <limits.h>		/* for ULONGLONG_MAX */
#include <unistd.h>		/* for setreuid(), etc. */
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <sys/time.h>
#include <dmedia/dmedia.h>
#include <dmedia/moviefile.h>
#include <dmedia/vl.h>

#include "dmrecord.h"
#include "handy.h"

extern void setscheduling (int hiPriority);


/*
**	global variables
**
*/

char* g_ProgramName;
static Options options;
static volatile Boolean 	doneFlag = FALSE;

/*
**
**	Signal catcher
**
*/
static void
sigint()
{
	doneFlag = 1;
}

/*
**	This following two are callback functions. They are defined here but 
**	called by the modules that do the real work. 
**
**	done(int thisFrame) 
**	will be called once for each iteration of the main loop in the module.
**	It returns 1 if it is time to break out of the main loop.
*/

int
done(void)
{

	if ( doneFlag ) {
		return doneFlag;
	}

	return 0;
}

/*
**	goAhead() will be called by the modules after they have done the initial
**	setup but before they enter the main loop. goAhead() will block until it
**	time to actually begin recording.
*/

void
goAhead(void)
{
	if ( !options.batchMode) {
		printf ("\nHit the Enter key to begin recording... ");
		fflush(stdout);
		getchar();
	}

	if ( !options.seconds) {	/* record till user key stroke or <ctrl>-c. */
		printf("Hit <ctrl>-c to stop recording...\n");
	}
	signal(SIGINT, sigint);
} /* goAhead */

/*
 * Check the video device, its attached nodes,
 * and the compressor anhe results in UseThisDevice,
 * UseThisNode, UseThisdStatus, CosmoStatus, usedmIC,
 * UseThisEngine, and useMVP.
 */
int useEV1;
int useIMPACT;
VLNodeInfo UseThisNode[MAXNODES];
VLDevice   UseThisDevice;


void setDefaults
    (
    Options *options
    )
{
    VLServer	server;
    int		devNum;
    VLDevList	devlist;
    VLDevice	*cdevice;
    VLNodeInfo	*node;
    int		i;
    int		numGoodNodes;

    options->height 		= 0;
    options->videoDevice	= NULL;
    options->critical 		= FALSE;
    options->verbose 		= FALSE;
    options->fileName 		= NULL;
    options->movieTitle 	= NULL;
    options->video 		= FALSE;
    options->audio 		= FALSE;
    options->audioChannels 	= 2;
    options->seconds 		= 0;
    options->batchMode		= FALSE;
    options->compressionScheme	= FALSE;
    options->compressionEngine	= FALSE;
    options->qualityFactor	= 75;
    options->halfx		= 0;
    options->halfy		= 0;
    options->avrFrameRate	= 0;
    options->videoPortName 	= NULL;
    options->format=		MV_FORMAT_QT;
    options->noNewInfo=		0;
#if 0
    options->nodeKind		= VL_VIDEO;
    options->frames		= 0;
#endif


    UseThisDevice.numNodes = -1; /* mark as no input device found (yet) */
    server = vlOpenVideo("");
    /* Get a list of all video devices */
    vlGetDeviceList(server, &devlist);

    for (devNum = 0; devNum < devlist.numDevices; devNum++) {
	cdevice = &(devlist.devices[devNum]);
	if (strcmp(cdevice->name, "impact") == 0) {
	    useIMPACT = 1;
	    goto devFound;
	}
    }
    for (devNum = 0; devNum < devlist.numDevices; devNum++) {
	cdevice = &(devlist.devices[devNum]);
	if (strcmp(cdevice->name, "ev1") == 0) {
	    useEV1 = 1;
	    goto devFound;
	}
    }
    vlCloseVideo(server);
    return;

devFound:
    if (options->verbose > 1)
	printf("Available ports for video input device %s:\n", cdevice->name);
    UseThisDevice.dev = cdevice->dev;
    memcpy(UseThisDevice.name, cdevice->name, sizeof(UseThisDevice.name));
    UseThisDevice.numNodes = 0;
    UseThisDevice.nodes  = UseThisNode;

    numGoodNodes = 0;
    /* Find all available input nodes for the specified kind */
    for (i = 0; i < cdevice->numNodes; i++) {
	node = &(cdevice->nodes[i]);
	if ((useEV1 == 1) && (node->number == 2)) {
	/* port #2 is not supported for this program if using ev1 */
		continue;
	}
	if ((useIMPACT==1) &&(node->number == 2)){
	/* dual link is not supported for this program on impact */
		continue;
	}
	if ((node->type == VL_SRC) && (node->kind == VL_VIDEO)) {
	    UseThisNode[numGoodNodes] = *node;
	    if (options->verbose > 1)
		printf("   port=%s' (%d)\n", UseThisNode[numGoodNodes].name, UseThisNode[numGoodNodes].number);
	    if (++numGoodNodes >= MAXNODES)
		break;
	}
    }
    UseThisDevice.numNodes = numGoodNodes;
    vlCloseVideo(server);
    return;
} /* checkDevicePortsComp */

/********
*
* main
*
********/

void main
    (
    int argc, 
    char** argv
    )
{
    int i;
    /*
    ** Save the program name to use in error messages.
    */
    
    g_ProgramName = basename( argv[0] );

    /*
    ** Set high non-degrading priority for realtime
    */
    
    setscheduling( 1 );

    /*
    ** switch to running as joe user (instead of root) if run setuid 
    */
	
    setreuid( getuid(), getuid() );
    setregid( getgid(), getgid() );
    
    /*
    ** Set the default options.
    */
    setDefaults(&options);

#if 0
    options.height 		= 0;
    options.videoDevice		= NULL;
    options.videoPort 		= VL_ANY;
    options.critical 		= FALSE;
    options.verbose 		= FALSE;
    options.fileName 		= NULL;
    options.movieTitle 		= NULL;
    options.video 		= FALSE;
    options.audio 		= FALSE;
    options.audioChannels 	= 2;
    options.seconds 		= 0;
    options.batchMode		= FALSE;
    options.compressionScheme	= FALSE;
    options.compressionEngine	= FALSE;
    options.qualityFactor	= 75;
    options.halfx		= 0;
    options.halfy		= 0;
#endif

    /*
    ** Use the command-line arguments to override the defaults.
    */

    parseArgs( argc, argv, &options );
    
    /*
    ** check if every thing has been correctly specified.
    */
	
#if 0
    if ( ! options.video ) {
	fprintf (stderr, "Video path not specified\n" );
	usage(0);
    }
#endif
    
    if ( options.videoDevice == NULL ) {
	if (options.compressionEngine == NULL){
	    if (useIMPACT)
		    options.videoDevice = strdup("impact");
	    else if (useEV1)
		    options.videoDevice = strdup("ev1");
	    else {
		fprintf (stderr, "Video path not specified\n" );
		usage(0);
	    }

	}
	else if (!strcmp(options.compressionEngine, "impact"))
	    options.videoDevice = strdup("impact");
	else if (!strcmp(options.compressionEngine, "cosmo"))
	    options.videoDevice = strdup("ev1");
	if ( options.verbose > 1) {
	    printf("Using default video device:  %s\n", options.videoDevice);
	}
    }
    
    if ( options.videoDevice ){
	if ((useIMPACT) && (strcmp ( options.videoDevice, "impact" )==0)){
	}
	else if ((useEV1) && strcmp ( options.videoDevice, "ev1" )==0){
	} else{
	fprintf(stderr,"Unsupported video device '%s'\n",options.videoDevice);
	usage(0);
     }
    }
    
    if ( ! options.compressionScheme ) {
	/* if compression scheme not specified, default to jpeg */
	options.compressionScheme = strdup("jpeg"); 
	if ( options.verbose > 1) {
	    printf("Using default video compression scheme:  jpeg\n");
	}
    }
    
    if ( options.compressionScheme && 
	 strcmp( options.compressionScheme, "jpeg" ) !=0 ) {
	fprintf ( stderr, "Unsupported compression scheme '%s'\n",
			options.compressionScheme);
	usage(0);
    }
    
    if ( ! options.compressionEngine ) {
	if (!strcmp(options.videoDevice, "impact"))
	    options.compressionEngine = strdup("impact"); 
	else if (!strcmp(options.videoDevice, "ev1"))
	    options.compressionEngine = strdup("cosmo"); 
	if ( options.verbose > 1)
	    printf("Using default video compression engine:  %s\n", 
		options.compressionEngine);
    }
    
    if ( options.compressionEngine){
	 if (useIMPACT && strcmp( options.compressionEngine, "impact" ) == 0 ){
	 } else if (useEV1 && strcmp(options.compressionEngine,"cosmo" )== 0 ) {
	 } else {
	fprintf ( stderr, "Unsupported compression engine '%s'\n",
		  options.compressionEngine);
	usage(0);
	}
    }

    if (!strcmp(options.compressionEngine, "impact")) {
	options.videoDrain = VL_CODEC;
	options.codecPort = -1;
    } else if (!strcmp(options.compressionEngine, "cosmo")) {
	options.videoDrain = VL_VIDEO;
	options.codecPort = 2;
    }
    if (getSpecifiedPort(&options) == -1) {
        fprintf(stderr, "Unsupported input video port \"%s\"\n", options.videoPortName);
	fprintf(stderr,"Supported ports are:\n");
	for(i=0;i<UseThisDevice.numNodes; i++){
		fprintf(stderr,"\"%s\" ",UseThisNode[i].name);
	}
	fprintf(stderr,"\n");
        usage(0);
    }

    
    if ( options.verbose ) {
	printf( "Options:\n");
	if ( options.audio )
	    printf("        Audio channels: %d\n", options.audioChannels);
	/*if ( options.video ) */
	    printf("          Video device: %s\n"
		   "          Input port  : '%s' (%d)\n"
		   "    Compression scheme: %s\n"
		   "    Compression engine: %s\n",
		   options.videoDevice, 
		   options.videoPortName,options.videoPort,
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
    
    capture( &options );
    
    exit( EXIT_SUCCESS );
} /* main */

