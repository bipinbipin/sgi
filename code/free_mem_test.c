#include<stdio.h>
#include<stdlib.h>
#include<conio.h>
#include<malloc.h>

void main()
{
        unsigned char huge *array;
        long size;
        size = 8000000;
#ifdef _MSC_VER
        while((array = (unsigned char huge *) halloc(size,1)) == NULL)
#else
        while((array = (unsigned char huge *) farmalloc(size)) == NULL)
#endif
                size -= 100;
#ifdef _MSC_VER
        hfree(array);
#else
        farfree(array);
#endif
        printf("Largest Allocatable Block is %ld Bytes\n",size);
        getch();
        return;
}
