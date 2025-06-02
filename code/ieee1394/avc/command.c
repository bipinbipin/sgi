/*
 *  This application sends an AV/C command to a certain phy id. AV/C commands are
 *  constructed based on IEC's 61883 specification.
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
    AVCcommandframe cmd;
    AVCresponseframe resp;
    int operand;
    
    bzero(&cmd, sizeof(AVCcommandframe));

    if(argc != 7) {
	printf("Usage:  %s node command-type subunit-type subunit-id opcode operand\n",
	       argv[0]);
	printf("For examples, see stop, play, rewind.\n");
	exit(1);
    }

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"command: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    avcSetBufferSize(config, 2); 

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"command: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    operand = strtoul(argv[6], 0, 0);

    cmd.ctype = strtoul(argv[2], 0, 0);
    cmd.subunitType = strtoul(argv[3], 0, 0);
    cmd.subunitID = strtoul(argv[4], 0, 0);
    cmd.opcode = strtoul(argv[5], 0, 0);
    cmd.operand[0] = operand;

    unit = avcNewUnit(port, atoi(argv[1]));
    avcCommand(port, unit, &cmd, &resp, 1);

    fprintf(stderr,"Response frame: response = %x; subunitType = %x; subunitID = %x; opcode = %x; operand = %x\n",
	    resp.response, resp.subunitType, resp.subunitID, resp.opcode, resp.operand[0]);


    if (avcFreeUnit(unit) < 0) {
	perror("command: avcFreeUnit() failed");
	exit(-1);
    }

    if (avcClosePort(port) < 0) {
	perror("command: avcClosePort() failed");
	exit(-1);
    }

    exit(0);
}
