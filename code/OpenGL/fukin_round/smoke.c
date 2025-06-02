/*----------------------------------------------------------------------------/
  vlib demo

  (c) 2001 Andrew S. Winter, csandrew@swansea.ac.uk
  University of Wales Swansea, UK.

  A simple smoke texture.
/----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "samples.h"

/*----------------------------------------------------------------------------/
/----------------------------------------------------------------------------*/
 
vlfloat cylMap[] = {
    0.001, 0.0, 0.0,
    1.000, 1.0, 0.7
};

/*----------------------------------------------------------------------------/
/----------------------------------------------------------------------------*/

void mySmoke(vlfloat *p, void *misc) {

  static vlint seed;

  if (vlField(VL_F) < 0.0000001)
    return;

  vlSet(VL_O, (cos(p[0] + vluTurbulence(p, .35, 5, seed) * 1.2) * vlField(VL_F)) 
     * (p[1] - 1.));
}

/*----------------------------------------------------------------------------/
/----------------------------------------------------------------------------*/

int main(int argc, char *argv[]) {

  vlInit();
#include "parallel.inc"
  vlScene();

      vluCamera(0., 100., -250., 0., 0., 0.);

      vlImage("smoke.ppm", ASPECT, WIDTH, HEIGHT);

      vlSuperSample(SUPER_SAMPLE);

      vlBackground(0.4, 0.4, 1.);
      vlAmbient(1., 1., 1., 1.);

      vlPointLight(-100., 100., -100., 
          VL_NOATTENUATION, 1., 1., 1., 1.);

      vlStepSize(1., 15.);

      vlLoad(1, DATA_FILE("grass_r.vlb"));
      vlLoad(2, DATA_FILE("grass_g.vlb"));
      vlLoad(3, DATA_FILE("grass_b.vlb"));

      vlToggle(VL_CASTSHADOWS, VL_TRUE);

      vlMapping(1, VL_LOOKUP, 2, 2, cylMap);

      /* smoke */

      vlObject();

          vlTranslate(-.5, -.5, -.5);
          vlScale(140., 200., 80.);
          vlTranslate(0., 75., 0.);

          vlFunction(VL_F, &VLU_CYLINDER, NULL);

          vlFunction(VL_O, &mySmoke, NULL);

          vlSet(VL_F, 0.);

          vlSet(VL_COLORS, 1.);

          vlSet(VL_KA, .8); 
          vlSet(VL_KD, .3);
      vlEnd(); 

      /* chimney */

      vlObject();

          vlTranslate(-.5, -.5, -.5);
          vlScale(140., 50., 80.);
          vlTranslate(0., -50., 0.);

          vlFunction(VL_F, &VLU_CYLINDER, NULL);
          vlMap(VL_O, VL_F, 1, 1);

          vlSet(VL_COLORS, .2);

          vlSet(VL_KA, .3); 
          vlSet(VL_KD, .8);

          vlMap(VL_KS, VL_F, 1, 2);
          vlSet(VL_N, VLSPECULAR(100));
      vlEnd(); 

#ifdef HIGH_QUALITY
      vlToggle(VL_RECEIVESHADOWS, VL_TRUE);
#endif

      /* ground */

      vlObject();

          vlTranslate(-.5, -.5, -.5);
          vlScale(2000., 2000., 300.);
          vlRotate(90., 0., 0.);
          vlTranslate(0., -225., 0.);

          vlDataset(VL_R, 1);
          vlDataset(VL_G, 2);
          vlDataset(VL_B, 3);

          vlSet(VL_O | VL_F, 1.);

          vlSet(VL_KA | VL_KD, .5); 
          vlSet(VL_KS, .4);
          vlSet(VL_N, VLSPECULAR(10));
      vlEnd(); 
  vlEnd();
  vlEnd();

  return 0;
}
