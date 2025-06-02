/* 
 * This application Allocates Channel with the Isochronous Resource Manager of the
 * IEEE 1394 bus. The channel is allocated for isochronous transfers.
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
	printf("Usage:  %s channel (channel number to allocate)\n",argv[0]);
	exit(1);
    }

    channel = strtoul(argv[1], 0, 0);

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"allocchannel: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"allocchannel: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    result = avcAvailableChannels(port);

    fprintf(stderr,"Channel mask before allocation = 0x%llx\n", result);
    avcAllocateChannel(port, channel);

    result = avcAvailableChannels(port);
    fprintf(stderr,"Channel mask after allocation = 0x%llx\n", result);

    if (avcClosePort(port) < 0) {
	perror("allocchannel: avcClosePort() failed");
	exit(-1);
    }

    exit(0);
}
