#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/nodes/SoSeparator.h>

// Positions of all of the vertices:
float rndnum_vert[160][3];
int i,j;

	

// Routine to create a scene graph representing an arch.
SoSeparator *
makeArch()
{
	for (i = 0;i < 160;i++)
	 {
		for (j = 0;j < 3;j++)
		  {
		    rndnum_vert[i][j] = mrand48();
		  }
		
		for (j = 0;j < 3;j++)
		  {    printf("%f,",rndnum_vert[i][j]);	  }
	   printf("\n");
	 }


   SoSeparator *result = new SoSeparator;
   result->ref();

#ifdef IV_STRICT
   // This is the preferred code for Inventor 2.1 

   // Using the new SoVertexProperty node is more efficient
   SoVertexProperty *myVertexProperty = new SoVertexProperty;

   // Define the material
   myVertexProperty->orderedRGBA.setValue(SbColor(.78, .57, .11).getPackedValue());

   // Define coordinates for vertices
   myVertexProperty->vertex.setValues(0, 60, rndnum_vert);

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
   myCoords->point.setValues(0, 60, rndnum_vert);
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
   myViewer->setTitle("RANDOM MESH");
   myViewer->show();
   myViewer->viewAll();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}
