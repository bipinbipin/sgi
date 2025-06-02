/*
 * Files:         simplev2v.c
 *
 * Usage:         simplev2v
 *
 * Description:   Simplev2v sends continuous video video input
 *                to the video output. Simplev2v only runs on 
 *  	  	  video harware that has a video output port.
 *		  It will not run on a VINO video board.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <dmedia/vl.h>

#define USAGE \
"%s: [-n devnum]\n\
\t-n\t video device number\n"

main(int argc, char **argv)
{
    VLServer svr;
    VLPath VIDPath;
    VLNode src, drn;
    int c;
    int devicenum = -1;
    char *_progName;

    _progName = argv[0];

    while ((c = getopt(argc, argv, "n:")) != EOF) 
    {
	switch(c) 
	{
	    case 'n':
		devicenum = atoi(optarg);
	    break;
	    
	    default:
		fprintf(stderr, USAGE, _progName);
		exit(1);
	    break;
	 }    
    }
    
    /* Connect to the daemon */
    if (!(svr = vlOpenVideo(""))) 
    {
	printf("%s: can't open video: %s\n", _progName, vlStrError(vlGetErrno()));
	exit(1);
    }

    /* Set up a source node on the first available video device */
    src = vlGetNode(svr, VL_SRC, VL_VIDEO, VL_ANY);
    
    /* Set up a video drain node on the first device that has one */
    drn = vlGetNode(svr, VL_DRN, VL_VIDEO, VL_ANY); 
    
    /* Create a path using the selected device(s) */
    if (devicenum == -1)
	VIDPath = vlCreatePath(svr, VL_ANY, src, drn);
    else {
	VLDevList devlist;
	VLDev vlDev;

	vlGetDeviceList(svr, &devlist);
	vlDev = devlist.devices[devicenum].dev;
	VIDPath = vlCreatePath(svr, vlDev, src, drn);
    }

    /* Set up the hardware for and define the usage of the path */
    if (vlSetupPaths(svr, (VLPathList)&VIDPath, 1, VL_SHARE, VL_SHARE)<0)
    {
	printf("%s: can't setup path: %s\n", _progName, vlStrError(vlGetErrno()));
	exit(1);
    }

    /* Begin the data transfer */
    vlBeginTransfer(svr, VIDPath, 0, NULL);

    /* Wait until user presses a key */
    printf("Hit return to exit.\n");
    c = getc(stdin);
    
    /* End the data transfer */
    vlEndTransfer(svr, VIDPath);
    
    /* Clean up and exit */
    vlDestroyPath(svr, VIDPath);
    vlCloseVideo(svr);
}
