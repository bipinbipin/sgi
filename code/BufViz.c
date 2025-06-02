#include <math.h>
#include <stdio.h>

main()
{
	int 		counter;
	double		arg;
	double 		argInc;
	int 		buf[11025];
	argInc = 480*M_PI/44100;
	arg = M_PI / 2.0;

for (counter = 0; counter < 11025; counter++, arg += argInc)
{
buf[counter] = (short) (32767.0*cos(arg));
}

for (counter = 0; counter < 11025; counter++)
{
printf("arg - %f\targInc - %f\tbuffer[%d] - %d\n",arg,argInc,counter,buf[counter]);
}

}
