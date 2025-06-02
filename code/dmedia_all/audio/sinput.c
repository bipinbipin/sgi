#include <audio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>

main(int argc, char **argv)
{
     int fd;
     int rv;
     ALpv x;
     
     if (argc != 2) {
	printf("usage: %s <device>\n",argv[0]);
	exit(-1);
     }

     /*
      * Get a device resource from the given name 
      */
     rv = alGetResourceByName(AL_SYSTEM, argv[1], AL_DEVICE_TYPE);
     if (!rv) {
	printf("invalid device\n");
	exit(-1);
     }

     /*
      * Set that resource as the global default input
      */
     x.param = AL_DEFAULT_INPUT;
     x.value.i = rv;
     if (alSetParams(AL_SYSTEM,&x, 1)<0) {
	printf("setparams failed: %d\n",alGetErrorString(oserror()));
     }

     /*
      * alSetParams sets sizeOut < 0 if an individual parameter had
      * an error.
      */
     if (x.sizeOut < 0) {
	printf("%s was not accepted as default input\n",argv[1]);
     }
}
