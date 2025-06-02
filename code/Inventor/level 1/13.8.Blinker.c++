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
 *  chapter 13, example 9.
 *
 *  Blinker node.
 *  Use a blinker node to flash a neon ad sign on and off
 *------------------------------------------------------------*/

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/nodes/SoBlinker.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoText3.h>

void
main(int , char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);  
   if (myWindow == NULL) exit(1);     

   // Set up camera and light
   SoSeparator *root = new SoSeparator;
   root->ref();
   SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
   root->addChild(myCamera);
   root->addChild(new SoDirectionalLight);

   // Read in the parts of the sign from a file
   SoInput myInput;
   if (!myInput.openFile("/usr/share/src/Inventor/examples/data/eatAtJosies.iv")) 
      exit (1);
   SoSeparator *fileContents = SoDB::readAll(&myInput);
   if (fileContents == NULL) 
      exit (1);

   SoSeparator *eatAt;
   eatAt = (SoSeparator *)SoNode::getByName("EatAt");
   SoSeparator *josie;
   josie = (SoSeparator *)SoNode::getByName("Josies");
   SoSeparator *frame;
   frame = (SoSeparator *)SoNode::getByName("Frame");

//////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE

   // Add the non-blinking part of the sign to the root
   root->addChild(eatAt);
   
   // Add the fast-blinking part to a blinker node
   SoBlinker *fastBlinker = new SoBlinker;
   root->addChild(fastBlinker);
   fastBlinker->speed = 2;  // blinks 2 times a second
   fastBlinker->addChild(josie);

   // Add the slow-blinking part to another blinker node
   SoBlinker *slowBlinker = new SoBlinker;
   root->addChild(slowBlinker);
   slowBlinker->speed = 0.5;  // 2 secs per cycle; 1 on, 1 off
   slowBlinker->addChild(frame);

// CODE FOR The Inventor Mentor ENDS HERE
//////////////////////////////////////////////////////////////

   // Set up and display render area 
   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   SbViewportRegion myRegion(myRenderArea->getSize()); 
   myCamera->viewAll(root, myRegion);

   myRenderArea->setSceneGraph(root);
   myRenderArea->setTitle("Neon");
   myRenderArea->show();
   SoXt::show(myWindow);
   SoXt::mainLoop();
}

  
