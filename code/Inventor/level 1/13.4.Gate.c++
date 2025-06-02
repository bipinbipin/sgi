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
 *  chapter 13, example 5.
 *
 *  Gate engine.
 *  Mouse button presses enable and disable a gate engine.
 *  The gate engine controls an elapsed time engine that
 *  controls the motion of the duck.
 *------------------------------------------------------------*/

#include <Inventor/SoDB.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/engines/SoCompose.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoGate.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>

void myMousePressCB(void *, SoEventCallback *);

void
main(int , char **argv)
{
   // Print out usage message
   printf("Click the left mouse button to enable/disable the duck motion\n");

   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);  
   if (myWindow == NULL) exit(1);     

   SoSeparator *root = new SoSeparator;
   root->ref();

   // Add a camera and light
   SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
   myCamera->position.setValue(0., -4., 8.0);
   myCamera->heightAngle = M_PI/2.5; 
   myCamera->nearDistance = 1.0;
   myCamera->farDistance = 15.0;
   root->addChild(myCamera);
   root->addChild(new SoDirectionalLight);

   // Rotate scene slightly to get better view
   SoRotationXYZ *globalRotXYZ = new SoRotationXYZ;
   globalRotXYZ->axis = SoRotationXYZ::X;
   globalRotXYZ->angle = M_PI/9;
   root->addChild(globalRotXYZ);

   // Pond group
   SoSeparator *pond = new SoSeparator; 
   root->addChild(pond);
   SoMaterial *cylMaterial = new SoMaterial;
   cylMaterial->diffuseColor.setValue(0., 0.3, 0.8);
   pond->addChild(cylMaterial);
   SoTranslation *cylTranslation = new SoTranslation;
   cylTranslation->translation.setValue(0., -6.725, 0.);
   pond->addChild(cylTranslation);
   SoCylinder *myCylinder = new SoCylinder;
   myCylinder->radius.setValue(4.0);
   myCylinder->height.setValue(0.5);
   pond->addChild(myCylinder);

/////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE  (part 1)

   // Duck group
   SoSeparator *duck = new SoSeparator;
   root->addChild(duck);

   // Read the duck object from a file and add to the group
   SoInput myInput;
   if (!myInput.openFile("/usr/share/src/Inventor/examples/data/duck.iv")) 
      exit (1);
   SoSeparator *duckObject = SoDB::readAll(&myInput);
   if (duckObject == NULL) 
      exit (1);

   // Set up the duck transformations
   SoRotationXYZ *duckRotXYZ = new SoRotationXYZ;
   duck->addChild(duckRotXYZ);
   SoTransform *initialTransform = new SoTransform;
   initialTransform->translation.setValue(0., 0., 3.);
   initialTransform->scaleFactor.setValue(6., 6., 6.);
   duck->addChild(initialTransform);

   duck->addChild(duckObject);

   // Update the rotation value if the gate is enabled.
   SoGate *myGate = new SoGate(SoMFFloat::getClassTypeId());
   SoElapsedTime *myCounter = new SoElapsedTime;
   myGate->input->connectFrom(&myCounter->timeOut); 
   duckRotXYZ->axis = SoRotationXYZ::Y;  // rotate about Y axis
   duckRotXYZ->angle.connectFrom(myGate->output);

   // Add an event callback to catch mouse button presses.
   // Each button press will enable or disable the duck motion.
   SoEventCallback *myEventCB = new SoEventCallback;
   myEventCB->addEventCallback(
            SoMouseButtonEvent::getClassTypeId(),
            myMousePressCB, myGate);
   root->addChild(myEventCB);

// CODE FOR The Inventor Mentor ENDS HERE
/////////////////////////////////////////////////////////////

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   myRenderArea->setSceneGraph(root);
   myRenderArea->setTitle("Duck Pond");
   myRenderArea->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}

/////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE  (part 2)

// This routine is called for every mouse button event.
void
myMousePressCB(void *userData, SoEventCallback *eventCB)
{
   SoGate *gate = (SoGate *) userData;
   const SoEvent *event = eventCB->getEvent();

   // Check for mouse button being pressed
   if (SO_MOUSE_PRESS_EVENT(event, ANY)) {

      // Toggle the gate that controls the duck motion
      if (gate->enable.getValue()) 
         gate->enable.setValue(FALSE);
      else 
         gate->enable.setValue(TRUE);

      eventCB->setHandled();
   } 
}

// CODE FOR The Inventor Mentor ENDS HERE
/////////////////////////////////////////////////////////////
