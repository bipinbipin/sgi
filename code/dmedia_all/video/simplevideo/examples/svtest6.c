/* $Id: svtest6.c,v 1.1 1994/04/27 19:38:36 dpb Exp $ */

/*** Capture vino input, and paste the frames to the screen ***/

#include <stdio.h>
#include <unistd.h>
#include <sv/sv.h>

int
main(void)
{
    int i, ret;
    svImage *imageInfo;

    svSelectInput(vnINPUT_INDYCAM);
    imageInfo = svNewImage();

    for (i=0; i<=15; i++) { 
	printf("frame #%d\n", i);

	if (ret = svGetFrame(imageInfo))
	    printf("svGetFrame returns %d\n", ret);

	svViewImage(imageInfo, i*10, i*10);
    }

    return 0;
}
