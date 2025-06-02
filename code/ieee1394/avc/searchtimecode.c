/*  
 *  This application sends the search AVC command to an AVC device on the bus.
 *
 *  Note: Not all IEEE 1394 devices support the search AVC command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <avc.h>
#include <avc_vcr.h>

/*
 *  Convert to and from 2-digit Binary Coded Decimal
 */

#define  BCDTOINT( bcd )    (((bcd) >> 4) * 10 + ((bcd) & 0xf))
#define  INTTOBCD( i )      ((((i) / 10) << 4) | ((i) % 10))

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

    if(argc != 6) {
	printf("Usage:  %s node hh mm ss ff\n", argv[0]);
	exit(1);
    }

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"searchtimecode: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"searchtimecode: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    cmd.ctype = AVC_CTYPE_CONTROL;

    cmd.subunitType = AVC_SUBUNIT_TYPE_TAPE_RECORDER;
    cmd.subunitID = 0;
    cmd.opcode = VCR_TIMECODE;
    cmd.operand[0] = VCR_TIMECODE_CONTROL;
    cmd.operand[1] = INTTOBCD(atoi(argv[5]));
    cmd.operand[2] = INTTOBCD(atoi(argv[4]));
    cmd.operand[3] = INTTOBCD(atoi(argv[3]));
    cmd.operand[4] = INTTOBCD(atoi(argv[2]));

    fprintf(stderr,"Searching to %d:%d:%d:%d\n", BCDTOINT(cmd.operand[4]), BCDTOINT(cmd.operand[3]),
	    BCDTOINT(cmd.operand[2]), BCDTOINT(cmd.operand[1]));

    unit = avcNewUnit(port, atoi(argv[1]));
  
    avcCommand(port, unit, &cmd, &resp, 5);
    
    fprintf(stderr,"Response frame: response = %x; subunitType = %x; subunitID = %x; opcode = %x; operand = %x\n",
	    resp.response, resp.subunitType, resp.subunitID, resp.opcode, resp.operand[0]);

    cmd.ctype = AVC_CTYPE_STATUS;
    cmd.operand[0] = VCR_TIMECODE_STATUS;
    cmd.operand[1] = 0xff;
    cmd.operand[2] = 0xff;
    cmd.operand[3] = 0xff;
    cmd.operand[4] = 0xff;

    avcFreeUnit(unit);
    avcClosePort(port);
    return (0);
}
