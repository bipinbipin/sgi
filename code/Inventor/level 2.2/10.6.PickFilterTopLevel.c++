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
 *  chapter 10, example 6.
 *
 *  This example demonstrates the use of the pick filter
 *  callback to implement a top level selection policy.
 *  That is, always select the top most group beneath the
 *  selection node,  rather than selecting the actual
 *  shape that was picked.
 *------------------------------------------------------------*/

#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/actions/SoBoxHighlightRenderAction.h>
#include <Inventor/nodes/SoSelection.h>


// Pick the topmost node beneath the selection node
SoPath *
pickFilterCB(void *, const SoPickedPoint *pick)
{    
   // See which child of selection got picked
   SoPath *p = pick->getPath();
   int i;
   for (i = 0; i < p->getLength() - 1; i++) {
      SoNode *n = p->getNode(i);
      if (n->isOfType(SoSelection::getClassTypeId()))
         break;
   }
   
   // Copy 2 nodes from the path:
   // selection and the picked child
   return p->copy(i, 2);
}


void
main(int argc, char *argv[])
{
   // Initialization
   Widget mainWindow = SoXt::init(argv[0]);
    
   // Open the data file
   SoInput in;   
   char *datafile = "/usr/share/src/Inventor/examples/data/parkbench.iv";
   if (! in.openFile(datafile)) {
      fprintf(stderr, "Cannot open %s for reading.\n", datafile);
      return;
   }

   // Read the input file
   SoNode *n;
   SoSeparator *sep = new SoSeparator;
   while ((SoDB::read(&in, n) != FALSE) && (n != NULL))
      sep->addChild(n);
   
   // Create two selection roots - one will use the pick filter.
   SoSelection *topLevelSel = new SoSelection;
   topLevelSel->addChild(sep);
   topLevelSel->setPickFilterCallback(pickFilterCB);

   SoSelection *defaultSel = new SoSelection;
   defaultSel->addChild(sep);

   // Create two viewers, one to show the pick filter for top level
   // selection, the other to show default selection.
   SoXtExaminerViewer *viewer1 = new SoXtExaminerViewer(mainWindow);
   viewer1->setSceneGraph(topLevelSel);
   viewer1->setGLRenderAction(new SoBoxHighlightRenderAction());
   viewer1->redrawOnSelectionChange(topLevelSel);
   viewer1->setTitle("Top Level Selection");

   SoXtExaminerViewer *viewer2 = new SoXtExaminerViewer();
   viewer2->setSceneGraph(defaultSel);
   viewer2->setGLRenderAction(new SoBoxHighlightRenderAction());    
   viewer2->redrawOnSelectionChange(defaultSel);
   viewer2->setTitle("Default Selection");

   viewer1->show();
   viewer2->show();
   
   SoXt::show(mainWindow);
   SoXt::mainLoop();
}

