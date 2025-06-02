#include <audio.h>

/*
 * Here is an example of how to use alGetResourceByName in a 
 * robust fashion.
 * 
 * This example takes two names, a source and a destination.
 *
 * The source can be the name of a device, like "AnalogIn,"
 * or it can be the name of an interface on a device, like "Microphone"
 * or "LineIn", both of which are interfaces on "AnalogIn" on
 * most machines.
 *
 * The destination can also be the name of a device or the
 * name of an interface on a device.
 *
 * This example will connect the given source to the given
 * destination, selecting the appropriate interface on the input
 * device, if necessary.
 *
 * See the alParams man page for information on devices
 * and interfaces.
 */

main(int argc, char **argv)
{
     int fd;
     int src, dest;
     int itf;
     int id;
     
     if (argc != 3) {
	printf("usage: %s <src> <dest>\n",argv[0]);
	exit(-1);
     }

     /*
      * First, let "src" be the device given by the first argument.
      * If the first argument happens to be the name of an interface,
      * this will find the device that has that interface. 
      * 
      * This works
      * because alGetResourceByName searches the resource hierarchy
      * for the named item, then looks for the closest resource of
      * the given type which can get to the resource it finds. Since
      * we've specified AL_DEVICE_TYPE, it will try to find a device 
      * for us.
      *
      * For example, if the user specifies "Microphone," and that
      * is an interface on AnalogIn, src will be AnalogIn.
      */

     src = alGetResourceByName(AL_SYSTEM, argv[1], AL_DEVICE_TYPE);
     if (!src) {
	printf("invalid device %s\n", argv[1]);
	exit(-1);
     }

     /*
      * Now see if the given name is an interface. If it is,
      * set the interface on the source device to be that interface.
      *
      * For example, if the user specifies "Microphone," and that
      * is an interface on AnalogIn, src will be AnalogIn.
      */
     if (itf = alGetResourceByName(AL_SYSTEM, argv[1], AL_INTERFACE_TYPE)) {
	 ALpv p;
	 
	 p.param = AL_INTERFACE;
	 p.value.i = itf;
	 if (alSetParams(src, &p, 1) < 0 || p.sizeOut < 0) {
	    printf("set interface failed\n");
	 }
     }

     /*
      * Now find dest.
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
	printf("connect failed: %d\n",oserror());
     }
     printf("connection ID is %d\n",id);
}
