/* 
 *  This application sends a block read command to the specified node.
 */

#include <stdio.h>
#include <raw1394.h>

main(argc,argv)
int argc;
char **argv;
{
    RAW1394port port;
    RAW1394config config;
    RAW1394blockread* br;
    RAW1394quadread* qr;
    int generationNumber;
    int i,s;

    config = raw1394NewConfig();
    if (config == NULL) {
        fprintf(stderr,"ReadBlock: raw1394NewConfig() returned null config.\n");
        exit(-1);
    }

    port = raw1394OpenPort(config);
    if (port == NULL) {
        fprintf(stderr,"ReadBlock: raw1394OpenPort() returned null port.\n");
        exit(-1);
    }

    generationNumber=raw1394GetGenerationNumber(port);

    /* First we check the size of TOPOMAP on the IRM by means of a ReadQuadlet transaction */
    qr = (RAW1394quadread*)malloc(sizeof(RAW1394quadread));

    qr->destinationOffset = 0xfffff0000000 + RAW1394_CSR_TOPOLOGY_MAP;
    qr->generation = generationNumber; 
    qr->destinationID = raw1394GetIRMPhyID(port); 
    qr->quadletData = 0xdeadbead;                       /* Returned by the driver */
    qr->requestStatus = 0xdeadfeed;                     /* Returned by the driver */
    qr->responseCode = 0xdeadfeed;                      /* Returned by the driver */

    s=raw1394Quadread(port, qr);

    if (s!=0) {
        fprintf(stderr, "Quadlet Read transaction failed. \n");
	fprintf(stderr, "The address or operation may not be implemented by the remote node. \n");
        exit(-1);
    }

    /* Next we read the rest of topology map of the IRM node */
    br = (RAW1394blockread*)malloc(sizeof(RAW1394blockread));

    br->destinationOffset = 0xfffff0000000 + RAW1394_CSR_TOPOLOGY_MAP;
    br->generation = generationNumber;			/* To make sure that the bus generation has not changed */
    br->destinationID = raw1394GetIRMPhyID(port); 
    br->dataLength = ((qr->quadletData>>16)&0xffff)*4 + 4;
    br->requestStatus = 0xdeadfeed;			/* Returned by the driver */
    br->responseCode = 0xdeadfeed;			/* Returned by the driver */

    s=raw1394Blockread(port, br);

    if (s!=0) {
        fprintf(stderr, "Block Read transaction failed. \n");
        fprintf(stderr, "The address or operation may not be implemented by the remote node. \n");
        exit(-1);
    }

    printf("Blockread of RAW1394_CSR_TOPOLOGY_MAP reports:\n");
    for (i=0; i<=((qr->quadletData>>16)&0xffff); i++) {
	printf("TopoMap[%d]: 0x%x\n", i, br->payload[i]);
    }

    if (raw1394ClosePort(port) < 0) {
        fprintf(stderr, "ReadBlock: raw1394ClosePort() failed\n");
        exit(-1);
    }

    exit(0);
}
