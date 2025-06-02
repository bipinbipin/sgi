#include <audio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>

/*
 * Here is a very simple example of how to use alConnect
 * and alGetResourceByName.
 */
main(int argc, char **argv)
{
     int fd;
     int src, dest;
     int id;
     
     if (argc != 3) {
	printf("usage: %s <src> <dest>\n",argv[0]);
	exit(-1);
     }

     /*
      * Find a source device from the given name.
      */
     src = alGetResourceByName(AL_SYSTEM, argv[1], AL_DEVICE_TYPE);
     if (!src) {
	printf("invalid device %s\n", argv[1]);
	exit(-1);
     }

     /*
      * Find a destination device from the given name.
      */
     dest = alGetResourceByName(AL_SYSTEM, argv[2], AL_DEVICE_TYPE);
     if (!dest) {
	printf("invalid device %s\n", argv[2]);
	exit(-1);
     }

     /*
      * Connect src and dest. Note that this type of connection is persistent,
      * and will remain after this program exits. Use the 
      * alDisconnect call to remove the connection; see "disconnect.c"
      * for a simple example of alDisconnect.
      */

     if ((id = alConnect(src, dest, 0, 0))<0) {
	printf("connect failed: %s\n",alGetErrorString(oserror()));
     }
     else {
        printf("connection ID is %d\n",id);
     }
}
