/* $Id: svtest5.c,v 1.2 1994/07/16 01:46:17 dpb Exp $ */

/*** 15 times, capture a frame and send it to galileo video out ***/

#include <stdio.h>
#include <unistd.h>
#include <sv/sv.h>

int
main(void)
{
    int i, ret;
    svImage *imageInfo;
    svContext context1, context2;

    context1 = svNewContext();
    svSelectInput(gvINPUT_COMPOSITE_1);
    svRecoverFromPreemption();

    context2 = svNewContext();
    svSelectOutput(gvOUTPUT_ANALOG);
    svRecoverFromPreemption();

    imageInfo = svNewImage();

    for (i=0; i<=15; i++) { 
	printf("frame #%d\n", i);

	svSetContext(context1);
	if (ret = svGetFrame(imageInfo))
	    printf("svGetFrame returns %d\n", ret);

	svViewImage(imageInfo, i*10, i*10);

	svSetContext(context2);
	if (ret = svPutFrame(imageInfo)) {
	    printf("svPutFrame returns %d\n", ret);
	    printf("vlerrno is %d\n", vlGetErrno());
	}
	sleep(2);
    }

    return 0;
}
