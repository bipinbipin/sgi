/*
 *  This application selects for an exception condition like BUS RESET
 */

#include <stdio.h>
#include <fcntl.h>
#include <raw1394.h>

main(argc,argv)
int argc;
char **argv;
{
    RAW1394port port;
    RAW1394config config;
    fd_set fdset;
    int rc, exceptionType;

    config = raw1394NewConfig();
    if (config == NULL) {
        fprintf(stderr,"WaitForReset: raw1394NewConfig() returned null config.\n");
        exit(-1);
    }

    port = raw1394OpenPort(config);
    if (port == NULL) {
        fprintf(stderr,"WaitForReset: raw1394OpenPort() returned null port.\n");
        exit(-1);
    }

    FD_ZERO(&fdset);
    FD_SET(raw1394GetFD(port), &fdset);

    while (1) {
      printf("   Generation Number: %d\n", raw1394GetGenerationNumber(port));
      printf("   Our phy ID       : %d\n", raw1394GetPortPhyID(port));
    
      rc=select(10, NULL, NULL, &fdset, NULL);
      exceptionType=raw1394GetLastException(port);
      switch(exceptionType) {
	case RAW1394_EXCEPTION_BUSRESET:
           printf("Bus Reset Exception.\n");
	   break;
	case RAW1394_EXCEPTION_NONE:
	   printf("No Exception.\n");
	   break;
      }
    }

    /* Recommended if the above while loop was not infinitely long */

    /* if (raw1394ClosePort(port) < 0) {
     *    fprintf(stderr, "WaitForReset: raw1394ClosePort() failed");
     *    exit(-1);
     * }
     */
}
