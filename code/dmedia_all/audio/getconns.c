#include <audio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>


#define STRING_SIZE 32

void
dump_connections(ALvalue *x, int rv)
{
    ALpv y[3];
    char dn[STRING_SIZE];
    char sn[STRING_SIZE];
    int src,dest;
    int i;
    
    for (i = 0; i < rv; i++) {
	/*
	 * Get the source, destination, and sample-rate of the
	 * connection. 
	 */
	y[0].param = AL_SOURCE;
	y[1].param = AL_DEST;
	y[2].param = AL_RATE;
	alGetParams(x[i].i,y,3);

	src = y[0].value.i;
	dest = y[1].value.i;

	/*
	 * Get the text labels associated with the source and
	 * destination resources, so our printout is more readable.
	 */
	y[0].param = AL_LABEL;
	y[0].value.ptr = sn;
	y[0].sizeIn = STRING_SIZE;	/* pass in max size of string */
	alGetParams(src,y,1);		/* get the source name */

	/* we can re-use "y" for another getparams by just changing one
	   field: point it at a different string */

	y[0].value.ptr = dn;
	alGetParams(dest,y,1);		/* get the destination name */

	printf("found %d (%s->%s) -- rate %lf\n",
		x[i].i, sn,dn,alFixedToDouble(y[2].value.ll));
    }
}

main()
{
     int fd,i;
     int rv;
     ALvalue x[32];
     ALpv y[16];
     char n[32];
     int l[32];
     
     printf("Connections in use:\n");

     /*
      * We call alQueryValues to get the set of connections
      * on the system. 
      */
     if ((rv= alQueryValues(AL_SYSTEM,AL_CONNECTIONS,x,16,0,0))>=0) {
	dump_connections(x, rv);
     }
     else {
        printf("queryset failed: %d\n",oserror());
     }
}
