/*
 * File:          vidtocsctomem.c
 *
 * Usage:         vidtocsctomem [-f filename] [-c numframes] [-t]
 *	      	[-n devicenum] [-v videosource] [-o memorysource] [-I]
 *		[-h] [-r packing] [-m range] [-u] [-a] [-l load_id]
 *
 * Description:   Vidtocsctomem captures CSC output stream to memory. 
 *		  Video input in 10-bit YUV 4:2:2 format is connected to 
 *		  CSC (Color Space Converter) which is programmed to convert
 *		  YUV in CCIR range to a packing type specified in -r option
 * 		  with a range type specified in -m option. Default output
 *		  packing is RGBA 8bit. Default output range is full range.
 *		  Default memory node is dual-linked because output is RGBA
 *		  as default. Using -o option to change the memory node
 *		  number if output packing doesn't require dual link. 
 *
 *		  The -u option turns off constant hue and alpha correction.
 *		  By default, constant hue is on and alpha correction is off.
 *		  The -a option turns on alpha correction.
 *	
 *		  The -l option is to load coefficients and luts generated 
 *		  by this example. It is to demostrate how to load custom 
 *		  tables. Values used in the luts and coefficients don't
 *		  have any special meanings.
 *
 * Functions:     SGI Video Library functions used (see other modules)
 *
 *                vlOpenVideo()
 *                vlGetDeviceList()
 *                vlGetNode  ()
 *                vlCreatePath()
 *                vlGetDevice()
 *                vlSetupPaths()
 *                vlSelectEvents()
 *		  vlAddCallback()
 *                vlSetControl()
 *                vlGetControl()
 *		  vlCreateBuffer()
 *                vlRegisterBuffer()
 *		  vlGetActiveRegion()
 *		  vlGetNextValid()
 *		  vlPutFree()
 *                vlGetTransferSize()
 *                vlBeginTransfer()
 *                vlEndTransfer()
 *                vlDeregisterBuffer()
 *                vlDestroyPath()
 *		  vlDestroyBuffer()
 *                vlCloseVideo()
 *                vlPerror()
 *                vlStrError()
 */

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dmedia/vl.h>
#include <dmedia/vl_mgv.h>
#include "csc_tables.h"

char *deviceName;
char *_progName;

#ifndef MAXNAMLEN
#define MAXNAMLEN 1024
#endif

int icount = 0;
char iname[MAXNAMLEN];
char iname_tmp[MAXNAMLEN];
char outfilename[MAXNAMLEN - 5] = "";
int imageCount = 1;
VLServer svr;
VLBuffer buffer;
int frames = 0;
VLPath path;
VLNode src, drn;
int xsize = 0;
int ysize = 0;
int xsetsize = 0;
int ysetsize = 0;
int frameSize;

int const_hue = 1;			/* CSC constant hue */
int alpha_correction = 0;		/* CSC alpha correction */
VLNode csc_node;			/* CSC internal node           */
int outputPacking = VL_PACKING_ABGR_8;	/* CSC output packing type     */
int outputRange	= VL_FORMAT_RGB;	/* CSC output full range type  */

void docleanup(int);
void ProcessEvent(VLServer, VLEvent *, void *);
void usage(void);
char *get_packing(void);
char *get_format(void);
void check_format_packing(void);

#define GRABFILE    "out"

#define USAGE \
"%s: [-f <filename>] [-n <device>] [-v <videonode> ] [-c <num>]\n\
\t\t[-t] [-I] [-o <memorynode>] [-r <packing>] [-m <range>]\n\
\t\t[-u] [-a] [-l <load_mask>] [-h]\n\
\t-f\toutput filename\n\
\t-c num\tnumber of frames to capture\n\
\t-n device\tdevice number to use\n\
\t-v\tvideo source node number\n\
\t-t\twait for external trigger\n\
\t-I\tprint node and path IDs\n\
\t-o\tmemory drain node number\n\
\t-r\toutput packing type for CSC node and memory drain\n\
\t-m\toutput range (format) type for CSC node and memory drain\n\
\t-u\tturn off CSC constant hue algorithm\n\
\t\t(By default, constant hue is on)\n\
\t-a\tenable CSC constant hue correction factor to be saved\n\
\t\tin output alpha (By default,  alpha correction is off)\n\
\t-l load_mask\tload custom luts or coefficients. Bit 0 of load_mask\n\
\t\tis for coef. Bit 1 is for input luts. Bit 2 is for output\n\
\t\tluts. (ie 1 is for coef. only, and 4 is for output luts)\n\
\t\tThis option is to demonstrate how to load custom tables, not\n\
\t\tto support all of conversions. Therefore this -l option\n\
\t\tdoesn't support YUV full-range output. However YUV full\n\
\t\trange output does work without -l option by using our\n\
\t\tdefault values.\n\
\t-h\tthis help message\n"

