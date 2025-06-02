#include<time.h>


main()
{
        clock_t time,deltime;
        long junk,i;
        float secs;
LOOP:
        printf("input loop count:  ");
        scanf("%ld",&junk);
        time = clock();
        for(i=0;i<junk;i++)
        deltime = clock() - time;
        secs = (float) deltime/CLOCKS_PER_SEC;
        printf("for %ld loops, #tics = %ld, time = %f\n",junk,deltime,secs);
        goto LOOP;
        return 0;
}
