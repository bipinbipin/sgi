/* 
 *  This application shows how to setup a config and port to do cool stuff!
 */

#include <stdio.h>
#include <avc.h>

main(argc,argv)
int argc;
char **argv;
{
    AVCport port;
    AVCconfig config;
    int bufSize;

    if (argc == 1)
	bufSize = 30;			/* No of frames */
    else
	bufSize = atoi(argv[1]);	/* No of frames */

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"openbus: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    avcSetBufferSize(config, bufSize); 
    avcSetSpeed(config, AVC_SPEED_400); 

    if (avcGetBufferSize(config) != bufSize) {
	fprintf(stderr,"openbus: avcGetBufferSize() return bogus buffer size.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"openbus: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    /* Implement additional functions here */

    if (avcClosePort(port) < 0) {
	perror("openbus: avcClosePort() failed");
	exit(-1);
    }

    exit(0);
}