void
usage(void)
{
    int i;

    fprintf(stderr, USAGE, _progName);

    fprintf(stderr, "\nAvailable output packing types:\n");
 
    for (i = 0; i < VMMAX; i++) {
	fprintf(stderr, "\tNumber: %d, Name: %s\n", 
		packingTable[i].packing, packingTable[i].packingString);
    }

    fprintf(stderr, "\nAvailable output range types:\n");

    for (i = 0; i < FTMAX; i++) {
        fprintf(stderr, "\tNumber: %d, Name: %s\n",
		formatTable[i].format, formatTable[i].formatString);
    }
}

/* Dump data to an file */
void
DumpImage(char *data, int transferSize)
{
    int fd, ret;
    int fileSize;

    sprintf(iname,"%s-%05d", outfilename, icount++);

    strcat(iname, ".");
    strcat(iname, get_packing());
    strcat(iname, "_");
    strcat(iname, get_format());

    sprintf(iname_tmp, "%s.%d", iname, getpid());

    if ((fd = open(iname_tmp, O_WRONLY|O_CREAT|O_TRUNC, 0666)) == NULL) {
        fprintf(stderr, "Can't open %s\n", iname);
        exit(1);
    }

    fileSize = transferSize;
    ret = write(fd, data, fileSize);

    if (ret < fileSize) {
        perror(iname_tmp);
        exit(1);
    }
    close(fd);

    if (rename(iname_tmp, iname) < 0) {
        fprintf(stderr, "Cannot rename tmp file to %s, errno=%d\n",
                iname, errno);
        unlink(iname_tmp);
    }
}

/*
 * An example on how to load custom coefficients 
 */
void load_coef(void)
{
    VLControlValue val;
    VLExtendedValue extVal;
    int i;

    /* Each coefficient is 32-bit two's complement value. */

    static int ccir_yuv2fr_rgb_coef[9] =
        {0x25600000, 0xf3600000, 0xe5e00000, 0x25600000, 0x40c00000, 0, 0x25600000, 0, 0x33400000};

    static int ccir_yuv2rp175_rgb_coef[9] =
        {0x20000000, 0xf5000000, 0xe9200000, 0x20000000, 0x38c00000, 0, 0x20000000, 0, 0x2ce00000};

    static int passthru_coef[9] =
        {0x20000000, 0, 0, 0, 0x20000000, 0, 0, 0, 0x20000000};

    switch(outputPacking) {

    case VL_PACKING_RGBA_10:
    case VL_PACKING_RGBA_8:
    case VL_PACKING_ABGR_10:
    case VL_PACKING_ABGR_8:

    	if (outputRange == VL_FORMAT_RGB) {
	    extVal.dataPointer = ccir_yuv2fr_rgb_coef;
	    extVal.dataSize = sizeof (ccir_yuv2fr_rgb_coef);
	}
	else if (outputRange == VL_FORMAT_DIGITAL_COMPONENT_RGB_SERIAL) {
            extVal.dataPointer = ccir_yuv2rp175_rgb_coef;
            extVal.dataSize = sizeof (ccir_yuv2rp175_rgb_coef);
	}
	else {
	    fprintf(stderr, "Invalid range %d for loading coefficients\n",
	   	    outputRange);
	    exit(1);
 	}

    break;

    case VL_PACKING_YVYU_422_10:
    case VL_PACKING_YVYU_422_8:

        if (outputRange == VL_FORMAT_DIGITAL_COMPONENT_SERIAL) {

	    extVal.dataPointer = passthru_coef;
	    extVal.dataSize = sizeof (passthru_coef);

	}
        else {

            fprintf(stderr, "Output full range %d is not supported for this -l option\n");
            exit(1);
        }


    break;

    default:

	fprintf(stderr, "Invalid packing %d for loading coefficients\n",
		outputPacking);

	exit(1);	
    }

    extVal.dataType = MGV_CSC_COEF;
   
    val.extVal = extVal;

    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_COEF, &val) < 0) {
        vlPerror("SetControl of CSC coefficients failed");
	exit(1);
    }

    if (vlGetControl(svr, path, csc_node, VL_MGV_CSC_COEF, &val) < 0) {
        vlPerror("GetControl of CSC coefficients failed");
        exit(1);
    }

    for (i = 0; i < 9; i++)
	printf("Coef[%d] = 0x%x\n", i, *((int *)val.extVal.dataPointer+i)); 

}

