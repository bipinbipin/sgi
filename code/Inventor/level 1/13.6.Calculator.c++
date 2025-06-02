/*
 * Copyright (c) 1991-95 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the name of Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SILICON GRAPHICS BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER OR NOT ADVISED OF THE
 * POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF LIABILITY, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*--------------------------------------------------------------
 *  This is an example from the Inventor Mentor
 *  chapter 13, example 7.
 *
 *  A calculator engine computes a closed, planar curve.
 *  The output from the engine is connected to the translation
 *  applied to a flower object, which consequently moves
 *  along the path of the curve.
 *------------------------------------------------------------*/

#include <Inventor/SoDB.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoTimeCounter.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>

void
main(int , char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);  
   if (myWindow == NULL) exit(1);     

   SoSeparator *root = new SoSeparator;
   root->ref();

   // Add a camera and light
   SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
   myCamera->position.setValue(-0.5, -3.0, 19.0);
   myCamera->nearDistance = 10.0;
   myCamera->farDistance = 26.0;
   root->addChild(myCamera);
   root->addChild(new SoDirectionalLight);

   // Rotate scene slightly to get better view
   SoRotationXYZ *globalRotXYZ = new SoRotationXYZ;
   globalRotXYZ->axis = SoRotationXYZ::X;
   globalRotXYZ->angle = M_PI/7;
   root->addChild(globalRotXYZ);

   // Read the background path from a file and add to the group
   SoInput myInput;
   if (!myInput.openFile("/usr/share/src/Inventor/examples/data/flowerPath.iv")) 
      exit (1);
   SoSeparator *flowerPath = SoDB::readAll(&myInput);
   if (flowerPath == NULL) exit (1);
   root->addChild(flowerPath);

/////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE  

   // Flower group
   SoSeparator *flowerGroup = new SoSeparator;
   root->addChild(flowerGroup);

   // Read the flower object from a file and add to the group
   if (!myInput.openFile("/usr/share/src/Inventor/examples/data/flower.iv")) 
      exit (1);
   SoSeparator *flower= SoDB::readAll(&myInput);
   if (flower == NULL) exit (1);

   // Set up the flower transformations
   SoTranslation *danceTranslation = new SoTranslation;
   SoTransform *initialTransform = new SoTransform;
   flowerGroup->addChild(danceTranslation);
   initialTransform->scaleFactor.setValue(10., 10., 10.);
   initialTransform->translation.setValue(0., 0., 5.);
   flowerGroup->addChild(initialTransform);
   flowerGroup->addChild(flower);

   // Set up an engine to calculate the motion path:
   // r = 5*cos(5*theta); x = r*cos(theta); z = r*sin(theta)
   // Theta is incremented using a time counter engine,
   // and converted to radians using an expression in
   // the calculator engine.
   SoCalculator *calcXZ = new SoCalculator; 
   SoTimeCounter *thetaCounter = new SoTimeCounter;

   thetaCounter->max = 360;
   thetaCounter->step = 4;
   thetaCounter->frequency = 0.075;

   calcXZ->a.connectFrom(&thetaCounter->output);    
   calcXZ->expression.set1Value(0, "ta=a*M_PI/180"); // theta
   calcXZ->expression.set1Value(1, "tb=5*cos(5*ta)"); // r
   calcXZ->expression.set1Value(2, "td=tb*cos(ta)"); // x 
   calcXZ->expression.set1Value(3, "te=tb*sin(ta)"); // z 
   calcXZ->expression.set1Value(4, "oA=vec3f(td,0,te)"); 
   danceTranslation->translation.connectFrom(&calcXZ->oA);

// CODE FOR The Inventor Mentor ENDS HERE
/////////////////////////////////////////////////////////////

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   myRenderArea->setSceneGraph(root);
   myRenderArea->setTitle("Flower Dance");
   myRenderArea->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}

