/* 
 *  This application sends out an asynchronous block write command to the specified node.
 */

#include <stdio.h>
#include <raw1394.h>

main(argc,argv)
int argc;
char **argv;
{
    RAW1394port port;
    RAW1394config config;
    RAW1394blockwrite* bw;
    int i,s;

    config = raw1394NewConfig();
    if (config == NULL) {
        fprintf(stderr,"WriteBlock: raw1394NewConfig() returned null config.\n");
        exit(-1);
    }

    port = raw1394OpenPort(config);
    if (port == NULL) {
        fprintf(stderr,"WriteBlock: raw1394OpenPort() returned null port.\n");
        exit(-1);
    }

    /* XXX: Need to get a valid 'writeblock' transaction (maybe AVC) */

    bw = (RAW1394blockwrite*)malloc(sizeof(RAW1394blockwrite));

    bw->destinationOffset = 0xfffff0003000;
    bw->generation = -1;				/* If -1 filled in by the driver */
    bw->destinationID = raw1394GetIRMPhyID(port); 
    bw->dataLength = 16;
    bw->payload[0]=0xabababab; bw->payload[1]=0xcdcdcdcd;
    bw->payload[2]=0xefefefef; bw->payload[3]=0xffffffff;
    bw->requestStatus = 0xdeadfeed;			/* Returned by the driver */
    bw->responseCode = 0xdeadfeed;			/* Returned by the driver */

    s=raw1394Blockwrite(port, bw);

    if (s!=0) {
        fprintf(stderr, "Block Write transaction failed.\n");
	fprintf(stderr, "The address or operation may not be implemented by the remote node.\n");
        exit(-1);
    }

    if (raw1394ClosePort(port) < 0) {
        fprintf(stderr, "WriteBlock: raw1394ClosePort() failed\n");
        exit(-1);
    }

    exit(0);
}
