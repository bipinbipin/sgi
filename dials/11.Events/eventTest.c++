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


#include <X11/Intrinsic.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSeparator.h>

#include "ButtonBoxEvent.h"
#include "DialEvent.h"
#include "DialNButton.h"

int dialVals[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// Map which button (1-32) into complexity (0-1).
void
buttonBoxCB(void *userData, SoEventCallback *cb)
{
   const ButtonBoxEvent *ev = 
      (const ButtonBoxEvent *) cb->getEvent();
	
   if (ev->getState() == SoButtonEvent::DOWN) {
      int which = ev->getButton();
      SoComplexity *complexity = (SoComplexity *) userData;
      complexity->value = (which - 1)/31.0;
   }
}

// Convert a dial value into the range [0.0,1.0]
float
getDialFloat(int d)
{
   float v;
   
   // Clamp the dial value to [-128,128]
   // so the conversion is possible.
   if (d <= -128)
      v = 0.0;
   else if (d >= 128)
      v = 1.0;
   else
      v = (d + 128)/256.0;
   
   return v;
}

// Convert a dial value into the range [-1.0,1.0]
float
getDialFloat2(int d)
{
   float v;
   
   // Clamp the dial value to [-128,128]
   // so the conversion is possible.
   if (d <= -128)
      v = -1.0;
   else if (d >= 128)
      v = 1.0;
   else
      v = d/128.0;
   
   return v;
}

// Use dials to change the material of the sphere
void
dialCB(void *userData, SoEventCallback *cb)
{
   const DialEvent *ev = 
      (const DialEvent *) cb->getEvent();
   
   SoDirectionalLight *light = (SoDirectionalLight *) userData;
   
   // Use dials to control a directional light:
   //   dial 0: red         dial 1: x component of direction
   //   dial 2: green       dial 3: y component of direction
   //   dial 4: blue        dial 5: z component of direction
   //   dial 6: intensity   dial 7: unused
   float x, y, z, r, g, b, f;
   switch (ev->getDial()) {
      case 0:
      case 2:
      case 4:
         light->color.getValue().getValue(r, g, b);
	 f = getDialFloat(ev->getValue());

	 if (ev->getDial() == 0)
	      r = f;
	 else if (ev->getDial() == 2)
	      g = f;
	 else b = f;
	 
	 light->color.setValue(r, g, b);
         break;

      case 6:
	 light->intensity.setValue(getDialFloat(ev->getValue()));
         break;
	 
      case 1:
      case 3:
      case 5:
         light->direction.getValue().getValue(x, y, z);
	 f = getDialFloat2(ev->getValue());
	 
	 if (ev->getDial() == 1)
	      x = f;
	 else if (ev->getDial() == 3)
	      y = f;
	 else z = f;
	 
	 light->direction.setValue(x, y, z);
         break;
	 
      default: 
	 break;
   }
}   

SoNode *
buildSceneGraph()
{
   SoSeparator *sep = new SoSeparator;
   SoEventCallback *cb = new SoEventCallback;
   SoDirectionalLight *light = new SoDirectionalLight;
   SoComplexity *complexity = new SoComplexity;
   SoSphere *sphere = new SoSphere;
   
   sep->addChild(cb);
   sep->addChild(light);
   sep->addChild(complexity);
   sep->addChild(sphere);
   
   // Set starting values. For direction, set the z value
   // close to 0 so turning the dial does not cause jumpiness.
   light->color.setValue(.5, .5, .5);
   light->intensity.setValue(.5);
   light->direction.setValue(0, 0, -.01);
   
   complexity->value.setValue(1.0);
   
   // Set up event callbacks
   cb->addEventCallback(
      ButtonBoxEvent::getClassTypeId(),
      buttonBoxCB, 
      complexity);
   cb->addEventCallback(
      DialEvent::getClassTypeId(),
      dialCB, 
      light);
   
   return sep;
}

void
main(int , char *argv[])
{
   Widget mainWindow = SoXt::init(argv[0]);
   
   if (! DialNButton::exists()) {
       fprintf(stderr, "Sorry, no dial and button box on this display!\n");
       return;
   }
   
   ButtonBoxEvent::initClass();
   DialEvent::initClass();
   
   SoXtExaminerViewer *vwr = new SoXtExaminerViewer(mainWindow);
   vwr->setSceneGraph(buildSceneGraph());
   vwr->setTitle("Dial And Button Device");
   vwr->setViewing(FALSE);   // come up in pick mode
   vwr->setHeadlight(FALSE); // we supply our own light

   DialNButton *db = new DialNButton;
   vwr->registerDevice(db);
   
   vwr->show();
   SoXt::show(mainWindow);
   SoXt::mainLoop();
}
