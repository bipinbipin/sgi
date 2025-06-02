/* 
 *  This application playsback a DV video file on the IEEE 1394 bus in the
 *  loop playback mode (100 times).
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <bstring.h>
#include <avc.h>

main(argc,argv)
int argc;
char **argv;
{
    AVCport port;
    AVCconfig config;
    int result, filefd, i, numFrames, channel, portCount;
    AVCframe frame;
    char preloadBuf[AVC_FRAMESIZE_MAX];

    int dsf, arg, framesize;	/* for debugging */

    if (argc < 2) {
	printf("Usage %s <DIF file> [channel]\n",argv[0]);
	exit(1);
    }

    filefd = open(argv[1], O_RDONLY);
    if(filefd < 0) {
        fprintf(stderr, "Can't open %s\n", argv[1]);
        exit(1);
    }

    if (argc == 3)
	channel = atoi(argv[2]);
    else
	channel = 63;

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"sendframe: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    numFrames = 12;
    avcSetBufferSize(config, numFrames); 
    avcSetChannel(config, channel);

    result = read(filefd, preloadBuf, AVC_FRAMESIZE_MAX);

    if(result == -1) {
	perror("sendframe: input file read failed");
	exit(1);
    }
    close(filefd);

    for (portCount = 0; portCount < 100; portCount++) {
	
	port = avcOpenPort("w", config);
	if (port == NULL) {
	    fprintf(stderr,"sendframe: avcOpenPort() returned null port.\n");
	    exit(1);
	}

	avcAllocateBandwidth(port, 500);
	avcAllocateChannel(port, channel);

	/* Note that a "real" applications would register interest in bus resets
	   in order to re-allocate the channels and bandwidth.  See setbusreset.c.
	*/

	frame = avcNewFrame();
	if (avcEncodeFrameFromBuffer(port, frame, preloadBuf) < 0)
	    exit(1);

	framesize = avcGetFrameSize(frame);

	/* re-open DIF file */

	filefd = open(argv[1], O_RDONLY);
	if(filefd < 0) {
	    fprintf(stderr, "Can't open %s\n", argv[1]);
	    exit(1);
	}

	/* i=0; */
    
	while(1) {
	    if (avcAcquireFrame(port, frame) < 0) /* ,i */
		break;

	    result = read(filefd, avcGetFrameData(frame), framesize); 
	    if(result == -1) {
		perror("sendframe: mapped read failed");
		break;
	    }
	    

	    if(result != framesize){
		/* 	    lseek(filefd, 0, SEEK_SET); */
		/* 	    result = read(filefd, avcGetFrameData(frame), framesize);  */
		break;
	    }

	    result = avcSendFrame(port, frame);
	    if(result == -1) {
		perror("sendframe: AVC1394_PUT_FRAME ioctl failed");
		break;
	    }
	
	    /* i=(i+1)%numFrames; */
	}

	close(filefd);

	avcFreeBandwidth(port, 500);
	avcFreeChannel(port, channel);
	if (avcClosePort(port) < 0) {
	    perror("sendframe: avcClosePort() failed");
	    exit(-1);
	}

    }
    exit(0);
}
