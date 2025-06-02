#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoQuadMesh.h>
#include <Inventor/nodes/SoSeparator.h>

// Positions of all of the vertices:
float rndnum_vert[1000][3];
float colors[1000][3];
int i,j;

	

// Routine to create a scene graph representing an arch.
SoSeparator *
makeArch()
{
	rndnum_vert[0][0] = 2;
	rndnum_vert[0][1] = 2;
	rndnum_vert[0][2] = 2000;

	for (i = 1;i < 1000;i++)
	 {
	   rndnum_vert[i][0] = 2 + rndnum_vert[i-1][0];
	   rndnum_vert[i][1] = lrand48() / 1000000;
	   rndnum_vert[i][2] = rndnum_vert[i-1][2] - 2;
		  		
		for (j = 0;j < 3;j++)
		  {    printf("%f,",rndnum_vert[i][j]);	  }
	   printf("\n");
	 }

     for (i = 0; i < 1000; i++)
	{
	  for (j = 0;j < 3;j++)
	  {
		colors[i][j] = drand48();
		//printf("%f,",colors[i][j]);
	  }
	  //printf("\n");
	}


   SoSeparator *result = new SoSeparator;
   result->ref();

   // Using the new SoVertexProperty node is more efficient
   SoVertexProperty *myVertexProperty = new SoVertexProperty;

   // Define the material
   for (int c=0; c<1000; c++)
     myVertexProperty->orderedRGBA.set1Value(c, SbColor(colors[c]).getPackedValue());
  
   myVertexProperty->materialBinding = SoMaterialBinding::PER_VERTEX;

   // Define coordinates for vertices
   myVertexProperty->vertex.setValues(0, 1000, rndnum_vert);

   // Define the QuadMesh.
   SoQuadMesh *myQuadMesh = new SoQuadMesh;

   myQuadMesh->verticesPerRow = 200;
   myQuadMesh->verticesPerColumn = 5;

   myQuadMesh->vertexProperty.setValue(myVertexProperty);
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
