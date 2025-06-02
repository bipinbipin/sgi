/* 
 *  This application displays Unique ID and unit information for an AVC device
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
    AVCcommandframe cmd;
    AVCresponseframe resp;
    int totalUnits, phyID;

    if (argc < 2) {
	fprintf(stderr,"usage: %s <phyID>\n", argv[0]);	
	exit(-1);
    }

    phyID = atoi(argv[1]);

    if (0 > phyID > 63) {
	fprintf(stderr,"displayunit: bogus phyID.\n");
	exit(-1);
    }

    config = avcNewConfig();

    avcSetBufferSize(config, 10);

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"displayunit: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    totalUnits = avcGetNumUnits(port);
    if (phyID > totalUnits-1) {
	fprintf(stderr,"displayunit: Only %d units on the bus.\n", totalUnits);
	exit(-1);
    }

    unit = avcNewUnit(port, phyID);

    cmd.ctype = AVC_CTYPE_STATUS;
    cmd.subunitType = 0x1f ;
    cmd.subunitID = 7;
    cmd.opcode = AVC_UNIT_INFO;
    cmd.operand[0] = 0xff;
    cmd.operand[1] = 0xff;
    cmd.operand[2] = 0xff;
    cmd.operand[3] = 0xff;
    cmd.operand[4] = 0xff;

    avcCommand(port, unit, &cmd, &resp, 5);

    fprintf(stderr,"Unit %d:  UID = 0x%llx; VendorID = 0x%llx; DeviceID = 0x%llx;\n",
	    phyID, avcGetUnitUID(unit), avcGetUnitVendorID(unit), avcGetUnitDeviceID(unit));
    fprintf(stderr,"AV query: Unit type = %x;  Unit ID = %x\n",
	    resp.operand[1]>>3, resp.operand[1]&7);
    
    if (avcFreeUnit(unit) < 0) {
	perror("displayunit: avcFreeUnit() failed");
	exit(-1);
    }
    if (avcClosePort(port) < 0) {
	perror("displayunit: avcClosePort() failed");
	exit(-1);
    }

    exit(0);
}