/*
 * An example on how to set active page and load custom LUT to the new active
 * page in all of 3 input LUTS. 
 */
void load_onePageLut(void)
{
    VLControlValue val;
    VLExtendedValue extVal;
    VL_MGVInAlphaLutValue ylutVal;
    VL_MGVInAlphaLutValue uvlutVal;
    int i; 

    switch(outputPacking) {

    case VL_PACKING_RGBA_10:
    case VL_PACKING_RGBA_8:
    case VL_PACKING_ABGR_10:
    case VL_PACKING_ABGR_8:

    	for (i = 0; i < 1024; i++) {

	    ylutVal.lut[i] = i - 502;

	    if (ylutVal.lut[i] > 511)
		ylutVal.lut[i] = 511;
	    else if (ylutVal.lut[i] < -511)
		ylutVal.lut[i] = -511;

	    uvlutVal.lut[i] = i - 512;

	    if (uvlutVal.lut[i] > 511)
                uvlutVal.lut[i] = 511;
            else if (uvlutVal.lut[i] < -511)
                uvlutVal.lut[i] = -511;

        }

    break;

    case VL_PACKING_YVYU_422_10:
    case VL_PACKING_YVYU_422_8:

        if (outputRange == VL_FORMAT_DIGITAL_COMPONENT_SERIAL) {

	    /* pass-thru case */
    	    for (i = 0; i < 1024; i++)
		ylutVal.lut[i] = i;

	}
        else {

            fprintf(stderr, "Output full range %d is not supported for this -l option\n");
            exit(1);
        }


    break;

    default:

        fprintf(stderr, "Invalid packing %d for loading input LUTs\n",
                outputPacking);

        exit(1);
    }

    /* Use the page 2 in input luts as active page during normal mode */
    val.intVal = 2;
    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_LUT_IN_PAGE, &val) < 0)
        vlPerror("SetControl of CSC input lut page failed\n");

    /* New active page */
    ylutVal.pageNumber = 2;
    uvlutVal.pageNumber = 2;

    extVal.dataType = MGV_CSC_LUT_INPUT_AND_ALPHA; 
    extVal.dataSize = sizeof(VL_MGVInAlphaLutValue); 
    extVal.dataPointer = &ylutVal;

    val.extVal = extVal;

    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_LUT_IN_YG, &val) < 0) {
	vlPerror("SetControl of CSC YG input LUT failed\n");
	exit(1);
    }

    if ((outputPacking != VL_PACKING_YVYU_422_10) && 
	(outputPacking != VL_PACKING_YVYU_422_8)) {

    	extVal.dataType = MGV_CSC_LUT_INPUT_AND_ALPHA;
    	extVal.dataSize = sizeof(VL_MGVInAlphaLutValue);
    	extVal.dataPointer = &uvlutVal;
    	val.extVal = extVal;

    }

    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_LUT_IN_UB, &val) < 0) {
        vlPerror("SetControl of CSC UB input LUT failed\n");
        exit(1);
    }

    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_LUT_IN_VR, &val) < 0) {
        vlPerror("SetControl of CSC VR input LUT failed\n");
        exit(1);
    }
}

/*
 * An example on how to load 4 pages of LUT to all of 3 output LUTS. 
 */
