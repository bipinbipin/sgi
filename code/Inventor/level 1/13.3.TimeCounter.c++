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
 *  chapter 13, example 4.
 *
 *  Time counter engine.
 *  The output from an time counter engine is used to control
 *  horizontal and vertical motion of a figure object.
 *  The resulting effect is that the figure jumps across
 *  the screen.
 *------------------------------------------------------------*/

#include <Inventor/SoDB.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/engines/SoCompose.h>
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
   myCamera->position.setValue(-8.0, -7.0, 20.0);
   myCamera->heightAngle = M_PI/2.5; 
   myCamera->nearDistance = 15.0;
   myCamera->farDistance = 25.0;
   root->addChild(myCamera);
   root->addChild(new SoDirectionalLight);

//////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE

   // Set up transformations
   SoTranslation *jumpTranslation = new SoTranslation;
   root->addChild(jumpTranslation);
   SoTransform *initialTransform = new SoTransform;
   initialTransform->translation.setValue(-20., 0., 0.);
   initialTransform->scaleFactor.setValue(40., 40., 40.);
   initialTransform->rotation.setValue(SbVec3f(1,0,0), M_PI/2.);
   root->addChild(initialTransform);

   // Read the man object from a file and add to the scene
   SoInput myInput;
   if (!myInput.openFile("/usr/share/src/Inventor/examples/data/jumpyMan.iv")) 
      exit (1);
   SoSeparator *manObject = SoDB::readAll(&myInput);
   if (manObject == NULL) 
      exit (1);
   root->addChild(manObject);

   // Create two counters, and connect to X and Y translations.
   // The Y counter is small and high frequency.
   // The X counter is large and low frequency.
   // This results in small jumps across the screen, 
   // left to right, again and again and again and ....
   SoTimeCounter *jumpHeightCounter = new SoTimeCounter;
   SoTimeCounter *jumpWidthCounter = new SoTimeCounter;
   SoComposeVec3f *jump = new SoComposeVec3f;

   jumpHeightCounter->max = 4;
   jumpHeightCounter->frequency = 1.5;
   jumpWidthCounter->max = 40;
   jumpWidthCounter->frequency = 0.15;

   jump->x.connectFrom(&jumpWidthCounter->output);
   jump->y.connectFrom(&jumpHeightCounter->output);
   jumpTranslation->translation.connectFrom(&jump->vector);

// CODE FOR The Inventor Mentor ENDS HERE
//////////////////////////////////////////////////////////////

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   SbViewportRegion myRegion(myRenderArea->getSize()); 
   myRenderArea->setSceneGraph(root);
   myRenderArea->setTitle("Jumping Man");
   myRenderArea->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}
