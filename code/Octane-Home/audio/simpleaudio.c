#include <dmedia/audio.h>  
#include <math.h>

#define FRAMES 10000
main()
{
	  ALport p;
          short buf[10000];   /* 5000 stereo frames */
	  int n;

          /* open a port with the default configuration */
          p = alOpenPort("alWriteFrames example","w",0);

          if (!p) {
              printf("port open failed:%s\n", alGetErrorString(oserror()));
              exit(-1);
          }

	  for (n = 0; n < FRAMES; n++)
		buf[n] = (short) (32767.0 * sin(3.1415926585 * 8.0 * (n / (double) FRAMES)));
         
          alWriteFrames(p, buf, 5000);       /* write 5000 stereo frames */
}

