/* 
 *  This application sends an AVC command to a device to set it's channel in the 
 *  oPCR register to what the user desires. Note: Changing the channel in oPCR
 *  of an AVC device forces the device to playback on this new channel.
 *  
 */  

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <avc.h>

main(argc,argv)
int argc;
char **argv;
{
    AVCport port;
    AVCconfig config;
    AVCunit unit;
    int bufSize, phyID, plug, result, totalUnits, channel;

    if(argc < 3) {
	printf("Usage: %s dev channel [plug]\n",argv[0]);
	exit(1);
    }

    phyID = atoi(argv[1]);
    channel = atoi(argv[2]);

    if(argc == 4) 
	plug = atoi(argv[3]);
    else
	plug = -1;

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"setoutputchannel: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"setoutputchannel: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    totalUnits = avcGetNumUnits(port);

    if (phyID > totalUnits-1) {
	fprintf(stderr,"setoutputchannel: Only %d units on the bus.\n", totalUnits);
	exit(-1);
    }

    unit = avcNewUnit(port, phyID);

    result = avcSetUnitOutputChannel(port, unit, channel, plug);

    if (avcClosePort(port) < 0) {
	perror("setoutputchannel: avcClosePort() failed");
	exit(-1);
    }

    avcFreeUnit(unit);
    avcClosePort(port);
    exit(0);
}
