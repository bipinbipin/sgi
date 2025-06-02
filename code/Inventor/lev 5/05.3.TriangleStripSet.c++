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
 *  chapter 5, example 3.
 *
 *  This example creates a TriangleStripSet. It creates
 *  a pennant-shaped flag.
 *------------------------------------------------------------*/

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShapeHints.h>
#include <Inventor/nodes/SoTriangleStripSet.h>

//////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE

//
// Positions of all of the vertices:
//
static float vertexPositions[40][3] =
{
   {  0,   12,    0 }, {   0,   15,    0},
   {2.1, 12.1,  -.2 }, { 2.1, 14.6,  -.2},
   {  4, 12.5,  -.7 }, {   4, 14.5,  -.7},
   {4.5, 12.6,  -.8 }, { 4.5, 14.4,  -.8},
   {  5, 12.7,   -1 }, {   5, 14.4,   -1},
   {4.5, 12.8, -1.4 }, { 4.5, 14.6, -1.4},
   {  4, 12.9, -1.6 }, {   4, 14.8, -1.6},
   {3.3, 12.9, -1.8 }, { 3.3, 14.9, -1.8},
   {  3,   13, -2.0 }, {   3, 14.9, -2.0}, 
   {3.3, 13.1, -2.2 }, { 3.3, 15.0, -2.2},
   {  4, 13.2, -2.5 }, {   4, 15.0, -2.5},
   {  6, 13.5, -2.2 }, {   6, 14.8, -2.2},
   {  8, 13.4,   -2 }, {   8, 14.6,   -2},
   { 10, 13.7, -1.8 }, {  10, 14.4, -1.8},
   { 12,   14, -1.3 }, {  12, 14.5, -1.3},
   { 15, 14.9, -1.2 }, {  15,   15, -1.2},

   {-.5, 15,   0 }, { -.5, 0,   0},   // the flagpole
   {  0, 15,  .5 }, {   0, 0,  .5},
   {  0, 15, -.5 }, {   0, 0, -.5},
   {-.5, 15,   0 }, { -.5, 0,   0}
};


// Number of vertices in each strip.
static int32_t numVertices[2] =
{
   32, // flag
   8   // pole
};
 
// Colors for the 12 faces
static float colors[2][3] =
{
   { .5, .5,  1 }, // purple flag
   { .4, .4, .4 }, // grey flagpole
};

// Routine to create a scene graph representing a pennant.
SoSeparator *
makePennant()
{
   SoSeparator *result = new SoSeparator;
   result->ref();

   // A shape hints tells the ordering of polygons. 
   // This insures double sided lighting.
   SoShapeHints *myHints = new SoShapeHints;
   myHints->vertexOrdering = SoShapeHints::COUNTERCLOCKWISE;
   result->addChild(myHints);

#ifdef IV_STRICT
   // This is the preferred code for Inventor 2.1 

   // Using the new SoVertexProperty node is more efficient
   SoVertexProperty *myVertexProperty = new SoVertexProperty;

   // Define colors for the strips
   for (int i=0; i<2; i++)
     myVertexProperty->orderedRGBA.set1Value(i, SbColor(colors[i]).getPackedValue());
   myVertexProperty->materialBinding = SoMaterialBinding::PER_PART;

   // Define coordinates for vertices
   myVertexProperty->vertex.setValues(0, 40, vertexPositions);

   // Define the TriangleStripSet, made of two strips.
   SoTriangleStripSet *myStrips = new SoTriangleStripSet;
   myStrips->numVertices.setValues(0, 2, numVertices);
 
   myStrips->vertexProperty.setValue(myVertexProperty);
   result->addChild(myStrips);

#else
   // Define colors for the strips
   SoMaterial *myMaterials = new SoMaterial;
   myMaterials->diffuseColor.setValues(0, 2, colors);
   result->addChild(myMaterials);
   SoMaterialBinding *myMaterialBinding = new SoMaterialBinding;
   myMaterialBinding->value = SoMaterialBinding::PER_PART;
   result->addChild(myMaterialBinding);

   // Define coordinates for vertices
   SoCoordinate3 *myCoords = new SoCoordinate3;
   myCoords->point.setValues(0, 40, vertexPositions);
   result->addChild(myCoords);

   // Define the TriangleStripSet, made of two strips.
   SoTriangleStripSet *myStrips = new SoTriangleStripSet;
   myStrips->numVertices.setValues(0, 2, numVertices);
   result->addChild(myStrips);
#endif

   result->unrefNoDelete();
   return result;
}

// CODE FOR The Inventor Mentor ENDS HERE
//////////////////////////////////////////////////////////////

void
main(int, char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   SoSeparator *root = makePennant();
   root->ref();

   SoXtExaminerViewer *myViewer = 
            new SoXtExaminerViewer(myWindow);
   myViewer->setSceneGraph(root);
   myViewer->setTitle("Triangle Strip Set: Pennant");
   myViewer->show();
   myViewer->viewAll();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}