void load_outputLut(void)
{
    VLControlValue val;
    VLExtendedValue extVal;
    VL_MGVOutLutValue lutVal;
    int i, j;

    switch(outputPacking) {

    case VL_PACKING_RGBA_10:
    case VL_PACKING_RGBA_8:
    case VL_PACKING_ABGR_10:
    case VL_PACKING_ABGR_8:

    	for (i = 0; i < 4096; i++) {

            if (i > 2047)
                j = i - 4096;
            else
                j = i;

            /* RP175 RGB range */
            if (outputRange == VL_FORMAT_DIGITAL_COMPONENT_RGB_SERIAL) {

		lutVal.lut[i] = j + 502;

                if (lutVal.lut[i] > 940)
                    lutVal.lut[i] = 940;
                else if (lutVal.lut[i] < 64)
                    lutVal.lut[i] = 64;

            }
            else { /* Full range */

		lutVal.lut[i] = j + 512;

                if (lutVal.lut[i] > 1023)
                    lutVal.lut[i] = 1023;
                else if (lutVal.lut[i] < 0)
                    lutVal.lut[i] = 0;

            }
    	}

    break;

    case VL_PACKING_YVYU_422_10:
    case VL_PACKING_YVYU_422_8:

	if (outputRange == VL_FORMAT_DIGITAL_COMPONENT_SERIAL) {

            /* pass-thru case */
	    for (i = 0; i < 4; i++)
    	        for (j = 0; j < 1024; j++)
           	     lutVal.lut[j+(i*1024)] = j;
	}
	else {

	    fprintf(stderr, "Output full range %d is not supported for this -l option\n");
	    exit(1);
	}

    break;

    default:

        fprintf(stderr, "Invalid packing %d for loading output LUTs\n",
                outputPacking);

        exit(1);
    }

    extVal.dataType = MGV_CSC_LUT_OUTPUT;
    extVal.dataSize = sizeof(VL_MGVOutLutValue);
    extVal.dataPointer = &lutVal;

    val.extVal = extVal;

    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_LUT_OUT_YG, &val) < 0) {
        vlPerror("SetControl of CSC YG output LUT failed\n");
        exit(1);
    }

    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_LUT_OUT_UB, &val) < 0) {
        vlPerror("SetControl of CSC YG output LUT failed\n");
        exit(1);
    }

    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_LUT_OUT_VR, &val) < 0) {
        vlPerror("SetControl of CSC YG output LUT failed\n");
        exit(1);
    }

}

