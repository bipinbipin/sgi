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
 *  chapter 5, example 4.
 *
 *  This example creates the St. Louis Arch using a QuadMesh.
 *------------------------------------------------------------*/

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/nodes/SoSeparator.h>

//////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE

// Positions of all of the vertices:
static float vertexPositions[160][3] =
{  // 1st row
   {-13.0,  0.0, 1.5}, {-10.3, 13.7, 1.2}, { -7.6, 21.7, 1.0}, 
   { -5.0, 26.1, 0.8}, { -2.3, 28.2, 0.6}, { -0.3, 28.8, 0.5},
   {  0.3, 28.8, 0.5}, {  2.3, 28.2, 0.6}, {  5.0, 26.1, 0.8}, 
   {  7.6, 21.7, 1.0}, { 10.3, 13.7, 1.2}, { 13.0,  0.0, 1.5},
   // 2nd row
   {-10.0,  0.0, 1.5}, { -7.9, 13.2, 1.2}, { -5.8, 20.8, 1.0}, 
   { -3.8, 25.0, 0.8}, { -1.7, 27.1, 0.6}, { -0.2, 27.6, 0.5},
   {  0.2, 27.6, 0.5}, {  1.7, 27.1, 0.6}, {  3.8, 25.0, 0.8}, 
   {  5.8, 20.8, 1.0}, {  7.9, 13.2, 1.2}, { 10.0,  0.0, 1.5},
   // 3rd row
   {-10.0,  0.0,-1.5}, { -7.9, 13.2,-1.2}, { -5.8, 20.8,-1.0}, 
   { -3.8, 25.0,-0.8}, { -1.7, 27.1,-0.6}, { -0.2, 27.6,-0.5},
   {  0.2, 27.6,-0.5}, {  1.7, 27.1,-0.6}, {  3.8, 25.0,-0.8}, 
   {  5.8, 20.8,-1.0}, {  7.9, 13.2,-1.2}, { 10.0,  0.0,-1.5},
   // 4th row 
   {-13.0,  0.0,-1.5}, {-10.3, 13.7,-1.2}, { -7.6, 21.7,-1.0}, 
   { -5.0, 26.1,-0.8}, { -2.3, 28.2,-0.6}, { -0.3, 28.8,-0.5},
   {  0.3, 28.8,-0.5}, {  2.3, 28.2,-0.6}, {  5.0, 26.1,-0.8}, 
   {  7.6, 21.7,-1.0}, { 10.3, 13.7,-1.2}, { 13.0,  0.0,-1.5},
   // 5th row
   {-13.0,  0.0, 1.5}, {-10.3, 13.7, 1.2}, { -7.6, 21.7, 1.0}, 
   { -5.0, 26.1, 0.8}, { -2.3, 28.2, 0.6}, { -0.3, 28.8, 0.5},
   {  0.3, 28.8, 0.5}, {  2.3, 28.2, 0.6}, {  5.0, 26.1, 0.8}, 
   {  7.6, 21.7, 1.0}, { 10.3, 13.7, 1.2}, { 13.0,  0.0, 1.5}
};

// Routine to create a scene graph representing an arch.
SoSeparator *
makeArch()
{
   SoSeparator *result = new SoSeparator;
   result->ref();

#ifdef IV_STRICT
   // This is the preferred code for Inventor 2.1 

   // Using the new SoVertexProperty node is more efficient
   SoVertexProperty *myVertexProperty = new SoVertexProperty;

   // Define the material
   myVertexProperty->orderedRGBA.setValue(SbColor(.78, .57, .11).getPackedValue());

   // Define coordinates for vertices
   myVertexProperty->vertex.setValues(0, 60, vertexPositions);

   // Define the QuadMesh.
   SoQuadMesh *myQuadMesh = new SoQuadMesh;
   myQuadMesh->verticesPerRow = 12;

   myQuadMesh->verticesPerColumn = 5;

   myQuadMesh->vertexProperty.setValue(myVertexProperty);
   result->addChild(myQuadMesh);

#else
   // Define the material
   SoMaterial *myMaterial = new SoMaterial;
   myMaterial->diffuseColor.setValue(.78, .57, .11);
   result->addChild(myMaterial);

   // Define coordinates for vertices
   SoCoordinate3 *myCoords = new SoCoordinate3;
   myCoords->point.setValues(0, 60, vertexPositions);
   result->addChild(myCoords);

   // Define the QuadMesh.
   SoQuadMesh *myQuadMesh = new SoQuadMesh;
   myQuadMesh->verticesPerRow = 12;
#endif

   myQuadMesh->verticesPerColumn = 5;
   result->addChild(myQuadMesh);

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

   SoSeparator *root = makeArch();
   root->ref();

   SoXtExaminerViewer *myViewer = 
            new SoXtExaminerViewer(myWindow);
   myViewer->setSceneGraph(root);
   myViewer->setTitle("Quad Mesh: Arch");
   myViewer->show();
   myViewer->viewAll();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}
