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

/*-----------------------------------------------------------
 *  This is an example from The Inventor Mentor,
 *  chapter 9, example 5.
 *
 *  Using a callback for generated primitives.
 *  A simple scene with a sphere is created.
 *  A callback is used to write out the triangles that
 *  form the sphere in the scene.
 *----------------------------------------------------------*/

#include <Inventor/SoDB.h>
#include <Inventor/SoPrimitiveVertex.h>
#include <Inventor/actions/SoCallbackAction.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>

// Function prototypes
void printSpheres(SoNode *);
SoCallbackAction::Response printHeaderCallback(void *, 
   SoCallbackAction *, const SoNode *);
void printTriangleCallback(void *, SoCallbackAction *,
   const SoPrimitiveVertex *, const SoPrimitiveVertex *,
   const SoPrimitiveVertex *);
void printVertex(const SoPrimitiveVertex *);

//////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE

void
printSpheres(SoNode *root)
{
   SoCallbackAction myAction;

   myAction.addPreCallback(SoSphere::getClassTypeId(), 
            printHeaderCallback, NULL);
   myAction.addTriangleCallback(SoSphere::getClassTypeId(), 
            printTriangleCallback, NULL);

   myAction.apply(root);
}

SoCallbackAction::Response
printHeaderCallback(void *, SoCallbackAction *, 
      const SoNode *node)
{
   printf("\n Sphere ");
   // Print the node name (if it exists) and address
   if (! !node->getName())
      printf("named \"%s\" ", node->getName());
   printf("at address %#x\n", node);

   return SoCallbackAction::CONTINUE;
}

void
printTriangleCallback(void *, SoCallbackAction *,
   const SoPrimitiveVertex *vertex1,
   const SoPrimitiveVertex *vertex2,
   const SoPrimitiveVertex *vertex3)
{
   printf("Triangle:\n");
   printVertex(vertex1);
   printVertex(vertex2);
   printVertex(vertex3);
}

void
printVertex(const SoPrimitiveVertex *vertex)
{
   const SbVec3f &point = vertex->getPoint();
   printf("\tCoords     = (%g, %g, %g)\n", 
               point[0], point[1], point[2]);

   const SbVec3f &normal = vertex->getNormal();
   printf("\tNormal     = (%g, %g, %g)\n", 
               normal[0], normal[1], normal[2]);
}

// CODE FOR The Inventor Mentor ENDS HERE
///////////////////////////////////////////////////////////////

main(int, char **)
{
   // Initialize Inventor
   SoDB::init();

   // Make a scene containing a red sphere
   SoSeparator *root = new SoSeparator;
   SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
   SoMaterial *myMaterial = new SoMaterial;
   root->ref();
   root->addChild(myCamera);
   root->addChild(new SoDirectionalLight);
   myMaterial->diffuseColor.setValue(1.0, 0.0, 0.0);   // Red
   root->addChild(myMaterial);
   root->addChild(new SoSphere);
   root->ref();

   // Write out the triangles that form the sphere in the scene
   printSpheres(root);

   root->unref();
   return 0;
}
