/* 
 *  This application displays the Unique ID, Vendor ID, and Device ID for
 *  all nodes on the IEEE 1394 bus.
 */

#include <stdio.h>
#include <avc.h>

main(argc,argv)
int argc;
char **argv;
{
    AVCport port;
    AVCconfig config;
    AVCunit unit;
    int bufSize, generationNum;

    if (argc > 1)

    _AVC_debug_level = atoi(argv[1]);

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"displaytree: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"displaytree: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    generationNum = -1;
    
    while (1) {
	int i, numUnits;
	if (generationNum != (i = avcGetGenerationNumber(port))) {
	    generationNum = i;
	    fprintf(stderr,"Generation Number = %d\n", generationNum);
	    numUnits = avcGetNumUnits(port);
	    for (i = 0; i < numUnits; i++) {
		unit = avcNewUnit(port, i);
		fprintf(stderr,"  Unit %d:  UID = 0x%llx; VendorID = 0x%llx; DeviceID = 0x%llx\n",
			i, avcGetUnitUID(unit), avcGetUnitVendorID(unit), avcGetUnitDeviceID(unit));
		avcFreeUnit(unit);
	    }
	    printf("Waiting for busreset\n");
	}

    sleep(2);
    }
}
