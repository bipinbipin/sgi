/* $Id: svtest8.c,v 1.4 1994/04/29 23:15:20 dpb Exp $ */

/*** Continuous transfer of frames from vino to galileo;
 *** Use continuous transfers, yuv images;
 ***/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sv/sv.h>

int frameCount = 0;
struct timeval startTime, now;

void
intHandler(void)
{
    double delta_t, fps;

    gettimeofday(&now);
    
    now.tv_sec  -= startTime.tv_sec;
    now.tv_usec -= startTime.tv_usec;
    
    delta_t = (double)now.tv_sec + (double)now.tv_usec/1000000.0;
    fps = (double)frameCount/delta_t;

    printf("%5.2f frames/sec.\n", fps); 

    exit(0);
}

int
main(void)
{
    int ret;
    svImage *imageInfo;
    svContext context1, context2;

    context1 = svCurrentContext();
    svSelectInput(vnINPUT_INDYCAM);
    svSetFrameCount(2);
    svSetTransferMode(svTRANSFER_CONTINUOUS);
    svSetImagePacking(VL_PACKING_YVYU_422_8);

    context2 = svNewContext();
    svSelectOutput(gvOUTPUT_ANALOG);
    svSetFrameCount(2);
    svSetTransferMode(svTRANSFER_CONTINUOUS);
    svSetImagePacking(VL_PACKING_YVYU_422_8);

    imageInfo = svNewImage();

    signal(SIGINT, intHandler);
    gettimeofday(&startTime);

    while (1) {
	svSetContext(context1);
	if (ret = svGetFrame(imageInfo))
	    printf("svGetFrame returns %d\n", ret);

	svSetContext(context2);
	if (ret = svPutFrame(imageInfo))
	    printf("svPutFrame returns %d\n", ret);
	
	frameCount++;
    }

    return 0;
}

