/*
 * Program: Alpha from the Impact screen example.
 *
 * This program is an example blending with Alpha from
 * the screen node and using the screen Alpha.
 *
 * The Impact Video Blender is the only way to get the
 * screen alpha into the Impact Video hardware.  This
 * program is an example of extracting the screen
 * Pixel and Alpha from an area of the screen.
 *
 * The pixel portion of the data is sent to Vout 1
 * and the alpha portion is sent to Vout 2.  This
 * is not a dual link example!  The video output
 * are locked, but they are not a dual link.
 */

/*
 * System Include Files
 */
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

/*
 * Include files
 */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <Xm/MwmUtil.h>

/*
 * Video Include files
 */
#include <dmedia/vl.h>
#include <dmedia/vl_mgv.h>
#include <dmedia/vl_impact.h>

/* 
 * GL include files
 */
#include <gl/gl.h>
#include <gl/device.h>

/*
 *  Function prototypes
 */
void	docleanup(void);
void    usage(void);

/*
 * Globals for this program.
 *  ( static to control the name space if this example is )
 *  ( ever used for other code.                           )
 */
static VLServer		vlSvr = NULL;
static VLPath		vlPath = -1;
static VLDevList	devlist;
static VLNode		src_scr;
static VLNode		drn_vid0;
static VLNode		drn_vid1;
static VLNode		blend_node;

static char		*_progname;
static int		devicenum = -1;
static int		debug = 0;
/*
 * Macros
 */
#define Debug	if (debug) printf

/*
 * Usage String:
 */
#define USAGE \
"%s: [-f] [-d] [-I] [-h]\n\
\t-f\tenable forking\n\
\t-d\tdebug mode\n\
\t-I\tprint node and path IDs for command line interface users\n\
\t-h\tthis help message\n"

/*
 * Command line help message
 */
void 
usage(void)
{
    fprintf(stderr, USAGE, _progname);
}

/*
 * The Simple program that send the screen to video out.
 */