void check_format_packing(void)
{
    switch(outputPacking) {
   
    	case VL_PACKING_RGBA_8:
	case VL_PACKING_RGBA_10:
	case VL_PACKING_ABGR_8:
	case VL_PACKING_ABGR_10:
	case VL_PACKING_A_2_BGR_10:
	case VL_PACKING_BGR_8_P:

	    if ((outputRange != VL_FORMAT_DIGITAL_COMPONENT_RGB_SERIAL) &&
		(outputRange != VL_FORMAT_RGB)) {
		fprintf(stderr, "Invalid output range %d\n", outputRange);
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

	    if ((outputRange != VL_FORMAT_DIGITAL_COMPONENT_SERIAL) &&
		(outputRange != VL_FORMAT_SMPTE_YUV)) {
		fprintf(stderr, "Invalid output range %d\n", outputRange);
		exit(1);
	    }
	    break;

	default:

	    fprintf(stderr, "Invalid output packing %d\n", outputPacking);
	    exit(1);
    }
}

main(int argc, char **argv)
{
    VLDevList devlist;
    VLDev     deviceId;
    VLControlValue val;
    int c;
    int vin = VL_ANY;
    int memnode = VL_MGV_NODE_NUMBER_MEM_VGI_DL;	/* Dual link */ 
    int devicenum = -1;
    int ext_trig = 0;
    int print_ids = 0;
    VLTransferDescriptor xferDesc;
    int load_mask = 0;

    _progName = argv[0];

    /*
     * Parse command line options
     * -f   output filename
     * -c   capture n frames
     * -n   use device number n
     * -v   video source node number n
     * -o   memory drain node number n
     * -t   wait for external trigger
     * -I   publish node and path IDs for command line interface users
     * -p   use n as CSC output packing type
     * -r   use n as CSC output range type
     * -u   turn off constant hue algorithm in CSC (Default is on)
     * -h   help message
     */
    while ((c = getopt(argc, argv, "f:n:c:hv:o:r:m:l:Itua")) != EOF)
    {
	switch (c)
	{
	    case 'f':
		sprintf(outfilename,"%s",optarg);
	    break;

	    case 'c':
		imageCount = atoi(optarg);
	    break;

	    case 'n':
		devicenum = atoi(optarg);
	    break;

	    case 'v':
		vin = atoi(optarg);
	    break;

	    case 'o':
		memnode = atoi(optarg);
	    break;
	
	    case 't':
		ext_trig = 1;
	    break;

	    case 'I':
		print_ids = 1;
	    break;

	    /********************************/
	    /* CSC constant hue user option */
	    /********************************/
	    case 'u':
		const_hue = 0;
	    break;

	    case 'a':
		alpha_correction = 1;
	    break;

	    case 'r':
		outputPacking = atoi(optarg);
            break;

	    case 'm':
		outputRange = atoi(optarg);
            break;

	    case 'l':
		load_mask = atoi(optarg);
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

    if (outfilename[0] == '\0')
        strcpy(outfilename, GRABFILE);

    /* Connect to the daemon */
    if (!(svr = vlOpenVideo("")))
    {
	fprintf(stderr, "%s: couldn't open video\n", _progName);
	exit(1);
    }

    /* Get the list of devices that the daemon supports */
    if (vlGetDeviceList(svr, &devlist) < 0)
    {
	fprintf(stderr,"%s: getting device list: %s\n",_progName,vlStrError(vlErrno));
	exit(1);
    }

    /* Make sure the user specified device (if any) is in the list */
    if ((devicenum >= (int)devlist.numDevices) || (devicenum < -1))
    {
	if (devlist.numDevices == 1)
	    fprintf(stderr,"%s: The device number must be 0\n",_progName);
	else
	    fprintf(stderr,"%s: The device number must be between 0 and %d\n",
		_progName, devlist.numDevices-1);
	exit(1);
    }

    /* Set up a drain node in memory */
    drn = vlGetNode(svr, VL_DRN, VL_MEM, memnode);

    /* Set up a source node from video */
    src = vlGetNode(svr, VL_SRC, VL_VIDEO, vin);

    /*********************/
    /* Set up a CSC node */
    /*********************/

    csc_node = vlGetNode(svr, VL_INTERNAL, VL_CSC, VL_MGV_NODE_NUMBER_CSC);

    /*
     * If the user didn't specify a device create a path using the first
     * device that will support it
     */
    if (devicenum == -1)
    {
	if ((path = vlCreatePath(svr, VL_ANY, src, drn)) < 0)
	{
	    vlPerror(_progName);
	    exit(1);
	}
	/* Get the device number and name */
	devicenum = vlGetDevice(svr, path);
	deviceName = devlist.devices[devicenum].name;
    }
    else
    /* Create a path using the user specified device */
    {
	deviceName = devlist.devices[devicenum].name;
	deviceId   = devlist.devices[devicenum].dev;
	if ((path = vlCreatePath(svr, deviceId, src, drn)) < 0)
	{
	    vlPerror(_progName);
	    exit(1);
	}
    }

    /****************************/
    /* Add CSC node to the path */
    /****************************/

    if (vlAddNode(svr, path, csc_node))
    {
        vlPerror("Add CSC Device Node");
        vlDestroyPath(svr, path);
	exit(1);
    }

    /* Print the node and path IDs for cmd line users */
    if (print_ids)
    {
	printf("VIDTOCSCTOMEM NODE IDs:\n");
	printf("video source = %d\n", src);
	printf("memory drain = %d\n", drn);
	printf("csc = %d\n", csc_node);
	printf("VIDEO TO MEMORY PATH ID = %d\n", path);
    }

    /* Set up the hardware for and define the usage of the path */
    if (vlSetupPaths(svr, (VLPathList)&path, 1, VL_SHARE, VL_SHARE) < 0)
    {
	vlPerror(_progName);
	exit(1);
    }

    /*
     * Set the memory node's timing based upon the video source's timing
     */
    if (vlGetControl(svr, path, src, VL_TIMING, &val) < 0) {
        vlPerror("GetControl Failed");
        exit(1);
    }
    if (vlSetControl(svr, path, drn, VL_TIMING, &val) < 0) {
        vlPerror("SetControl Failed");
        exit(1);
    }


    /*
     * Specify what path-related events we want to receive.
     * In this example we want transfer complete and transfer failed
     * events only.
     */
    vlSelectEvents(svr, path, VLTransferCompleteMask|VLTransferFailedMask);

    val.intVal = outputRange;
    if (vlSetControl(svr, path, drn, VL_FORMAT, &val) < 0)
        vlPerror("SetControl of memory format failed");

    val.intVal = outputPacking;
    if (vlSetControl(svr, path, drn, VL_PACKING, &val) < 0)
	vlPerror("SetControl of memory packing failed");

 
    /********************/
    /* Set CSC controls */
    /********************/

    /* Set up CSC input range - CCIR YUV */
    val.intVal = VL_FORMAT_DIGITAL_COMPONENT_SERIAL;
    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_IN_RANGE, &val) < 0)
	vlPerror("SetControl of CSC input range failed");

    /* Set up CSC input packing - YUV 422 */

    if (vin == VL_MGV_NODE_NUMBER_VIDEO_DL)
	val.intVal = VL_PACKING_YUVA_4444_10;
    else
    	val.intVal = VL_PACKING_YVYU_422_10;

    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_IN_PACKING, &val) < 0)
	vlPerror("SetControl of CSC input packing failed");

    /* Set up CSC output range */
    val.intVal = outputRange;
    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_OUT_RANGE, &val) < 0)
	vlPerror("SetControl of CSC output range failed");

    /* Set up CSC output packing */
    val.intVal = outputPacking;
    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_OUT_PACKING, &val) < 0)
	vlPerror("SetControl of CSC output packing failed");

    /*   
     * Default coefficients, input luts, output luts and alpha lut are loaded
     * once input/output packing and range types are specified.
     *
     * For users to load custom coef. and luts, control names VL_MGV_CSC_COEF,
     * VL_MGV_CSC_LUT_IN_Y/G, VL_MGV_CSC_LUT_IN_U/B, VL_MGV_CSC_LUT_IN_V/R,
     * VL_MGV_CSC_LUT_OUT_Y/G, VL_MGV_CSC_LUT_OUT_U/B, VL_MGV_CSC_LUT_OUT_V/R,
     * VL_MGV_CSC_LUT_ALPHA should be used. Their control value types are 
     * VLExtendedValue.
     *
     * Controls VL_MGV_CSC_LUT_IN_PAGE and VL_MGV_CSC_LUT_ALPHA_PAGE set
     * active page in the input and alpha luts to be used during normal 
     * operation. Their default values are 0 (first page in the luts).
     *
     * Controls VL_MGV_CSC_CONST_HUE and VL_MGV_CSC_ALPHA_CORRECTION
     * turns constant hue and alpha correction on or off. As default, 
     * constant hue is on and alpha correction is off.
     */ 

    /* Turn on or off CSC constant hue algorithm */
    val.boolVal = const_hue;
    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_CONST_HUE, &val) < 0)
	vlPerror("SetControl of CSC constant hue failed\n");

    /* Turn on or off alpha correction in the alpha channel */
    val.boolVal = alpha_correction;
    if (vlSetControl(svr, path, csc_node, VL_MGV_CSC_ALPHA_CORRECTION, &val) < 0)
	vlPerror("SetControl of CSC alpha correction failed\n");

    /* Loading custom coefficients and LUTs */

    if (load_mask & 0x1)
	load_coef();

    if (load_mask & 0x2)
	load_onePageLut();

    if (load_mask & 0x4)
	load_outputLut();
 

    /* Set the video size to what the user requested */
    if (!xsize)
	vlGetControl(svr, path, drn, VL_SIZE, &val);
    else
    {
	val.xyVal.x = xsetsize = xsize;
	val.xyVal.y = ysetsize = ysize;
	vlSetControl(svr, path, drn, VL_SIZE, &val);

	/* Make sure the hardware supports this size */
	vlGetControl(svr, path, drn, VL_SIZE, &val);
    }

    xsize = val.xyVal.x;
    ysize = val.xyVal.y;

    if (!xsetsize)
    {
	xsetsize = xsize;
	ysetsize = ysize;
    }
    else
    {
	if (xsetsize > xsize || xsetsize <= 0)
	    xsetsize = xsize;
	if (ysetsize > ysize || ysetsize <= 0)
	    ysetsize = ysize;
    }

    frameSize = vlGetTransferSize(svr, path);

    /* Set ProcessEvent() as the callback for a transfer events */
    vlAddCallback(svr, path, VLTransferCompleteMask|VLTransferFailedMask,
			         ProcessEvent, NULL);

    /* Create and register a buffer of imageCount frames... */
    buffer = vlCreateBuffer(svr, path, drn, imageCount);
    if (buffer == NULL)
    {
	vlPerror(_progName);
	exit(1);
    }
    if (vlRegisterBuffer(svr, path, drn, buffer))
    {
	vlPerror(_progName);
	exit(1);
    }

    /*
     * Fill the transfer descriptor for a discrete transfer of imageCount
     * frames. If the external trigger option is set, we use VLDeviceEvent
     * for a trigger, otherwise trigger immediately. Note that this may not
     * work well on all devices, as VLDeviceEvent it not well-defined.
     */
    xferDesc.mode = VL_TRANSFER_MODE_DISCRETE;
    xferDesc.count = imageCount;
    xferDesc.delay = 0;
    if (ext_trig)
        xferDesc.trigger = VLDeviceEvent;
    else
        xferDesc.trigger = VLTriggerImmediate;

    /* Begin the data transfer */
    if (vlBeginTransfer(svr, path, 1, &xferDesc))
    {
	vlPerror(_progName);
	exit(1);
    }

    /* Loop until all requested frames are grabbed */
    vlMainLoop();

}

