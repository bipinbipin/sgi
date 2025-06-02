/* $Id: svtest9.c,v 1.4 1994/04/29 22:56:13 dpb Exp $ */

/*** Grab a single galileo frame, save it as a yuv compressed image;
 ***/

#include <stdio.h>
#include <sv/sv.h>

int
main(void)
{
    int ret;
    svImage *imageInfo;

    svSelectInput(gvINPUT_COMPOSITE_1);
    svSetImagePacking(VL_PACKING_YVYU_422_8);
    svCompressedImages(TRUE);

    imageInfo = svNewImage();
    if (ret = svGetFrame(imageInfo))
	printf("svGetFrame returns %d\n", ret);

    if (ret = svSaveImage("test9.yuv", imageInfo))
	printf("svSaveImage returns %d\n", ret);

    svFreeImage(&imageInfo);
    return 0;
}
