#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoNormal.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoVertexProperty.h>


float vertices[28][3];

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
   int i,j;

    for (i = 0;i < 28;i++)
	 {
		for (j = 0;j < 3;j++)
		  {
		    vertices[i][j] = mrand48();
		  }
		for (j = 0;j < 3;j++)
		  {
		    printf("%f\t",vertices[i][j]);
		  }
	   printf("\n");
	 }
   
   SoSeparator *obelisk = new SoSeparator();
   obelisk->ref();

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

