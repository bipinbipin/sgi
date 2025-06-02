#include <stdio.h>
#include <stdlib.h>


main()
{
	double rndnum;
	
	for (;;)
	 {
	  rndnum = lrand48();
	  printf("%f\t%o\t%u\t%x\n",rndnum);
	 }
}
