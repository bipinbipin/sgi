#include <audio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>

/*
 * A very simple example of how to use alDisconnect.
 * Disconnect a connection given its resource ID.
 */
main(int argc, char **argv)
{
    int conn;
     
     if (argc != 2) {
	printf("usage: %s <conn>\n",argv[0]);
	exit(-1);
     }
     conn = atoi(argv[1]);
     if (alDisconnect(conn)<0) {
	printf("disconnect failed: %s\n",alGetErrorString(oserror()));
     }
}
