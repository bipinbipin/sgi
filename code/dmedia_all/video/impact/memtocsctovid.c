/*
 * Files:         memtocsctovid.c
 *
 * Usage:         memtocsctovid [-t] [-p] [-n numframes] [-I] -f filename
 *		  [-r inputPackingType] [-m inputRangeType] [-u] [-a]
 *
 * Description:   Memtocsctovid sends a frame from memory to CSC which will
 *		  convert data format from input packing specified in -r and -m
 *		  options to CCIR 10-bit YUV 422 packing which will then
 *		  be sent to the video output #1. The -u option is to turn
 *		  constant hue off. Default is on. The -a option is to turn
 *		  alpha correction on. Default is off.
 *
 * Functions:     SGI Video Library functions used (see other modules)
 *
 *                vlOpenVideo()
 *                vlGetNode()
 *                vlCreatePath()
 *                vlSetupPaths()
 *                vlGetControl()
 *                vlRegisterBuffer()
 *                vlGetBufferInfo()
 *		  vlCreateBuffer()
 *                vlGetTransferSize()
 *		  vlGetNextFree()
 *		  vlGetActiveRegion()
 *                vlBufferDone()
 *                vlBeginTransfer()
 *                vlEndTransfer()
 *                vlDeregisterBuffer()
 *		  vlDestroyBuffer()
 *                vlDestroyPath()
 *                vlCloseVideo()
 *                vlPerror()
 *                vlStrError()
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <malloc.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <getopt.h>
#include <vl/vl.h>
#include <dmedia/vl_mgv.h>
#include "csc_tables.h"

#define XSIZE 640
#define YSIZE 480

#define MIN(x,y) ((x>y)?y:x)

#ifndef MAXNAMLEN
#define MAXNAMLEN 1024
#endif

extern int errno;

int count = 0;
int tevent = 0;
int stopped = 0;
char *_progName;
char fileName[MAXNAMLEN];
VLServer svr;
VLPath MEMtoVIDPath;
VLNode src, drn;
VLBuffer buf;

VLNode csc_node;			/* CSC internal node */
int const_hue = 1;			/* CSC constant hue */
int alpha_correction = 0;		/* CSC alpha correction */
int inputPacking = -1;			/* CSC input packing type */
int inputRange = -1;			/* CSC input range type */

void usage(void);
char *incr_filename(char *fname);
void process_event(int block);
void do_cleanup(int exit_type);
int  get_packing(char *fname);
int  get_format(char *fname);
void check_format_packing(void);

#define USAGE \
"%s: -f <filename> [-h] [-o <video-node>] [-v <memory-node>]\n\
\t\t[-t ] [-p] [-P] [-n <numframes>] [-I] [-m <format>] \n\
\t\t[-r [packing]] [-u] [-a]\n\
\n\
\t-f\tinput filename\n\
\t-h\tthis message\n\
\t-o\toutput node number\n\
\t-v\tmemory node number\n\
\t-t\tuse external trigger\n\
\t-p\tpause after each frame\n\
\t-P\tpause after entire sequence is done\n\
\t-n\tnumber of frames to output\n\
\t-I\tprint node and path IDs\n\
\t-r\tinput packing type for CSC node and memory surce\n\
\t-m\tinput range (format) type for CSC node and memory source\n\
\t-u\tturn off CSC constant hue algorithm\n\
\t\t(By default, constant hue is on)\n\
\t-a\tenable CSC constant hue correction factor stored in\n\
\t\tinput alpha (By default, alpha correction is off)\n"

void
usage(void)
{
    int i;

    fprintf(stderr, USAGE, _progName);

    fprintf(stderr, "\nAvailable input packing types:\n");

    for (i = 0; i < VMMAX; i++) {
        fprintf(stderr, "\tNumber: %d, Name: %s\n",
                packingTable[i].packing, packingTable[i].packingString);
    }

    fprintf(stderr, "\nAvailable input range types:\n");

    for (i = 0; i < FTMAX; i++) {
        fprintf(stderr, "\tNumber: %d, Name: %s\n",
                formatTable[i].format, formatTable[i].formatString);
    }
}

