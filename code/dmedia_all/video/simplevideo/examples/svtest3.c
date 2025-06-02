/* $Id: svtest3.c,v 1.4 1994/07/22 20:39:38 dpb Exp $ */

/*** Send a series of rgb files to the galileo output ***/

#include <stdio.h>
#include <unistd.h>
#include <sv/sv.h>
#include <vl/vl.h>


void setupPathCallback(VLServer svr, VLPath path, VLNode src, VLNode drn)
{
#ifdef notdef
    vlConversionAdvise(svr, path, 
		       VL_PACKING_RGB_8, VL_PACKING_YVYU_422_8,
		       42);
#endif
}

int
main(void)
{
    svImage *imageInfo;

    svSetupPathCallback(setupPathCallback);

    svSelectOutput(gvOUTPUT_ANALOG);

    svLoadImage("test1.rgb", &imageInfo);
    svPutFrame(imageInfo);
    svFreeImage(&imageInfo);
    sleep(2);

    svLoadImage("test2.rgb", &imageInfo);
    svPutFrame(imageInfo);
    svFreeImage(&imageInfo);
    sleep(2);

    svLoadImage("test3.rgb", &imageInfo);
    svPutFrame(imageInfo);
    svFreeImage(&imageInfo);
    sleep(2);

    return 0;
}
