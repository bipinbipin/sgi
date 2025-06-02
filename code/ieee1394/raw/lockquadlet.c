/* 
 *  This application performs lock operation on the specified register
 *  of the local/remote device. LOCK operation is performed based on the
 *  Compare & Swap 4 as defined in IEEE 1394 standard.
 */

#include <stdio.h>
#include <raw1394.h>

main(argc,argv)
int argc;
char **argv;
{
    RAW1394port port;
    RAW1394config config;
    RAW1394compareSwap* ql;
    RAW1394quadread* qr;
    int s;

    config = raw1394NewConfig();
    if (config == NULL) {
        fprintf(stderr,"LockQuadlet: raw1394NewConfig() returned null config.\n");
        exit(-1);
    }

    port = raw1394OpenPort(config);
    if (port == NULL) {
        fprintf(stderr,"LockQuadlet: raw1394OpenPort() returned null port.\n");
        exit(-1);
    }

    /* First we read the BANDWIDTH_AVAILABLE register of IRM */
    qr = (RAW1394quadread*)malloc(sizeof(RAW1394quadread));

    qr->destinationOffset = 0xfffff0000000 + RAW1394_CSR_BANDWIDTH_AVAILABLE;
    qr->generation = -1;                                /* If -1 filled in by the driver */    
    qr->destinationID = raw1394GetIRMPhyID(port);
    qr->quadletData = 0xdeadbead;                       /* Returned by the driver */
    qr->requestStatus = 0xdeadfeed;                     /* Returned by the driver */
    qr->responseCode = 0xdeadfeed;                      /* Returned by the driver */

    s=raw1394Quadread(port, qr);
    if (s!=0) {
	fprintf(stderr, "ReadQuadlet of CSR_BANDWIDTH_AVAILABLE register of the IRM failed\n");
	fprintf(stderr, "The address or operation may not be implemented by the remote node\n");
	exit(-1);
    }

    /* Next reserve 100 units of bandwidth using a lock (compareSwap) transaction */
    if (qr->quadletData<100) {
        fprintf(stderr, "Insufficient bandwidth available\n");
	exit(-1);
    }

    ql = (RAW1394compareSwap*)malloc(sizeof(RAW1394compareSwap));

    ql->destinationOffset = 0xfffff0000000 + RAW1394_CSR_BANDWIDTH_AVAILABLE;
    ql->generation = -1; 				/* If -1 filled in by the driver */
    ql->destinationID = raw1394GetIRMPhyID(port);
    ql->argValue      = qr->quadletData;		/* Original value */
    ql->dataValue     = qr->quadletData-100;		/* New bandwidth to lock */
    ql->requestStatus = 0xdeadfeed;			/* Returned by the driver */
    ql->responseCode = 0xdeadfeed;			/* Returned by the driver */

    s=raw1394CompareSwap(port, ql);

    if (s!=0) {
        fprintf(stderr, "Lock transaction failed.\n");
        fprintf(stderr, "The address or operation may not be implemented by the remote node\n");
	exit(-1);
    }

    printf("Value returned for RAW1394_CSR_BANDWIDTH_AVAILABLE: 0x%x\n",
            ql->oldValue);

    if (raw1394ClosePort(port) < 0) {
        fprintf(stderr, "LockQuadlet: raw1394ClosePort() failed\n");
        exit(-1);
    }

    exit(0);
}
