#include <audio.h>

/*
 * a simple example of using alQueryValues with qualifiers.
 * Find any kind of digital input on the system.
 */
main()
{
	ALvalue x[32];
	ALpv y;
	int n,i;

	y.param = AL_INTERFACE;
	y.value.i = AL_DIGITAL_IF_TYPE;

	/* 
	 * The following query says "find me any input devices with an
	 * interface which is a subtype of AL_DIGITAL_IF_TYPE" -- this will
	 * find any kind of digital input devices
	 */
	n = alQueryValues(AL_SYSTEM,AL_DEFAULT_INPUT, x, 32, &y, 1);
	if (n < 0) {
		printf("alQueryValues error:%s\n",alGetErrorString(oserror()));
	}

	/* 
	 * at this point, x[i].i is a list of digital input devices, and "n"
	 * is the number of such devices found.
	 * The following code gets the name of each device and prints it.
	 * You don't need this code if all you want to do is know if there
	 * are any such devices
	 */
	for (i = 0; i < n; i++) {
		char name[32];
		ALpv z;

		z.param = AL_LABEL;
		z.value.ptr = name;
		z.sizeIn = 32;
		alGetParams(x[i].i, &z, 1);
		printf("found %d: %s\n", x[i].i, name);
	}
}
