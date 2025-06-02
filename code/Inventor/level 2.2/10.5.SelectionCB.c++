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

/*-------------------------------------------------------------
 *  This is an example from The Inventor Mentor,
 *  chapter 10, example 5.
 *
 *  The scene graph has a sphere and a text 3D object. 
 *  A selection node is placed at the top of the scene graph. 
 *  When an object is selected, a selection callback is called
 *  to change the material color of that object.
 *------------------------------------------------------------*/

#include <X11/Intrinsic.h>
#include <Inventor/Sb.h>
#include <Inventor/SoInput.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPickStyle.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTransform.h>

// global data
SoMaterial *textMaterial, *sphereMaterial;
static float reddish[] = {1.0, 0.2, 0.2};  // Color when selected
static float white[] = {0.8, 0.8, 0.8};    // Color when not selected

// This routine is called when an object gets selected. 
// We determine which object was selected, and change 
// that objects material color.
void
mySelectionCB(void *, SoPath *selectionPath)
{
   if (selectionPath->getTail()->
            isOfType(SoText3::getClassTypeId())) { 
      textMaterial->diffuseColor.setValue(reddish);
   } else if (selectionPath->getTail()->
            isOfType(SoSphere::getClassTypeId())) {
      sphereMaterial->diffuseColor.setValue(reddish);
   }
}

// This routine is called whenever an object gets deselected. 
// We determine which object was deselected, and reset 
// that objects material color.
void
myDeselectionCB(void *, SoPath *deselectionPath)
{
   if (deselectionPath->getTail()->
            isOfType(SoText3::getClassTypeId())) {
      textMaterial->diffuseColor.setValue(white);
   } else if (deselectionPath->getTail()->
            isOfType(SoSphere::getClassTypeId())) {
      sphereMaterial->diffuseColor.setValue(white);
   }
}


void
main(int argc, char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   // Create and set up the selection node
   SoSelection *selectionRoot = new SoSelection;
   selectionRoot->ref();
   selectionRoot->policy = SoSelection::SINGLE;
   selectionRoot-> addSelectionCallback(mySelectionCB);
   selectionRoot-> addDeselectionCallback(myDeselectionCB);

   // Create the scene graph
   SoSeparator *root = new SoSeparator;
   selectionRoot->addChild(root);

   SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
   root->addChild(myCamera);
   root->addChild(new SoDirectionalLight);

   // Add a sphere node
   SoSeparator *sphereRoot = new SoSeparator;
   SoTransform *sphereTransform = new SoTransform;
   sphereTransform->translation.setValue(17., 17., 0.);
   sphereTransform->scaleFactor.setValue(8., 8., 8.);
   sphereRoot->addChild(sphereTransform);

   sphereMaterial = new SoMaterial;
   sphereMaterial->diffuseColor.setValue(.8, .8, .8);
   sphereRoot->addChild(sphereMaterial);
   sphereRoot->addChild(new SoSphere);
   root->addChild(sphereRoot);

   // Add a text node
   SoSeparator *textRoot = new SoSeparator;
   SoTransform *textTransform = new SoTransform;
   textTransform->translation.setValue(0., -1., 0.);
   textRoot->addChild(textTransform);

   textMaterial = new SoMaterial;
   textMaterial->diffuseColor.setValue(.8, .8, .8);
   textRoot->addChild(textMaterial);
   SoPickStyle *textPickStyle = new SoPickStyle;
   textPickStyle->style.setValue(SoPickStyle::BOUNDING_BOX);
   textRoot->addChild(textPickStyle);
   SoText3 *myText = new SoText3;
   myText->string = "rhubarb";
   textRoot->addChild(myText);
   root->addChild(textRoot);

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   myRenderArea->setSceneGraph(selectionRoot);
   myRenderArea->setTitle("My Selection Callback");
   myRenderArea->show();

   // Make the camera see the whole scene
   const SbViewportRegion myViewport = 
            myRenderArea->getViewportRegion();
   myCamera->viewAll(root, myViewport, 2.0);

   SoXt::show(myWindow);
   SoXt::mainLoop();
}

