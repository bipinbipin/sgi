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
 *  chapter 13, example 8.
 *
 *  Rotor node example.  
 *  Read in the tower and vanes of a windmill from a file.
 *  Use a rotor node to rotate the vanes.
 *------------------------------------------------------------*/

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoRotor.h>

SoSeparator *
readFile(const char *filename)
{
   // Open the input file
   SoInput mySceneInput;
   if (!mySceneInput.openFile(filename)) {
      fprintf(stderr, "Cannot open file %s\n", filename);
      return NULL;
   }

   // Read the whole file into the database
   SoSeparator *myGraph = SoDB::readAll(&mySceneInput);
   if (myGraph == NULL) {
      fprintf(stderr, "Problem reading file\n");
      return NULL;
   } 

   mySceneInput.closeFile();
   return myGraph;
}

void
main(int argc, char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);

   SoSeparator *root = new SoSeparator;
   root->ref();

   // Read in the data for the windmill tower
   SoSeparator *windmillTower = 
            readFile("/usr/share/src/Inventor/examples/data/windmillTower.iv");
   root->addChild(windmillTower);

   // Add a rotor node to spin the vanes
   SoRotor *myRotor = new SoRotor;
   myRotor->rotation.setValue(SbVec3f(0, 0, 1), 0); // z axis
   myRotor->speed = 0.2;
   root->addChild(myRotor);

   // Read in the data for the windmill vanes
   SoSeparator *windmillVanes = 
            readFile("/usr/share/src/Inventor/examples/data/windmillVanes.iv");
   root->addChild(windmillVanes);

   // Create a viewer
   SoXtExaminerViewer *myViewer = 
            new SoXtExaminerViewer(myWindow);

   // attach and show viewer
   myViewer->setSceneGraph(root);
   myViewer->setTitle("Windmill");
   myViewer->show();
    
   // Loop forever
   SoXt::show(myWindow);
   SoXt::mainLoop();
}

