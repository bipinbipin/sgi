/* 
 *  This application Free's Bandwidth with the Isochronous Resource Manager of
 *  the IEEE 1394 bus. The free'd bandwidth is specified in terms of 1394 bandwidth
 *  units
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
    int incrBandwidth, availBandwidth;

    if(argc != 2) {
	printf("Usage:  %s bandwidth (to be freed)\n",argv[0]);
	exit(1);
    }

    incrBandwidth = strtoul(argv[1], 0, 0);

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"freebandwidth: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"freebandwidth: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    availBandwidth = avcAvailableBandwidth(port);
    fprintf(stderr,"Available bandwidth before de-allocation = %d\n", availBandwidth);

    avcFreeBandwidth(port, incrBandwidth);

    availBandwidth = avcAvailableBandwidth(port);
    fprintf(stderr,"Available bandwidth after de-allocation = %d\n", availBandwidth);

    if (avcClosePort(port) < 0) {
	perror("freebandwidth: avcClosePort() failed");
	exit(-1);
    }

    exit(0);
}
