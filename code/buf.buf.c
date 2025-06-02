#include <stdio.h>
#include <math.h>

main()
{
	int 	i,j;
	int	arr[10][100];
	int	buf[10][100];
	double	arg,argInc;

argInc = 240*2.0*M_PI/44100;
arg = M_PI / 2.0;

for (i = 0; i < 100; i ++, arg += argInc )
	arr[0][i] = (short) 32767.0*cos(arg);



for (i = 0; i < 100; i ++, arg += argInc )
        arr[2][i] = (short) 32767.0*cos(arg);


for (i = 0; i < 100; i ++, arg += argInc )
        arr[3][i] = (short) 32767.0*cos(arg);


for (i = 0; i < 100; i ++, arg += argInc )
        arr[4][i] = (short) 32767.0*cos(arg);


for (i = 0; i < 100; i ++, arg += argInc )
        arr[5][i] = (short) 32767.0*cos(arg);


for (i = 0; i < 100; i ++, arg += argInc )
        arr[6][i] = (short) 32767.0*cos(arg);


for (i = 0; i < 100; i ++, arg += argInc ) 
        arr[7][i] = (short) 32767.0*cos(arg);


for (i = 0; i < 100; i ++, arg += argInc )
        arr[8][i] = (short) 32767.0*cos(arg);


for (i = 0; i < 100; i ++, arg += argInc )
        arr[9][i] = (short) 32767.0*cos(arg);


for (i = 0; i < 100; i ++, arg += argInc )
        arr[1][i] = (short) 32767.0*cos(arg);

for (j = 0; j < 10; j ++)
{

	for (i = 0; i < 100; i++, arg += argInc )
		buf[j][i] = (short) 32767*cos(arg);

}
printf("hello");
}




