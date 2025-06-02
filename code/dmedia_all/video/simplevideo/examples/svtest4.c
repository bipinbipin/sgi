/* $Id: svtest4.c,v 1.1 1994/04/27 19:38:36 dpb Exp $ */

/*** Paste a series of images to the screen **/

#include <stdio.h>
#include <unistd.h>
#include <sv/sv.h>

int
main(void)
{
    svImage *imageInfo;

    svLoadImage("test1.rgb", &imageInfo);
    svViewImage(imageInfo, 10, 10);
    svFreeImage(&imageInfo);

    svLoadImage("test2.rgb", &imageInfo);
    svViewImage(imageInfo, 50, 50);
    svFreeImage(&imageInfo);

    svLoadImage("test3.rgb", &imageInfo);
    svViewImage(imageInfo, 100, 100);
    svFreeImage(&imageInfo);

    sleep(10);
    return 0;
}
