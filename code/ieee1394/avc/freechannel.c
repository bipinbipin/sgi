/*  
 *  This application Free's an allocated channel with the Isochronous Resource Manager of the
 *  IEEE 1394 bus. 
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
    int channel;
    unsigned long long result;

    if(argc != 2) {
	printf("Usage:  %s channel (to be freed)\n",argv[0]);
	exit(1);
    }

    channel = atoi(argv[1]);

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"freechannel: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"freechannel: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    result = avcAvailableChannels(port);
    fprintf(stderr,"Channel mask before de-allocation = 0x%llx\n", result);

    avcFreeChannel(port, channel);

    result = avcAvailableChannels(port);
    fprintf(stderr,"Channel mask after de-allocation = 0x%llx\n", result);

    if (avcClosePort(port) < 0) {
	perror("freechannel: avcClosePort() failed");
	exit(-1);
    }

    exit(0);
}
