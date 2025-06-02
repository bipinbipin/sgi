/* 
 *  This application shows the use of select() in waiting for an exception condition
 *  on the IEEE 1394 bus like BUS RESET. By default, it loops for 5 resets.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <bstring.h>
#include <avc.h>


main(argc,argv)
int argc;
char **argv;
{
    AVCport port;
    AVCconfig config;
    int result, i, numResets;
    fd_set exceptionFds;

    if (argc > 1)
	numResets = atoi(argv[1]);
    else
	numResets = 5;

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"avcNewConfig() returned null config.\n");
	exit(-1);
    }

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"avcOpenPort() returned null port.\n");
	exit(-1);
    }


    FD_ZERO(&exceptionFds);
    for  (i=0; i < numResets; i++) {
	FD_SET(avcGetFD(port), &exceptionFds);
	result = select(avcGetFD(port)+1, 0, 0, &exceptionFds, 0);
	fprintf(stderr,"Got Exception number %d!\n", avcGetLastException(port));	
    }

    if (avcClosePort(port) < 0) {
	perror("selectbusreset: avcClosePort() failed");
	exit(-1);
    }
    exit(0);
}
