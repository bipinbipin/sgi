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
 *  This is an example from the Inventor Mentor,
 *  chapter 5, example 6.
 *
 *  This example shows the effect of different order of
 *  operation of transforms.  The left object is first
 *  scaled, then rotated, and finally translated to the left.  
 *  The right object is first rotated, then scaled, and finally
 *  translated to the right.
 *------------------------------------------------------------*/

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoScale.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTranslation.h>

void
main(int, char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   SoSeparator *root = new SoSeparator;
   root->ref();

   // Create two separators, for left and right objects.
   SoSeparator *leftSep = new SoSeparator;
   SoSeparator *rightSep = new SoSeparator;
   root->addChild(leftSep);
   root->addChild(rightSep);

   // Create the transformation nodes
   SoTranslation *leftTranslation  = new SoTranslation;
   SoTranslation *rightTranslation = new SoTranslation;
   SoRotationXYZ *myRotation = new SoRotationXYZ;
   SoScale *myScale = new SoScale;

   // Fill in the values
   leftTranslation->translation.setValue(-1.0, 0.0, 0.0);
   rightTranslation->translation.setValue(1.0, 0.0, 0.0);
   myRotation->angle = M_PI/2;   // 90 degrees
   myRotation->axis = SoRotationXYZ::X;
   myScale->scaleFactor.setValue(2., 1., 3.);

   // Add transforms to the scene.
   leftSep->addChild(leftTranslation);   // left graph
   leftSep->addChild(myRotation);        // then rotated
   leftSep->addChild(myScale);           // first scaled

   rightSep->addChild(rightTranslation); // right graph
   rightSep->addChild(myScale);          // then scaled
   rightSep->addChild(myRotation);       // first rotated

   // Read an object from file. (as in example 4.2.Lights)
   SoInput myInput;
   if (!myInput.openFile("/usr/share/src/Inventor/examples/data/temple.iv")) 
      exit (1);
   SoSeparator *fileContents = SoDB::readAll(&myInput);
   if (fileContents == NULL) 
      exit (1);

   // Add an instance of the object under each separator.
   leftSep->addChild(fileContents);
   rightSep->addChild(fileContents);

   // Construct a renderArea and display the scene.
   SoXtExaminerViewer *myViewer = 
            new SoXtExaminerViewer(myWindow);
   myViewer->setSceneGraph(root);
   myViewer->setTitle("Transform Ordering");
   myViewer->viewAll();
   myViewer->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}
