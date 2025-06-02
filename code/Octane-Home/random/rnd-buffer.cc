#include <stdio.h>
#include <stdlib.h>


main()
{
	float rndnum_vert[160][3];
	int i,j;

	for (i = 0;i < 160;i++)
	 {
		for (j = 0;j < 3;j++)
		  {
		    rndnum_vert[i][j] = mrand48();
		  }
		for (j = 0;j < 3;j++)
		  {
		    printf("%f\t",rndnum_vert[i][j]);
		  }
	   printf("\n");
	 }
}