main(int argc, char **argv)
{
    VLControlValue val;
    VLInfoPtr info;
    char * dataPtr;
    int transferSize;
    int fd, ret;
    int trigger = 0;
    int singlefrm = 0;
    int seqpause = 0;
    int nframes = 1;
    int repeats;
    int print_ids = 0;
    struct stat status_buffer;
    int c;
    char *nextName;
    int voutnode = VL_ANY;
    int memnode = VL_MGV_NODE_NUMBER_MEM_VGI_DL;

    VLTransferDescriptor xferDesc;

    _progName = argv[0];

    /* 
     * Parse command line:
     * -t use external trigger
     * -p pause after each frame
     * -n number of frames to get
     * -P pause after entire sequence is done
     * -I print node and path IDs for command line interface users
     * -f <filename> name of file containing video data
     * -r <packing> specify input packing type
     * -m <format> specify requested format
     * -o <outnode> specify output node
     * -v <memnode> specify memory node
     * -u turn off constant hue algorithm
     * -a enable alpha correction factor
     */

    fileName[0] = 0;
    while ((c = getopt(argc, argv, "htpPIn:f:o:m:rv:ua")) != EOF) 
    { 
	switch(c)
	{
	    case 'u':
		const_hue = 0;
	    break;
	
	    case 'a':
		alpha_correction = 1;
	    break;
	
	    case 'r':
		if (optind < argc && argv[optind][0] != '-') {
		    inputPacking = atoi(argv[optind]);
		    optind++;
		}
	    break;

	    case 'v':
		memnode = atoi(optarg);
	    break;

	    case 'm':
		inputRange = atoi(optarg);
	    break;

	    case 'n':
		nframes = atoi(optarg);
	    break;
	    
	    case 'p':
		singlefrm = 1;
	    break;
	    
	    case 'P':
		seqpause = 1;
	    break;

	    case 'o':
		voutnode = atoi(optarg);
	    break;
	    
	    case 'f':
		sprintf(fileName,"%s",optarg);
	    break;
	    
	    case 't':
		trigger = 1;
	    break;
	    
	    case 'I':
		print_ids = 1;
	    break;
	    
	    case 'h':
		usage();
		exit(0);
	    break;

	    default:
		usage();
		exit(1);
            break;
	 }
    }

    check_format_packing();

    if (strlen(fileName) == 0) {
	usage();
	exit(1);
    }

    /* Connect to the daemon */
    if (!(svr = vlOpenVideo(""))) 
    {
	printf("couldn't open video\n");
	exit(1);
    }

    /* 
     * Create the memory to video path (just grab the first device 
     * that supports it) 
     */
     
    /* Set up a source node in memory */
    src = vlGetNode(svr, VL_SRC, VL_MEM, memnode);

    /* Set up a video drain node on the first device that has one */
    drn = vlGetNode(svr, VL_DRN, VL_VIDEO, voutnode);

    /*********************/
    /* Set up a CSC node */
    /*********************/

    csc_node = vlGetNode(svr, VL_INTERNAL, VL_CSC, VL_MGV_NODE_NUMBER_CSC);

    /* Create a path using the selected devices */
    MEMtoVIDPath = vlCreatePath(svr, VL_ANY, src, drn);

    if (MEMtoVIDPath == -1) 
    {
	printf("%s: can't create path: %s\n", _progName, vlStrError(vlErrno));
	exit(1);
    }

    /****************************/
    /* Add CSC node to the path */
    /****************************/

    if (vlAddNode(svr, MEMtoVIDPath, csc_node))
    {
        vlPerror("Add CSC Device Node");
        vlDestroyPath(svr, MEMtoVIDPath);
        exit(1);
    }

     /* Print the node and path IDs for cmd line users */
    if (print_ids)
    {
	printf("MEMTOVID NODE IDs:\n");
	printf("memory source = %d\n", src);
	printf("video drain = %d\n", drn);
        printf("csc = %d\n", csc_node);
	printf("MEMORY TO VIDEO PATH ID = %d\n", MEMtoVIDPath);
    }
    
    /* Set up the hardware for and define the usage of the path */
    if (vlSetupPaths(svr, (VLPathList)&MEMtoVIDPath, 1, VL_SHARE, VL_SHARE)<0)
    {
	printf("%s: can't setup path: %s\n", _progName, vlStrError(vlErrno));
	exit(1);
    }

    if (inputPacking < 0)
    {
    	inputPacking = get_packing(fileName);
	if (inputPacking < 0) {
	    printf("Couldn't determine packing.\n"
		   "Default is VL_PACKING_YVYU_422_10\n");
	    inputPacking = VL_PACKING_YVYU_422_10;
	}
    }

    if (inputRange < 0)
    {
	inputRange = get_format(fileName);

   	if (inputRange < 0)
	{
	    printf("Couldn't determine format\n"
		   "Default is VL_FORMAT_DIGITAL_COMPONENT_SERIAL");
 
	    inputRange = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
	}
    }
	
    val.intVal = inputRange;
    if (vlSetControl(svr, MEMtoVIDPath, src, VL_FORMAT, &val) < 0) 
    { 
	vlPerror("vlSetControl(VL_FORMAT)");
	exit(1);
    }

    val.intVal = inputPacking;
    if (vlSetControl(svr, MEMtoVIDPath, src, VL_PACKING, &val) < 0)
    {
        vlPerror("vlSetControl(VL_PACKING)");
        exit(1);
    }

    /********************/
    /* Set CSC controls */
    /********************/

    /* Set up CSC input range */
    val.intVal = inputRange;
    if (vlSetControl(svr, MEMtoVIDPath, csc_node, VL_MGV_CSC_IN_RANGE, 
	&val) < 0)
        vlPerror("SetControl of CSC input range failed");

    /* Set up CSC input packing */
    val.intVal = inputPacking;
    if (vlSetControl(svr, MEMtoVIDPath, csc_node, VL_MGV_CSC_IN_PACKING, 
	&val) < 0)
        vlPerror("SetControl of CSC input packing failed");

    /* Set up CSC output range - CCIR YUV */
    val.intVal = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
    if (vlSetControl(svr, MEMtoVIDPath, csc_node, VL_MGV_CSC_OUT_RANGE, 
	&val) < 0)
        vlPerror("SetControl of CSC output range failed");

    /* Set up CSC output packing - YUV 422 */

    if (voutnode == VL_MGV_NODE_NUMBER_VIDEO_DL)
	val.intVal = VL_PACKING_YUVA_4444_10;
    else
        val.intVal = VL_PACKING_YVYU_422_10;

    if (vlSetControl(svr, MEMtoVIDPath, csc_node, VL_MGV_CSC_OUT_PACKING, 
	&val) < 0)
        vlPerror("SetControl of CSC output packing failed");

    /* Turn on or off CSC constant hue algorithm */
    val.boolVal = const_hue;
    if (vlSetControl(svr, MEMtoVIDPath, csc_node, VL_MGV_CSC_CONST_HUE, 
	&val) < 0)
        vlPerror("SetControl of CSC constant hue failed\n");

    /* Turn on or off alpha correction in the alpha channel */
    val.boolVal = alpha_correction;
    if (vlSetControl(svr, MEMtoVIDPath, csc_node, VL_MGV_CSC_ALPHA_CORRECTION, 
	&val) < 0)
        vlPerror("SetControl of CSC alpha correction failed\n");

    /* Make sure the path supports this size */
    transferSize = vlGetTransferSize(svr,MEMtoVIDPath);

    /* 
     * Create a ring buffer for the data transfers.  Only a single buffer
     * is needed for single frame output, otherwise allocate enough space
     * for all frames to fit.
     */
    if (singlefrm)
	buf = vlCreateBuffer(svr, MEMtoVIDPath, src, 2);
    else
	buf = vlCreateBuffer(svr, MEMtoVIDPath, src, nframes);
    if (!buf)
    {
	vlPerror(_progName);
	exit(1);
    }

    /* Associate the ring buffer with the path */
    if (vlRegisterBuffer(svr, MEMtoVIDPath, src, buf))
    {
	vlPerror(argv[0]);
	vlDestroyBuffer(svr, buf);
	exit(1);
    }
    	 
    /* Events to look for */
    vlSelectEvents(svr, MEMtoVIDPath,  (VLTransferCompleteMask |
					VLStreamPreemptedMask	|
					VLStreamStartedMask	|
					VLStreamStoppedMask	|
					VLTransferFailedMask));
    /*
     * Fill the transfer descriptor for a discrete transfer of one frame.
     * If the external trigger option is set, we use VLDeviceEvent for a
     * trigger, otherwise trigger immediately. Note that this may not
     * work well on all devices, as VLDeviceEvent it not well-defined.
     */
    xferDesc.delay = 0;
    if (trigger)
	xferDesc.trigger = VLDeviceEvent;
    else
	xferDesc.trigger = VLTriggerImmediate;
    
    /*
     * For single framing, set up xfer descriptor for continuous transfer
     * mode and begin the transfer now.  Otherwise, set up for multiple
     * frames and begin the discrete transfer oly after all frames have
     * been put in the ring buffer.
     */
    if (singlefrm) {
	xferDesc.count = 0;
	xferDesc.mode = VL_TRANSFER_MODE_CONTINUOUS;
	if (vlBeginTransfer(svr, MEMtoVIDPath, 1, &xferDesc) < 0) {
	    vlPerror("vlBeginTransfer");
	    goto outahere;
	}
    }
    else {
	xferDesc.count = nframes;
	xferDesc.mode = VL_TRANSFER_MODE_DISCRETE;
    }
	
    for (repeats = nframes; repeats; repeats--) 
    {	 

	/* 
	 * Get the next free frame in the buffer, reserve it for the data.
	 * Loop until a frame is free or the buffer is taken over by
	 * someone else.
	 */
	do
	{
	    process_event(FALSE);
	    info = vlGetNextFree(svr, buf, transferSize);
	} while (!info && !vlBufferDone(buf));

	/* Get a pointer to where the active region of data will go */
	dataPtr = vlGetActiveRegion(svr, buf, info);

    	if ((fd = open(fileName, O_RDONLY)) < 0)
	{
	    perror(fileName);
	    exit(1);
 	}
	
	/* Get the file's size (image size of this data)*/
	if (fstat(fd,&status_buffer) == -1) 
	{
	    perror(fileName);
	    exit(1);
	}

	/* Make sure the hardware supports this image size */
	if (status_buffer.st_size != transferSize) 
	{
	    printf("%s: The image is not the right size for this device\n",
	   	   _progName);
	    exit(1);
	}

	/* Read in the data */
	ret = read(fd, dataPtr, transferSize);
	close(fd);
	    
	/* Check the size of the data read in */
	if (ret != transferSize) 
	{
	    if (ret == -1)
	        perror(fileName);
	    fprintf(stderr, "%s: Unable to read the image data\n", _progName);
			do_cleanup(1);
	}
    
	/* Put the data into the buffer  */
	vlPutValid(svr, buf);
        
	/*
	 * Pause for single frame output.
	 */
	if (singlefrm)	{
	    printf("[%s]  Hit return for next frame.", fileName);
	    c = getc(stdin);
	}

	/* 
	 * Increment the number in the filename to get
	 * the next file in sequence.
	 */
	if (repeats > 1)
	{
	    nextName = incr_filename(fileName);
	    if (!nextName)
	    {
		fprintf(stderr, "%s: Illegal filename\n", _progName);
		do_cleanup(1);
	    }
	    strcpy(fileName, nextName);   
	} 
    }

    /*
     * For full frame sequence, begin the transfer now since all of the
     * buffers have been filled.
     */
    if (!singlefrm) {
	if (vlBeginTransfer(svr, MEMtoVIDPath, 1, &xferDesc) < 0) {
	    vlPerror("vlBeginTransfer");
	    goto outahere;
	}
    }

    /* Wait for a return if -P option used,
     * otherwise wait until a StreamStopped event;
     */

    if (seqpause) {
	printf("Frame sequence complete.  Hit return to exit.\n");
	c = getc(stdin);
    } else if (!singlefrm) {
	while ( !stopped )
	    process_event(TRUE);
    }

outahere:
    /* Cleanup and exit with no error */
    do_cleanup(0);
}

