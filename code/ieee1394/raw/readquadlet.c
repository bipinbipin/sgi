/* 
 *  This application sends a quadlet read request command to the specified node.
 */

#include <stdio.h>
#include <raw1394.h>

main(argc,argv)
int argc;
char **argv;
{
    RAW1394port port;
    RAW1394config config;
    RAW1394quadread* qr;
    int s;

    config = raw1394NewConfig();
    if (config == NULL) {
        fprintf(stderr,"ReadQuadlet: raw1394NewConfig() returned null config.\n");
        exit(-1);
    }

    port = raw1394OpenPort(config);
    if (port == NULL) {
        fprintf(stderr,"ReadQuadlet: raw1394OpenPort() returned null port.\n");
        exit(-1);
    }

    qr = (RAW1394quadread*)malloc(sizeof(RAW1394quadread));

    qr->destinationOffset = 0xfffff0000000 + RAW1394_CSR_BANDWIDTH_AVAILABLE;
    qr->generation = -1; 				/* If -1 filled in by the driver */
    qr->destinationID = raw1394GetIRMPhyID(port);
    qr->quadletData = 0xdeadbead;			/* Returned by the driver */
    qr->requestStatus = 0xdeadfeed;			/* Returned by the driver */
    qr->responseCode = 0xdeadfeed;			/* Returned by the driver */

    s=raw1394Quadread(port, qr);

    if (s!=0) {
        fprintf(stderr, "Quadlet Read transaction failed.\n");
	fprintf(stderr, "The address of operation may not be implemented by remote node\n");
        exit(-1);
    }

    printf("Quadread of RAW1394_CSR_BANDWIDTH_AVAILABLE reports 0x%x\n",
            qr->quadletData);

    if (raw1394ClosePort(port) < 0) {
        fprintf(stderr, "ReadQuadlet: raw1394ClosePort() failed\n");
        exit(-1);
    }

    exit(0);
}
