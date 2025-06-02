/* 
 *  This application retrieves input and output channels from an AVC device's
 *  Plug Control Registers (PCR).
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
    int bufSize, phyID, plug, result, totalUnits;

    if(argc < 2) {
	printf("Usage: %s dev [plug]\n",argv[0]);
	exit(1);
    }

    phyID = atoi(argv[1]);

    if(argc == 3) 
	plug = atoi(argv[2]);
    else
	plug = 0;

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"getunitchannels: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"getunitchannels: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    totalUnits = avcGetNumUnits(port);

    if (phyID > totalUnits-1) {
	fprintf(stderr,"getunitchannels: Only %d units on the bus.\n", totalUnits);
	exit(-1);
    }

    unit = avcNewUnit(port, phyID);

    result = avcGetUnitOutputChannel(port, unit, plug);
    fprintf(stderr,"Output Channel for unit %d, plug %d = %d\n", phyID, plug, result);

    result = avcGetUnitInputChannel(port, unit, plug);
    fprintf(stderr,"Input Channel for unit %d, plug %d = %d\n", phyID, plug, result);

    if (avcFreeUnit(unit) < 0) {
	perror("getunitchannels: avcFreeUnit() failed");
	exit(-1);
    }
    if (avcClosePort(port) < 0) {
	perror("getunitchannels: avcClosePort() failed");
	exit(-1);
    }

    exit(0);
}