/*
 * Handle video library events
 * (only transfer complete and transfer failed events in this example)
 */
void
ProcessEvent(VLServer svr, VLEvent *ev, void *data)
{
    VLInfoPtr info;
    char *dataPtr;

    switch (ev->reason)
    {
	case VLTransferComplete:
	    /* Get the number and location of frames captured */
	    while (info = vlGetNextValid(svr, buffer))
	    {
		/* Get a pointer to the frame(s) */
		dataPtr = vlGetActiveRegion(svr, buffer, info);

		if (frames < imageCount)
		{
		    frames++;

		    /* Write the frame(s) to memory (and to disk if requested) */
		    DumpImage(dataPtr, frameSize);

		    fprintf(stderr,"%s: saved image to file %s\n", _progName, iname);

		}
		/* Finished with frame, unlock the buffer */
		vlPutFree(svr, buffer);
	    }

	    /* Exit when all frames are grabbed */
      	    if (frames == imageCount)
	        docleanup(0);
	break;

	case VLTransferFailed:
	    fprintf(stderr,"%s: Transfer failed.\n",_progName);
	    docleanup(1);
	break;

	default:
	break;
    }
}


/*
 * Do general cleanup before exiting. All of our resources should
 * be freed.
 */
void
docleanup(int ret)
{
    vlEndTransfer(svr, path);
    vlDeregisterBuffer(svr, path, drn, buffer);
    vlDestroyBuffer(svr, buffer);
    vlDestroyPath(svr, path);
    vlCloseVideo(svr);
    exit(ret);
}

char *
get_packing(void)
{
    VLControlValue val;
    char *packing = "unknown";
    int i;

    if (vlGetControl(svr, path, drn, VL_PACKING, &val) < 0)
        vlPerror("Getting VL_PACKING");

    for (i = 0; i < VMMAX; i++) {
        if ( val.intVal == packingTable[i].packing ) {
                packing = packingTable[i].packingString;
                break;
        }
    }

    return packing;
}

char *
get_format(void)
{
    VLControlValue val;
    char *format = "unknown";
    int i;

    if (vlGetControl(svr, path, drn, VL_FORMAT, &val) < 0)
        vlPerror("Getting VL_FORMAT");

    for (i = 0; i < FTMAX; i++) {
        if ( val.intVal == formatTable[i].format) {
                format = formatTable[i].formatString;
		break;
	}
    }

    return format;
}
