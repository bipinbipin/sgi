/* 
 *  This application sends out a quadlet write request command to a local/remote node.
 */

#include <stdio.h>
#include <raw1394.h>

main(argc,argv)
int argc;
char **argv;
{
    RAW1394port port;
    RAW1394config config;
    RAW1394quadwrite* qw;
    int s;

    config = raw1394NewConfig();
    if (config == NULL) {
        fprintf(stderr,"WriteQuadlet: raw1394NewConfig() returned null config.\n");
        exit(-1);
    }

    port = raw1394OpenPort(config);
    if (port == NULL) {
        fprintf(stderr,"WriteQuadlet: raw1394OpenPort() returned null port.\n");
        exit(-1);
    }

    qw = (RAW1394quadwrite*)malloc(sizeof(RAW1394quadwrite));

    qw->destinationOffset = 0xfffff0000b00;
    qw->generation = -1; 				/* If -1 filled in by the driver */
    qw->destinationID = raw1394GetIRMPhyID(port);	/* Assuming that the IRM is an AVC device */
    qw->quadletData = 0x0020c338;			/* AVC Play command */
    qw->requestStatus = 0xdeadfeed;			/* Returned by the driver */
    qw->responseCode = 0xdeadfeed;			/* Returned by the driver */

    s=raw1394Quadwrite(port, qw);

    if (s!=0) {
        fprintf(stderr, "Quadlet Write transaction failed.\n");
        fprintf(stderr, "The address or operation may not be implemented by the remote node.\n");
        exit(-1);
    }

    printf("Quadwrite of AVC command register reports 0x%x\n",
            qw->quadletData);

    if (raw1394ClosePort(port) < 0) {
        fprintf(stderr, "WriteQuadlet: raw1394ClosePort() failed\n");
        exit(-1);
    }

    exit(0);
}
