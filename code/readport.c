#include <stdio.h>

main()
{
        unsigned paddr,pdata;

LOOP:
        printf("Input port address (hex): ");
        scanf("%x",&paddr);
        pdata = inp(paddr);
        printf("Port(%xh) = %xh\n",paddr,pdata);

        goto LOOP;
        return 0;
}
