/* 
 *  This application forces a reset on the IEEE 1394 bus.
 *
 */

#include <stdio.h>
#include <raw1394.h>

main(argc,argv)
int argc;
char **argv;
{
    RAW1394port port;
    RAW1394config config;

    config = raw1394NewConfig();
    if (config == NULL) {
        fprintf(stderr,"BusReset: raw1394NewConfig() returned null config.\n");
        exit(-1);
    }

    port = raw1394OpenPort(config);
    if (port == NULL) {
        fprintf(stderr,"BusReset: raw1394OpenPort() returned null port.\n");
        exit(-1);
    }

    fprintf(stderr,"   Generation Number before BusReset: %d\n", raw1394GetGenerationNumber(port));
    fprintf(stderr,"   Our phy ID        before BusReset: %d\n", raw1394GetPortPhyID(port));

    fprintf(stderr,"Forcing a bus reset...\n");
    if (raw1394ForceBusReset(port) < 0) {
        perror("BusReset: raw1394ForceBusReset() failed");
        exit(-1);
    }

    fprintf(stderr,"   Generation Number after BusReset: %d\n", raw1394GetGenerationNumber(port));
    fprintf(stderr,"   Our phy ID        after BusReset: %d\n", raw1394GetPortPhyID(port));

    if (raw1394ClosePort(port) < 0) {
        perror("BusReset: raw1394ClosePort() failed");
        exit(-1);
    }

    exit(0);
}
