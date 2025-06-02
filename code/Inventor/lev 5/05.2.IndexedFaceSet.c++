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
 *  chapter 5, example 2.
 *
 *  This example creates an IndexedFaceSet. It creates 
 *  the first stellation of the dodecahedron.
 *------------------------------------------------------------*/

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoSeparator.h>

//////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE

//
// Positions of all of the vertices:
//
static float vertexPositions[12][3] =
{
   { 0.0000,  1.2142,  0.7453},  // top

   { 0.0000,  1.2142, -0.7453},  // points surrounding top
   {-1.2142,  0.7453,  0.0000},
   {-0.7453,  0.0000,  1.2142}, 
   { 0.7453,  0.0000,  1.2142}, 
   { 1.2142,  0.7453,  0.0000},

   { 0.0000, -1.2142,  0.7453},  // points surrounding bottom
   {-1.2142, -0.7453,  0.0000}, 
   {-0.7453,  0.0000, -1.2142},
   { 0.7453,  0.0000, -1.2142}, 
   { 1.2142, -0.7453,  0.0000}, 

   { 0.0000, -1.2142, -0.7453}, // bottom
};

//
// Connectivity, information; 12 faces with 5 vertices each },
// (plus the end-of-face indicator for each face):
//

static int32_t indices[72] =
{
   1,  2,  3,  4, 5, SO_END_FACE_INDEX, // top face

   0,  1,  8,  7, 3, SO_END_FACE_INDEX, // 5 faces about top
   0,  2,  7,  6, 4, SO_END_FACE_INDEX,
   0,  3,  6, 10, 5, SO_END_FACE_INDEX,
   0,  4, 10,  9, 1, SO_END_FACE_INDEX,
   0,  5,  9,  8, 2, SO_END_FACE_INDEX, 

    9,  5, 4, 6, 11, SO_END_FACE_INDEX, // 5 faces about bottom
   10,  4, 3, 7, 11, SO_END_FACE_INDEX,
    6,  3, 2, 8, 11, SO_END_FACE_INDEX,
    7,  2, 1, 9, 11, SO_END_FACE_INDEX,
    8,  1, 5,10, 11, SO_END_FACE_INDEX,

    6,  7, 8, 9, 10, SO_END_FACE_INDEX, // bottom face
};
 
// Colors for the 12 faces
static float colors[12][3] =
{
   {1.0, .0, 0}, { .0,  .0, 1.0}, {0, .7,  .7}, { .0, 1.0,  0},
   { .7, .7, 0}, { .7,  .0,  .7}, {0, .0, 1.0}, { .7,  .0, .7},
   { .7, .7, 0}, { .0, 1.0,  .0}, {0, .7,  .7}, {1.0,  .0,  0}
};

// Routine to create a scene graph representing a dodecahedron
SoSeparator *
makeStellatedDodecahedron()
{
   SoSeparator *result = new SoSeparator;
   result->ref();

#ifdef IV_STRICT
   // This is the preferred code for Inventor 2.1

   // Using the new SoVertexProperty node is more efficient
   SoVertexProperty *myVertexProperty = new SoVertexProperty;

   // Define colors for the faces
   for (int i=0; i<12; i++)
     myVertexProperty->orderedRGBA.set1Value(i, SbColor(colors[i]).getPackedValue());
   myVertexProperty->materialBinding = SoMaterialBinding::PER_FACE;

   // Define coordinates for vertices
   myVertexProperty->vertex.setValues(0, 12, vertexPositions);

   // Define the IndexedFaceSet, with indices into
   // the vertices:
   SoIndexedFaceSet *myFaceSet = new SoIndexedFaceSet;
   myFaceSet->coordIndex.setValues(0, 72, indices);

   myFaceSet->vertexProperty.setValue(myVertexProperty);
   result->addChild(myFaceSet);

#else
   // Define colors for the faces
   SoMaterial *myMaterials = new SoMaterial;
   myMaterials->diffuseColor.setValues(0, 12, colors);
   result->addChild(myMaterials);
   SoMaterialBinding *myMaterialBinding = new SoMaterialBinding;
   myMaterialBinding->value = SoMaterialBinding::PER_FACE;
   result->addChild(myMaterialBinding);

   // Define coordinates for vertices
   SoCoordinate3 *myCoords = new SoCoordinate3;
   myCoords->point.setValues(0, 12, vertexPositions);
   result->addChild(myCoords);

   // Define the IndexedFaceSet, with indices into
   // the vertices:
   SoIndexedFaceSet *myFaceSet = new SoIndexedFaceSet;
   myFaceSet->coordIndex.setValues(0, 72, indices);
   result->addChild(myFaceSet);
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

   SoSeparator *root = makeStellatedDodecahedron();
   root->ref();

   SoXtExaminerViewer *myViewer = 
            new SoXtExaminerViewer(myWindow);
   myViewer->setSceneGraph(root);
   myViewer->setTitle("Indexed Face Set: Stellated Dodecahedron");
   myViewer->show();
   myViewer->viewAll();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}

