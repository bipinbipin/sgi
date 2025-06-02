/* 
 * This application Allocates Bandwidth with the Isochronous Resource Manager of the
 * IEEE 1394 bus. The bandwidth is allocated in 1394 bandwidth units for isochronous
 * transfers.
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
    int bufSize, incrBandwidth, availBandwidth;

    if(argc != 2) {
	printf("Usage:  %s bandwidth (B/W to allocate in IEEE1394 bandwidth units)\n",argv[0]);
	exit(1);
    }

    incrBandwidth = strtoul(argv[1], 0, 0);

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"allocbandwidth: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"allocbandwidth: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    availBandwidth = avcAvailableBandwidth(port);
    fprintf(stderr,"Available bandwidth before allocation = %d\n", availBandwidth);

    avcAllocateBandwidth(port, incrBandwidth);

    availBandwidth = avcAvailableBandwidth(port);
    fprintf(stderr,"Available bandwidth after allocation = %d\n", availBandwidth);

    if (avcClosePort(port) < 0) {
	perror("allocbandwidth: avcClosePort() failed");
	exit(-1);
    }

    exit(0);
}