void check_format_packing(void)
{
    switch(inputPacking) {

        case VL_PACKING_RGBA_8:
        case VL_PACKING_RGBA_10:
        case VL_PACKING_ABGR_8:
        case VL_PACKING_ABGR_10:
        case VL_PACKING_A_2_BGR_10:
        case VL_PACKING_BGR_8_P:

            if ((inputRange != VL_FORMAT_DIGITAL_COMPONENT_RGB_SERIAL) &&
                (inputRange != VL_FORMAT_RGB)) {
                fprintf(stderr, "Invalid input range %d\n", inputRange);
                exit(1);
            }
            break;

        case VL_PACKING_YVYU_422_8:
        case VL_PACKING_YVYU_422_10:
        case VL_PACKING_YUVA_4444_8:
        case VL_PACKING_YUVA_4444_10:
        case VL_PACKING_AUYV_4444_8:
        case VL_PACKING_AUYV_4444_10:
        case VL_PACKING_AYU_AYV_10:
        case VL_PACKING_A_2_UYV_10:
        case VL_PACKING_UYV_8_P:

            if ((inputRange != VL_FORMAT_DIGITAL_COMPONENT_SERIAL) &&
                (inputRange != VL_FORMAT_SMPTE_YUV)) {
                fprintf(stderr, "Invalid input range %d\n", inputRange);
                exit(1);
            }
            break;

        default:

            fprintf(stderr, "Invalid input packing %d\n", inputPacking);
            exit(1);
    }
}

