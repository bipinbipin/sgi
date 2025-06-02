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
 *  chapter 13, example 6.
 *
 *  Boolean engine.  Derived from example 13.5.
 *  The smaller duck stays still while the bigger duck moves,
 *  and starts moving as soon as the bigger duck stops.
 *------------------------------------------------------------*/

#include <Inventor/SoDB.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/engines/SoBoolOperation.h>
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
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoTranslation.h>

void myMousePressCB(void *, SoEventCallback *);

void
main(int , char **argv)
{
   // Print out usage message
   printf("Only one duck can move at a time.\n");
   printf("Click the left mouse button to toggle between the two ducks.\n");

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
   SoTranslation *pondTranslation = new SoTranslation;
   pondTranslation->translation.setValue(0., -6.725, 0.);
   pond->addChild(pondTranslation);
   // water
   SoMaterial *waterMaterial = new SoMaterial;
   waterMaterial->diffuseColor.setValue(0., 0.3, 0.8);
   pond->addChild(waterMaterial);
   SoCylinder *waterCylinder = new SoCylinder;
   waterCylinder->radius.setValue(4.0);
   waterCylinder->height.setValue(0.5);
   pond->addChild(waterCylinder);
   // rock
   SoMaterial *rockMaterial = new SoMaterial;
   rockMaterial->diffuseColor.setValue(0.8, 0.23, 0.03);
   pond->addChild(rockMaterial);
   SoSphere *rockSphere = new SoSphere;
   rockSphere->radius.setValue(0.9);
   pond->addChild(rockSphere);

   // Read the duck object from a file and add to the group
   SoInput myInput;
   if (!myInput.openFile("/usr/share/src/Inventor/examples/data/duck.iv")) 
      exit (1);
   SoSeparator *duckObject = SoDB::readAll(&myInput);
   if (duckObject == NULL) 
      exit (1);

/////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE  

   // Bigger duck group
   SoSeparator *bigDuck = new SoSeparator;
   root->addChild(bigDuck);
   SoRotationXYZ *bigDuckRotXYZ = new SoRotationXYZ;
   bigDuck->addChild(bigDuckRotXYZ);
   SoTransform *bigInitialTransform = new SoTransform;
   bigInitialTransform->translation.setValue(0., 0., 3.5);
   bigInitialTransform->scaleFactor.setValue(6., 6., 6.);
   bigDuck->addChild(bigInitialTransform);
   bigDuck->addChild(duckObject);

   // Smaller duck group
   SoSeparator *smallDuck = new SoSeparator;
   root->addChild(smallDuck);
   SoRotationXYZ *smallDuckRotXYZ = new SoRotationXYZ;
   smallDuck->addChild(smallDuckRotXYZ);
   SoTransform *smallInitialTransform = new SoTransform;
   smallInitialTransform->translation.setValue(0., -2.24, 1.5);
   smallInitialTransform->scaleFactor.setValue(4., 4., 4.);
   smallDuck->addChild(smallInitialTransform);
   smallDuck->addChild(duckObject);

   // Use a gate engine to start/stop the rotation of 
   // the bigger duck.
   SoGate *bigDuckGate = new SoGate(SoMFFloat::getClassTypeId());
   SoElapsedTime *bigDuckTime = new SoElapsedTime;
   bigDuckGate->input->connectFrom(&bigDuckTime->timeOut); 
   bigDuckRotXYZ->axis = SoRotationXYZ::Y;  // Y axis
   bigDuckRotXYZ->angle.connectFrom(bigDuckGate->output);

   // Each mouse button press will enable/disable the gate 
   // controlling the bigger duck.
   SoEventCallback *myEventCB = new SoEventCallback;
   myEventCB->addEventCallback(
            SoMouseButtonEvent::getClassTypeId(),
            myMousePressCB, bigDuckGate);
   root->addChild(myEventCB);

   // Use a Boolean engine to make the rotation of the smaller
   // duck depend on the bigger duck.  The smaller duck moves
   // only when the bigger duck is still.
   SoBoolOperation *myBoolean = new SoBoolOperation;
   myBoolean->a.connectFrom(&bigDuckGate->enable);
   myBoolean->operation = SoBoolOperation::NOT_A;

   SoGate *smallDuckGate = new SoGate(SoMFFloat::getClassTypeId());
   SoElapsedTime *smallDuckTime = new SoElapsedTime;
   smallDuckGate->input->connectFrom(&smallDuckTime->timeOut); 
   smallDuckGate->enable.connectFrom(&myBoolean->output); 
   smallDuckRotXYZ->axis = SoRotationXYZ::Y;  // Y axis
   smallDuckRotXYZ->angle.connectFrom(smallDuckGate->output);

// CODE FOR The Inventor Mentor ENDS HERE
/////////////////////////////////////////////////////////////

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   myRenderArea->setSceneGraph(root);
   myRenderArea->setTitle("Duck and Duckling");
   myRenderArea->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}

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

