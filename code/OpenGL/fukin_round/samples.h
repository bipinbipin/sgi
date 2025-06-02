/* to achieve higher quality rendering, you should change this line */
#undef HIGH_QUALITY
/* to read
        #define HIGH_QUALITY
*/

#include <vlib.h>
#include <vlaux.h>

#ifdef HIGH_QUALITY
  #define WIDTH  800
  #define HEIGHT  600
  #define SUPER_SAMPLE 3
#else
  #define WIDTH 160
  #define HEIGHT 120
  #define SUPER_SAMPLE 1
#endif

#define ASPECT ((vlfloat)WIDTH / (vlfloat)HEIGHT)

char *DATA_FILE(char *name);

void FillCamera(VLCAMERA *camera);