char * 
incr_filename(char *fname)
{
    char *buffer;
    char *index;
    char *suffix = "";
    char name[MAXNAMLEN];
    long int number;
    
    strcpy(name, fname);
    if ((buffer = strtok(name, "-")) == NULL)
	return(FALSE);
    if ((index = strtok(NULL, ".")) == NULL)
        return(FALSE);
	
    suffix = strtok(NULL, " ");
	
    number = atoi(index);
    number++;
    sprintf(name,"%s-%05ld.%s", buffer, number, suffix);

    return(name);
}

void
process_event(int block)
{
    int ret;
    VLEvent ev;
    
    if (block)
	ret = vlNextEvent(svr, &ev);
    else
	ret = vlCheckEvent(svr, VLAllEventsMask, &ev);

    if (ret == -1)
	return;

    switch (ev.reason)
    {    
	case VLStreamPreempted:
	    fprintf(stderr, "%s: Stream Preempted by another Program\n",
		    _progName);
	    exit(1);
	break;

	/*
	 * XXX jas - FYI, as of 8/31/94 ev1 still does not support the
	 * VLStreamStopped and VLStreamStarted events.
	 */
        case VLStreamStopped:
	    stopped = 1;
	    break;
	case VLTransferFailed: 
	    fprintf(stderr, "%s: Transfer failed\n", _progName);
	    exit(1);
	break;
	
	default:
	break;
    }

}

