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
 *  chapter 5, example 1.
 *
 *  This example builds an obelisk using the Face Set node.
 *------------------------------------------------------------*/

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoVertexProperty.h>

//////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE

//  Eight polygons. The first four are triangles 
//  The second four are quadrilaterals for the sides.
static float vertices[28][3] =
{
   { 0, 30, 0}, {-2,27, 2}, { 2,27, 2},            //front tri
   { 0, 30, 0}, {-2,27,-2}, {-2,27, 2},            //left  tri
   { 0, 30, 0}, { 2,27,-2}, {-2,27,-2},            //rear  tri
   { 0, 30, 0}, { 2,27, 2}, { 2,27,-2},            //right tri
   {-2, 27, 2}, {-4,0, 4}, { 4,0, 4}, { 2,27, 2},  //front quad
   {-2, 27,-2}, {-4,0,-4}, {-4,0, 4}, {-2,27, 2},  //left  quad
   { 2, 27,-2}, { 4,0,-4}, {-4,0,-4}, {-2,27,-2},  //rear  quad
   { 2, 27, 2}, { 4,0, 4}, { 4,0,-4}, { 2,27,-2}   //right quad
};

// Number of vertices in each polygon:
static int32_t numvertices[8] = {3, 3, 3, 3, 4, 4, 4, 4};

// Normals for each polygon:
static float norms[8][3] =
{ 
   {0, .555,  .832}, {-.832, .555, 0}, //front, left tris
   {0, .555, -.832}, { .832, .555, 0}, //rear, right tris
   
   {0, .0739,  .9973}, {-.9972, .0739, 0},//front, left quads
   {0, .0739, -.9973}, { .9972, .0739, 0},//rear, right quads
};

SoSeparator *
makeObeliskFaceSet()
{
   SoSeparator *obelisk = new SoSeparator();
   obelisk->ref();

#ifdef IV_STRICT
   // This is the preferred code for Inventor 2.1
 
   // Using the new SoVertexProperty node is more efficient
   SoVertexProperty *myVertexProperty = new SoVertexProperty;

   // Define the normals used:
   myVertexProperty->normal.setValues(0, 8, norms);
   myVertexProperty->normalBinding = SoNormalBinding::PER_FACE;

   // Define material for obelisk
   myVertexProperty->orderedRGBA.setValue(SbColor(.4,.4,.4).getPackedValue());

   // Define coordinates for vertices
   myVertexProperty->vertex.setValues(0, 28, vertices);

   // Define the FaceSet
   SoFaceSet *myFaceSet = new SoFaceSet;
   myFaceSet->numVertices.setValues(0, 8, numvertices);
 
   myFaceSet->vertexProperty.setValue(myVertexProperty);
   obelisk->addChild(myFaceSet);

#else
   // Define the normals used:
   SoNormal *myNormals = new SoNormal;
   myNormals->vector.setValues(0, 8, norms);
   obelisk->addChild(myNormals);
   SoNormalBinding *myNormalBinding = new SoNormalBinding;
   myNormalBinding->value = SoNormalBinding::PER_FACE;
   obelisk->addChild(myNormalBinding);

   // Define material for obelisk
   SoMaterial *myMaterial = new SoMaterial;
   myMaterial->diffuseColor.setValue(.4, .4, .4);
   obelisk->addChild(myMaterial);

   // Define coordinates for vertices
   SoCoordinate3 *myCoords = new SoCoordinate3;
   myCoords->point.setValues(0, 28, vertices);
   obelisk->addChild(myCoords);

   // Define the FaceSet
   SoFaceSet *myFaceSet = new SoFaceSet;
   myFaceSet->numVertices.setValues(0, 8, numvertices);
   obelisk->addChild(myFaceSet);
#endif

   obelisk->unrefNoDelete();
   return obelisk;
}

// CODE FOR The Inventor Mentor ENDS HERE
//////////////////////////////////////////////////////////////

void
main(int, char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   SoSeparator *root = new SoSeparator;
   root->ref();

   root->addChild(makeObeliskFaceSet());

   SoXtExaminerViewer *myViewer = 
            new SoXtExaminerViewer(myWindow);
   myViewer->setSceneGraph(root);
   myViewer->setTitle("Face Set: Obelisk");
   myViewer->show();
   myViewer->viewAll();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}

