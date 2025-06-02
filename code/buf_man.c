#include<stdio.h>

int main(void)
{

	int	counter,value,increment;
	int	bufsize = 1000;
	int 	buf[bufsize];
	
for (counter = 0, value = 1, increment = 1; counter < bufsize; counter++, value += increment) 
	{
	 	buf[counter] = (short) value;
		printf("\t%d\t%d\n",counter,buf[counter]);
	}
}