void do_cleanup(int exit_type)
{
    /* End the data transfer */
    vlEndTransfer(svr, MEMtoVIDPath);
    /* Disassociate the ring buffer from the path */
    vlDeregisterBuffer(svr, MEMtoVIDPath, src, buf);
    /* Destroy the path, free its memory */
    vlDestroyPath(svr, MEMtoVIDPath);
    /* Destroy the ring buffer, free its memory */
    vlDestroyBuffer(svr, buf);
    /* Disconnect from the daemon */
    vlCloseVideo(svr);
    
    exit(exit_type);
}

int get_packing(char *fileName)
{
    /* packing is the string appended to the filename after the '.' */
    char tmpstr[256] = "";
    char *packstr, *ptr;
    int  packType = -1;
    int  i = 0;

    strcpy(tmpstr, fileName);

    /* remove _ending */
    ptr = strrchr(tmpstr, '_');
    if (ptr)
        *ptr = 0;

    packstr = strrchr(tmpstr, '.');

    if (packstr) {
        packstr++;
	printf("Packing = %s\n", packstr);
        for (i = 0; i < VMMAX; i++) {
                if (strcmp(packstr, packingTable[i].packingString) == 0) {
                        packType = packingTable[i].packing;
                        break;
                }

        }
    }

    return packType;
}

int get_format(char *fileName)
{
    /* format is the string appended to the filename after packing and 
	after the '.' */

    char *formatstr = strrchr(fileName, '_');
    int format = -1, i;

    if (formatstr) {
        formatstr++;
	printf("Format = %s\n", formatstr);
        for (i = 0; i < FTMAX; i++) {
                if (strcmp(formatstr, formatTable[i].formatString) == 0) {
                        format = formatTable[i].format;
                        break;
                }
        }
    }

    return format;
}
