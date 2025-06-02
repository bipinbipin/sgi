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
 *  chapter 13, example 3.
 *
 *  Elapsed time engine.
 *  The output from an elapsed time engine is used to control
 *  the translation of the object.  The resulting effect is
 *  that the figure slides across the scene.
 *------------------------------------------------------------*/

#include <Inventor/SoDB.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/engines/SoCompose.h>
#include <Inventor/engines/SoElapsedTime.h>
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
   myCamera->position.setValue(-2.0, -2.0, 5.0);
   myCamera->heightAngle = M_PI/2.5; 
   myCamera->nearDistance = 2.0;
   myCamera->farDistance = 7.0;
   root->addChild(myCamera);
   root->addChild(new SoDirectionalLight);

   // Set up transformations
   SoTranslation *slideTranslation = new SoTranslation;
   root->addChild(slideTranslation);
   SoTransform *initialTransform = new SoTransform;
   initialTransform->translation.setValue(-5., 0., 0.);
   initialTransform->scaleFactor.setValue(10., 10., 10.);
   initialTransform->rotation.setValue(SbVec3f(1,0,0), M_PI/2.);
   root->addChild(initialTransform);

   // Read the figure object from a file and add to the scene
   SoInput myInput;
   if (!myInput.openFile("/usr/share/src/Inventor/examples/data/jumpyMan.iv")) 
      exit (1);
   SoSeparator *figureObject = SoDB::readAll(&myInput);
   if (figureObject == NULL) 
      exit (1);
   root->addChild(figureObject);

   // Make the X translation value change over time.
   SoElapsedTime *myCounter = new SoElapsedTime;
   SoComposeVec3f *slideDistance = new SoComposeVec3f;
   slideDistance->x.connectFrom(&myCounter->timeOut);
   slideTranslation->translation.connectFrom(
            &slideDistance->vector);

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   SbViewportRegion myRegion(myRenderArea->getSize()); 
   myRenderArea->setSceneGraph(root);
   myRenderArea->setTitle("Sliding Man");
   myRenderArea->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}
