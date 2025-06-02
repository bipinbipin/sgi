/*
 *  This application queries an AVC device for the "current" timecode.
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

    if(argc != 2) {
	printf("Usage:  %s node\n", argv[0]);
	exit(1);
    }

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"printtimecode: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"printtimecode: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    cmd.ctype = AVC_CTYPE_STATUS;
    cmd.subunitType = AVC_SUBUNIT_TYPE_TAPE_RECORDER;
    cmd.subunitID = 0;
    cmd.opcode = VCR_TIMECODE;
    cmd.operand[0] = VCR_TIMECODE_STATUS;
    cmd.operand[1] = 0xff;
    cmd.operand[2] = 0xff;
    cmd.operand[3] = 0xff;
    cmd.operand[4] = 0xff;

    unit = avcNewUnit(port, atoi(argv[1]));
  
    while (1) {
	avcCommand(port, unit, &cmd, &resp, 5);
	sleep(1);
        fprintf(stderr,"Response frame: response = %x; subunitType = %x; subunitID = %x; opcode = %x; operand = %x\n",
	        resp.response, resp.subunitType, resp.subunitID, resp.opcode, resp.operand[0]);
	fprintf(stderr,"%d:%d:%d:%d\n", BCDTOINT(resp.operand[4]), BCDTOINT(resp.operand[3]),
		BCDTOINT(resp.operand[2]), BCDTOINT(resp.operand[1]));
    }
    
    /* Actually, you can't get here.  The following two calls are just here to demonstrate
       good form.
     */
    avcFreeUnit(unit);
    avcClosePort(port);
}
