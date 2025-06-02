/* $Id: svtest7.c,v 1.5 1997/07/25 21:00:53 grant Exp $ */

/*** Continuous transfer of frames from vino to galileo;
 *** Use discrete transfers, rgb images;
 ***/

#include <stdio.h>
#include <unistd.h>
#include <vl/vl.h>
#include <sv/sv.h>

int
main(void)
{
    int ret;
    svImage *imageInfo;
    svContext context1, context2;

    context1 = svCurrentContext();
    svSelectInput(vnINPUT_INDYCAM);
    svSetFrameCount(2);
    svSetImagePacking(VL_PACKING_YVYU_422_8);

    context2 = svNewContext();
    svSelectOutput(gvOUTPUT_ANALOG);
    svSetFrameCount(2);
    svSetImagePacking(VL_PACKING_YVYU_422_8);

    imageInfo = svNewImage();

    while (1) {
	svSetContext(context1);
	if (ret = svGetFrame(imageInfo))
	    printf("svGetFrame returns %d\n", ret);

	svSetContext(context2);
	if (ret = svPutFrame(imageInfo))
	    printf("svPutFrame returns %d\n", ret);
    }

    return 0;
}
