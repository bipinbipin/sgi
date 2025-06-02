#include <audio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>

#define STRING_SIZE 32

void
dump_resources(ALvalue *x, int rv)
{
    ALpv y[2];
    char dn[STRING_SIZE];
    char dl[STRING_SIZE];
    int i;
    
    for (i = 0; i < rv; i++) {

	/*
	 * Get the text labels associated with the source and
	 * destination resources, so our printout is more readable.
	 */
	y[0].param = AL_LABEL;
	y[0].value.ptr = dl;
	y[0].sizeIn = STRING_SIZE;	/* pass in max size of string */
	y[1].param = AL_NAME;
	y[1].value.ptr = dn;
	y[1].sizeIn = STRING_SIZE;	/* pass in max size of string */
	alGetParams(x[i].i,y,2);	/* get the resource name & label */

	if (alIsSubtype(AL_DEVICE_TYPE, x[i].i)) {
     		ALvalue z[32];
		int nres;

		/* 
		 * If this is a device, print its interfaces
		 */
		printf("\nDevice name '%s' label '%s':\n",dn,dl);
     		/*
     		 * We call alQueryValues to get the set of interfaces
     		 * on this device
     		 */
     		if ((nres= alQueryValues(x[i].i,AL_INTERFACE,z,16,0,0))>=0) {
			dump_resources(z, nres);
     		}
     		else {
        	    printf("queryset failed: %s\n",alGetErrorString(oserror()));
     		}
	}
	else {
		/*
		 * This program only lists devices and interfaces,
		 * so if it's not a device, assume it's an interface
		 */
		printf("	Interface name '%s' label '%s':\n",dn,dl);
	}

    }
}

main()
{
     int fd,i;
     int rv;
     ALvalue x[32];
     
     printf("Devices and Interfaces on this system:\n");

     /*
      * We call alQueryValues to get the set of devices
      * on the system. 
      */
     if ((rv= alQueryValues(AL_SYSTEM,AL_DEVICES,x,16,0,0))>=0) {
	dump_resources(x, rv);
     }
     else {
        printf("queryset failed: %d\n",oserror());
     }
}
