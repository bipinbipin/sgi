#include <dmedia/audio.h>
#include <math.h>
#include <stdio.h>
#include <sys/hdsp.h>
#include <sys/fcntl.h>
#include <stdlib.h>

main(int argc, char **argv)
{
	int 		dim,counter;
	double		samplingRate;
	double		frequency;
	double		arg;
	double 		argInc;
	int 		buf[5][100];


samplingRate = 100.0;
frequency = 240;
argInc = frequency*2.0*M_PI/samplingRate;
arg = M_PI / 2.0;


for (dim = 0; dim < 3; dim++)
{
	for (counter = 0; counter < (samplingRate/4); counter++, arg += argInc)
	{

		buf[dim][counter] = (short) (7.0*cos(arg));

		printf("buffer[%d][%d] - %d\n",dim,counter,buf[dim][counter]);
	}
}
for (dim = 2;dim < 5; dim++)
{
        for (counter = 0; counter < (samplingRate/4); counter++, arg += argInc)
        {

                buf[dim][counter] = (short) (7.0*cos(arg));

                printf("buffer[%d][%d] - %d\n",dim,counter,buf[dim][counter]);
        }
}


}
