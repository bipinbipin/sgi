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
 *  chapter 10, example 7.
 *
 *  This example demonstrates the use of the pick filter
 *  callback to pick through manipulators.
 *
 *  The scene graph has several objects. Clicking the left
 *  mouse on an object selects it and adds a manipulator to
 *  it. Clicking again deselects it and removes the manipulator.
 *  In this case, the pick filter is needed to deselect the
 *  object rather than select the manipulator.
 *------------------------------------------------------------*/

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/manips/SoHandleBoxManip.h>
#include <Inventor/nodes/SoBaseColor.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTransform.h>

// Returns path to xform left of the input path tail.
// Inserts the xform if none found. In this example,
// assume that the xform is always the node preceeding
// the selected shape.
SoPath *
findXform(SoPath *p)
{
   SoPath *returnPath;

   // Copy the input path up to tail's parent.
   returnPath = p->copy(0, p->getLength() - 1);

   // Get the parent of the selected shape
   SoGroup *g = (SoGroup *) p->getNodeFromTail(1);
   int tailNodeIndex = p->getIndexFromTail(0);
   
   // Check if there is already a transform node
   if (tailNodeIndex > 0) {
      SoNode *n = g->getChild(tailNodeIndex - 1);
      if (n->isOfType(SoTransform::getClassTypeId())) {
	 // Append to returnPath and return it.
         returnPath->append(n);
         return returnPath;
      }
   }
   
   // Otherwise, add a transform node.
   SoTransform *xf = new SoTransform;
   g->insertChild(xf, tailNodeIndex); // right before the tail
   // Append to returnPath and return it.
   returnPath->append(xf);
   return returnPath;
}

// Returns the manip affecting this path. In this example,
// the manip is always preceeding the selected shape.
SoPath *
findManip(SoPath *p)
{
   SoPath *returnPath;

   // Copy the input path up to tail's parent.
   returnPath = p->copy(0, p->getLength() - 1);

   // Get the index of the last node in the path.
   int tailNodeIndex = p->getIndexFromTail(0);
   
   // Append the left sibling of the tail to the returnPath
   returnPath->append(tailNodeIndex - 1);
   return returnPath;
}

// Add a manipulator to the transform affecting this path
// The first parameter, userData, is not used.
void
selCB(void *, SoPath *path)
{
   if (path->getLength() < 2) return;
    
   // Find the transform affecting this object
   SoPath *xfPath = findXform(path);
   xfPath->ref();
    
   // Replace the transform with a manipulator
   SoHandleBoxManip *manip = new SoHandleBoxManip;
   manip->ref();
   manip->replaceNode(xfPath);

   // Unref the xfPath
   xfPath->unref();
}

// Remove the manipulator affecting this path.
// The first parameter, userData, is not used.
void
deselCB(void *, SoPath *path)
{
   if (path->getLength() < 2) return;
    
   // Find the manipulator affecting this object
   SoPath *manipPath = findManip(path);
   manipPath->ref();
    
   // Replace the manipulator with a transform 
   SoTransformManip *manip = 
      (SoTransformManip *) manipPath->getTail();
   manip->replaceManip(manipPath, new SoTransform);
   manip->unref();

   // Unref the manipPath
   manipPath->unref();
}

//////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE  (part 1)

SoPath *
pickFilterCB(void *, const SoPickedPoint *pick)
{
   SoPath *filteredPath = NULL;
    
   // See if the picked object is a manipulator. 
   // If so, change the path so it points to the object the manip
   // is attached to.
   SoPath *p = pick->getPath();
   SoNode *n = p->getTail();
   if (n->isOfType(SoTransformManip::getClassTypeId())) {
      // Manip picked! We know the manip is attached
      // to its next sibling. Set up and return that path.
      int manipIndex = p->getIndex(p->getLength() - 1);
      filteredPath = p->copy(0, p->getLength() - 1);
      filteredPath->append(manipIndex + 1); // get next sibling
   }
   else filteredPath = p;
    
   return filteredPath;
}

// CODE FOR The Inventor Mentor ENDS HERE  
///////////////////////////////////////////////////////////////

// Create a sample scene graph
SoNode *
myText(char *str, int i, const SbColor &color)
{
   SoSeparator *sep = new SoSeparator;
   SoBaseColor *col = new SoBaseColor;
   SoTransform *xf = new SoTransform;
   SoText3 *text = new SoText3;
   
   col->rgb = color;
   xf->translation.setValue(6.0 * i, 0.0, 0.0);
   text->string = str;
   text->parts = (SoText3::FRONT | SoText3::SIDES);
   text->justification = SoText3::CENTER;
   sep->addChild(col);
   sep->addChild(xf);
   sep->addChild(text);
   
   return sep;
}

SoNode *
buildScene()
{
   SoSeparator *scene = new SoSeparator;
   SoFont *font = new SoFont;
   
   font->size = 10;
   scene->addChild(font);
   scene->addChild(myText("O",  0, SbColor(0, 0, 1)));
   scene->addChild(myText("p",  1, SbColor(0, 1, 0)));
   scene->addChild(myText("e",  2, SbColor(0, 1, 1)));
   scene->addChild(myText("n",  3, SbColor(1, 0, 0)));
   // Open Inventor is two words!
   scene->addChild(myText("I",  5, SbColor(1, 0, 1)));
   scene->addChild(myText("n",  6, SbColor(1, 1, 0)));
   scene->addChild(myText("v",  7, SbColor(1, 1, 1)));
   scene->addChild(myText("e",  8, SbColor(0, 0, 1)));
   scene->addChild(myText("n",  9, SbColor(0, 1, 0)));
   scene->addChild(myText("t", 10, SbColor(0, 1, 1)));
   scene->addChild(myText("o", 11, SbColor(1, 0, 0)));
   scene->addChild(myText("r", 12, SbColor(1, 0, 1)));
   
   return scene;
}

void
main(int , char *argv[])
{
   // Initialization
   Widget mainWindow = SoXt::init(argv[0]);
    
   // Create a scene graph. Use the toggle selection policy.
   SoSelection *sel = new SoSelection;
   sel->ref();
   sel->policy.setValue(SoSelection::TOGGLE);
   sel->addChild(buildScene());

   // Create a viewer
   SoXtExaminerViewer *viewer = new SoXtExaminerViewer(mainWindow);
   viewer->setSceneGraph(sel);
   viewer->setTitle("Select Through Manips");
   viewer->show();

   // Selection callbacks
   sel->addSelectionCallback(selCB);
   sel->addDeselectionCallback(deselCB);
   sel->setPickFilterCallback(pickFilterCB);
    
   SoXt::show(mainWindow);
   SoXt::mainLoop();
}

