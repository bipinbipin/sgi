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
     rv = alGetResourceByName(AL_SYSTEM, argv[1], AL_DEVICE_TYPE);
     if (!rv) {
	printf("invalid device\n");
	exit(-1);
     }
     x.param = AL_DEFAULT_OUTPUT;
     x.value.i = rv;
     if (alSetParams(AL_SYSTEM,&x, 1)<0) {
	printf("getparams failed: %d\n",oserror());
     }
}
