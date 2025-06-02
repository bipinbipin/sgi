/* 
 *  This application receives DV frames on channel 63 and saves them to 'dv.dif' file.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <bstring.h>
#include <avc.h>


/* #define PRINT_TIMECODE */

/* DIF Components */
typedef struct {
  unsigned char bytes[80];
} headerblock;

typedef struct {
  unsigned char bytes[80];
} subcodeblock;

typedef struct {
  unsigned char bytes[80];
} vauxblock;

typedef struct {
  unsigned char bytes[80];
} audioblock;

typedef struct {
  unsigned char bytes[80];
} macroblock;

typedef struct {
  macroblock mb[5];
} videosegment;

typedef struct {
  audioblock audio;
  videosegment vidseg[3];
} audiovideorow;

typedef struct {
  headerblock   header;
  subcodeblock subcode[2];
  vauxblock vaux[3];
  audiovideorow row[9];
} DIFsequence;



typedef struct
{
  unsigned char bf: 1;
  unsigned char d1: 1;
  unsigned char tof:2;
  unsigned char uof:4;
  unsigned char d2:1;
  unsigned char tos:3;
  unsigned char uos:4;
  unsigned char d3:1;
  unsigned char tom:3;
  unsigned char uom:4;
  unsigned char d4:2;
  unsigned char toh:2;
  unsigned char uoh:4;
} DVtimecode_t;


void BuildTimecodeString(char* strBuf, DVtimecode_t* timecode) {

    sprintf(strBuf, "%02d:%02d:%02d:%02d\n",
	   timecode->toh*10 + timecode->uoh,
	   timecode->tom*10 + timecode->uom,
	   timecode->tos*10 + timecode->uos,
	   timecode->tof*10 + timecode->uof);
}

long long TimecodeToFrames(DVtimecode_t* tc)
{
    long long numFrames;

    numFrames = tc->uof + tc->tof*10 
                + tc->uos*30 + tc->tos*10*30
                + tc->uom*60*30 + tc->tom*10*60*30
                + tc->uoh*60*60*30 + tc->toh*10*60*60*30;

    return numFrames;
}

void DecodeTimecode(DIFsequence* DIF, DVtimecode_t* tc)
{
    int i, d, s;
    char *p;
    unsigned char id0[3],id1[3],pc0[8];
    int sc;

    for (sc=0;sc<2;sc++) {
	for (i=0;i<6;i++){

	    pc0[i]=DIF->subcode[sc].bytes[i*8+3+3];
	    if (pc0[i] != 0xff){
		switch (pc0[i]) {
		case 0x13 :
		    {
			tc = (DVtimecode_t*) &DIF->subcode[sc].bytes[i*8+3+3+1];
			return;
		    }
		}
	    }
	}
    }
    
}


main(argc,argv)
int argc;
char **argv;
{
    AVCport port;
    AVCconfig config;
    int bufSize, outfd, i, numFrames;
    int result;
    fd_set rdfds;
    AVCframe frame;
    struct timeval timeout;
    DVtimecode_t tc;
    long long frameCount, oldFrameCount ;
    char buf[100];

    if (argc == 1)
	numFrames = 60;
    else
	numFrames = atoi(argv[1]);

    config = avcNewConfig();
    if (config == NULL) {
	fprintf(stderr,"recvframe: avcNewConfig() returned null config.\n");
	exit(-1);
    }

    avcSetBufferSize(config, 12); /* 1 second's worth */
    avcSetChannel(config, 63);

    port = avcOpenPort("r", config);
    if (port == NULL) {
	fprintf(stderr,"recvframe: avcOpenPort() returned null port.\n");
	exit(-1);
    }

    outfd = open("dv.dif", O_RDWR|O_CREAT|O_TRUNC, 0777);
    frame = avcNewFrame();

    FD_ZERO(&rdfds);
    for (i = 0; i < numFrames; i++) {
	FD_SET(avcGetFD(port), &rdfds);
	timeout.tv_sec=10;
	timeout.tv_usec=0;
	result = select(avcGetFD(port)+1, &rdfds, 0, 0, &timeout); 
	if(result == 0) {
	    printf("No 1394 frames arrived in the last %d seconds\n", timeout.tv_sec);
	    break;
	}
	result = avcReceiveFrame(port, frame);
	if (avcIsFrameValid(frame)) {
#ifdef PRINT_TIMECODE	    
	    DecodeTimecode((DIFsequence*)avcGetFrameData(frame), &tc);
	    BuildTimecodeString(buf, &tc);
	    fprintf(stderr,"%s\n", buf);
	    frameCount = TimecodeToFrames(&tc);
	    if (frameCount != oldFrameCount+1)
		fprintf(stderr,"Warning: Dropped incoming frames between %lld and %lld\n",
			frameCount, oldFrameCount);
	    oldFrameCount = frameCount;
#endif
	    write(outfd, avcGetFrameData(frame), avcGetFrameSize(frame));
	}
	else
	    printf("Invalid frame.\n");

	result = avcReleaseFrame(port);
    }

    close(outfd);

    if (avcClosePort(port) < 0) {
	perror("recvframe: avcClosePort() failed");
	exit(-1);
    }

    exit(0);
}
