/* $Id: svtest10.c,v 1.3 1994/04/29 22:56:24 dpb Exp $ */

/*** Verify that the single yuv compressed frame that we created
 *** in svtest9 is actually usable by sending it to video out.
 ***/

#include <stdio.h>
#include <sv/sv.h>

int
main(void)
{
    int ret;
    svImage *imageInfo;

    svSelectOutput(gvOUTPUT_ANALOG);
    svSetImagePacking(VL_PACKING_YVYU_422_8);

    if (ret = svLoadImage("test9.yuv", &imageInfo))
	printf("svLoadImage returns %d\n", ret);

    if (ret = svPutFrame(imageInfo))
	printf("svPutFrame returns %d\n", ret);

    svFreeImage(&imageInfo);
    sleep(5);

    return 0;
}
