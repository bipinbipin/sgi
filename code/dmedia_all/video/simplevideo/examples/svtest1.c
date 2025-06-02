/* $Id: svtest1.c,v 1.2 1994/07/16 01:45:32 dpb Exp $ */

/*** Grab 3 frames from the indycam */

#include <stdio.h>
#include <sv/sv.h>

int
main(void)
{
    svImage *imageInfo;

    sleep(2);
    svSelectInput(vnINPUT_INDYCAM);
    svSetFrameCount(1);

    imageInfo = svNewImage();
    svGetFrame(imageInfo);
    svSaveImage("test1.rgb", imageInfo);
    sleep(2);

    svGetFrame(imageInfo);
    svSaveImage("test2.rgb", imageInfo);
    sleep(2);

    svGetFrame(imageInfo);
    svSaveImage("test3.rgb", imageInfo);

    svFreeImage(&imageInfo);
    return 0;
}
