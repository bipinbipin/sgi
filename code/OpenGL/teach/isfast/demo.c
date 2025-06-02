/* isfast demo - X11 version */


#include <stdio.h>
#include "isfast.h"


static const char* cFast = "fast";
static const char* cSlow = "slow";


void
main(int argc, char** argv) {
	if (!IsFastXOpenDisplay(NULL)) {
		fprintf(stderr, "can't open display\n");
		exit(1);
		}

	printf("Immediate mode is %s\n", ImmediateModeIsFast()? cFast: cSlow);
	printf("Depth-buffering is %s\n", DepthBufferingIsFast()?cFast:cSlow);
	printf("Stencilling is %s\n", StencillingIsFast()? cFast: cSlow);
	printf("Texture mapping is %s\n", TextureMappingIsFast()?cFast:cSlow);
	printf("Line antialiasing is %s\n", LineAAIsFast()? cFast: cSlow);

	IsFastXCloseDisplay();
	}