main(int argc, char **argv)
{
    char ch;
    char *winname = NULL;
    int opterr = 0;
    int c,i;
    int dev;
    int ret;
    int fork_flag = 0;
    int x, y;
    int print_ids = 0;
    VLControlValue val;
    
    /*
     * Remember our command line name for error messages
     */
    _progname = argv[0];

    /*
     * Parse command line options
     * -f   enable forking
     * -d   set debugging
     * -I   print node and path IDs
     * -h   display help message
     */
    while ((c = getopt(argc, argv, "fdIh")) != EOF) 
    {
	switch(c) 
	{
	    case 'f':
		fork_flag = 1;
	    break;
	    
	    case 'd':
		debug++;
	    break;
	    
	    case 'I':
		print_ids = 1;
	    break;
	    
	    case 'h':
		usage();
		exit(0);
	}
    }

    if (opterr) 
    {
    	usage();
	exit(1);
    }

    /* Run in background if forking option set */
    if (fork_flag) 
    {
	ret = fork();
	switch (ret) 
	{
	    case 0:
	    break;
	    
	    case -1:
		fprintf(stderr, "%s: can't fork\n");
		exit(1);
	    break;
	    
	    default:
		exit(0);
	    break;
	}
    }

    /*
     * Connect to the daemon
     */
    if (!(vlSvr = vlOpenVideo(""))) 
    {
	printf("%s: opening video: %s\n",_progname,vlStrError(vlGetErrno()));
	exit(1);
    }

    /*
     * Get the list of devices the daemon supports
     */
    if (vlGetDeviceList(vlSvr, &devlist) < 0) 
    {
	printf("%s: getting device list: %s\n",
		_progname,vlStrError(vlGetErrno()));
	exit(1);
    }

   /*
    * Find Impact Device Number ( only one in a system )
    */

    for (i=0; i<devlist.numDevices; i++)
    {
            if (strcmp(devlist.devices[i].name,"impact") == 0)
                devicenum = i;
    }

    /*
     * No Impact Device found.
     */
    if ( -1 == devicenum )
    {
      printf("This program requires 'impact' hardware.\a\n");
      exit(1);
    }

    /*
     * Setup Drain nodes.
     */
    drn_vid0 = vlGetNode(vlSvr, VL_DRN, VL_VIDEO, VL_MGV_NODE_NUMBER_VIDEO_1);
    drn_vid1 = vlGetNode(vlSvr, VL_DRN, VL_VIDEO, VL_MGV_NODE_NUMBER_VIDEO_2);
    
    /*
     * Setup drain nodes on the screen and video.
     */
    src_scr = vlGetNode(vlSvr, VL_SRC, VL_SCREEN, VL_ANY);
    
    /*
     * Create a Blend node.
     */

    blend_node = vlGetNode(vlSvr, VL_INTERNAL, VL_BLENDER, VL_MGV_NODE_NUMBER_BLENDER);

    /*
     * Mark the Path as OK
     */
    ret = 0;

    /*
     * Establish a path between the screen source and video
     * drain. Then add a video blend node and a screen drain
     * node. All of the nodes are on this path.
     */

    vlPath = vlCreatePath(vlSvr, VL_ANY, VL_ANY, VL_ANY);

    if(vlPath == -1)
    {
	/*
         * Device doesn't support this path, these nodes.
	 */
	    vlDestroyPath(vlSvr, vlPath);
	    Debug("Can not Create Path\n");
            ret = -1;
    } 
    /*
     * Add the video screen source nodes.
     */

	if (!ret) ret = vlAddNode(vlSvr, vlPath, src_scr);
	if(ret)
	{
	    /*
             * Device doesn't support this path, these nodes.
	     */
	    vlDestroyPath(vlSvr, vlPath);
	    vlPath = -1;
	    Debug("Can not add src_scr Node\n");
	} 

	if (!ret) ret = vlAddNode(vlSvr, vlPath, drn_vid0);
	if(ret)
	{
	    /*
             * Device doesn't support this path, these nodes.
	     */
	    vlDestroyPath(vlSvr, vlPath);
	    vlPath = -1;
	    Debug("Can not add drn_vid0 Node\n");
	} 

	if (!ret) ret = vlAddNode(vlSvr, vlPath, drn_vid1);
	if(ret)
	{
	    /*
             * Device doesn't support this path, these nodes.
	     */
	    vlDestroyPath(vlSvr, vlPath);
	    vlPath = -1;
	    Debug("Can not add drn_vid1 Node\n");
	} 

	if (!ret) ret = vlAddNode(vlSvr, vlPath, blend_node);
	if(ret)
	{
	    /*
             * Device doesn't support this path, these nodes.
	     */
	    vlDestroyPath(vlSvr, vlPath);
	    vlPath = -1;
	    Debug("Can not add blend_node Node\n");
	} 

    /*
     * Couldn't create the path, quit.
     */
    if (vlPath == -1)
    {
	vlPerror("Path Setup");
	docleanup();
    }

    /*
     * Print the node and path IDs for cmd line users.
     */
    if (print_ids)
    {
	printf("Node IDs:\n");

	printf("Screen Source      = %d\n", src_scr);
	printf("Video Drain Pixel  = %d\n", drn_vid0);
	printf("Video Drain Alpha  = %d\n", drn_vid1);
	printf("Blender            = %d\n", blend_node);
	printf("Path          ID   = %d\n", vlPath);
    }
    		
    /*
     * Set up the hardware for and define the usage of the path
     */
    if (vlSetupPaths(vlSvr, (VLPathList)&vlPath, 1, VL_SHARE, VL_SHARE) < 0)
    {
	vlPerror(_progname);
	exit(1);
    }

    /*
     * Start the data transfer immediately (i.e. don't wait for trigger)
     */
    vlBeginTransfer(vlSvr, vlPath, 0, NULL);

    /*
     * SETUP the keyer mode, keyer source and blend controls.
     */
    val.intVal = VL_BLDFCN_ZERO;
    vlSetControl(vlSvr, vlPath, blend_node, VL_BLEND_A_FCN, &val);

    val.intVal = VL_BLDFCN_A_ALPHA;
    vlSetControl(vlSvr, vlPath, blend_node, VL_BLEND_B_FCN, &val);

    val.intVal = 2;		/* This selects 1.0 for alpha */
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_BLEND_B_ALPHA_SELECT, &val);

    /*
     * Blender FG
     */
    if( ret = vlSetConnection(vlSvr, vlPath,
				src_scr, VL_IMPACT_PORT_PIXEL_SRC_A,
				blend_node,  VL_IMPACT_PORT_PIXEL_DRN_A,
				FALSE ) )
    {
	vlPerror( "SetConnection for Screen to Blender Pixel" );
	printf("*Connection failed for the Screen to Blender Pixel %d\n" , ret);
    }

    if( ret = vlSetConnection(vlSvr, vlPath,
				src_scr, VL_IMPACT_PORT_PIXEL_SRC_A,
				blend_node,  VL_IMPACT_PORT_ALPHA_DRN_A,
				FALSE ) )
    {
	vlPerror( "SetConnection for Screen to Blender Alpha" );
	printf("*Connection failed for the Screen to Blender Alpha %d\n" , ret);
    }

    /*
     * Blender BG
     */
    if( ret = vlSetConnection(vlSvr, vlPath,
				src_scr, VL_IMPACT_PORT_PIXEL_SRC_A,
				blend_node,  VL_IMPACT_PORT_PIXEL_DRN_B,
				FALSE ) )
    {
	vlPerror( "SetConnection for Screen to Blender Pixel" );
	printf("*Connection failed for the Screen to Blender Pixel %d\n" , ret);
    }

    /*
     * Video Outputs
     */
    if( ret = vlSetConnection(vlSvr, vlPath,
				blend_node, VL_IMPACT_PORT_PIXEL_SRC_A,
				drn_vid0,  VL_IMPACT_PORT_PIXEL_DRN_A,
				FALSE ) )
    {
	vlPerror( "SetConnection for Video Pixel Out 0" );
    }

    if( ret = vlSetConnection(vlSvr, vlPath,
				blend_node, VL_IMPACT_PORT_ALPHA_SRC_A,
				drn_vid1,  VL_IMPACT_PORT_PIXEL_DRN_A,
				FALSE ) )
    {
	vlPerror( "SetConnection for the Blender Alpha the Drain Node" );
    }

/*
 * Setup the Blend Parameters
 */

    val.intVal = VL_MGV_KEYERMODE_NONE;
    vlSetControl(vlSvr, vlPath, blend_node, VL_MGV_KEYER_MODE, &val);

    val.intVal = FALSE;
    vlSetControl(vlSvr, vlPath, blend_node, VL_BLEND_A_NORMALIZE, &val);

    val.intVal = FALSE;
    vlSetControl(vlSvr, vlPath, blend_node, VL_BLEND_B_NORMALIZE, &val);

    val.intVal = 1;
    vlSetControl(vlSvr, vlPath, drn_vid1, VL_MGV_OUTPUT_CHROMA, &val);

    /*
     * Handle event dispatching
     */
    vlMainLoop();
}

/*
 * Dispose of the vl structures.
 */
void
docleanup(void)
{   /* End the data transfer */
    vlEndTransfer(vlSvr, vlPath);
     
    /* Destroy the path, free it's memory */
    vlDestroyPath(vlSvr, vlPath);

    /* Disonnect from the daemon */
    vlCloseVideo(vlSvr);

    exit(0);
}

