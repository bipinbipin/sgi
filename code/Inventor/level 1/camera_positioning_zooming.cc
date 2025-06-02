#include <Inventor/SbLinear.h>
#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/nodes/SoBlinker.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>

main(int, char **argv)
{
   int i;

   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL)
      exit(1);

   SoSeparator *root = new SoSeparator;
   root->ref();
   
   // Create a blinker node and put it in the scene. A blinker
   // switches between its children at timed intervals.
   SoBlinker *myBlinker = new SoBlinker;
   root->addChild(myBlinker);

   // Create three cameras. Their positions will be set later.
   // This is because the viewAll method depends on the size
   // of the render area, which has not been created yet.
   SoOrthographicCamera *orthoViewAll = new SoOrthographicCamera;
   SoPerspectiveCamera *perspViewAll = new SoPerspectiveCamera;
   SoPerspectiveCamera *perspOffCenter = new SoPerspectiveCamera;
   myBlinker->addChild(orthoViewAll);
   myBlinker->addChild(perspViewAll);
   myBlinker->addChild(perspOffCenter);

   // Create a light
   root->addChild(new SoDirectionalLight);

   // Read the object from a file and add to the scene
   SoInput myInput;
   if (! myInput.openFile("Barcelona.iv"))
      return 1;
   SoSeparator *fileContents = SoDB::readAll(&myInput);
   if (fileContents == NULL)
      return 1;

   SoMaterial *myMaterial = new SoMaterial;
   myMaterial->diffuseColor.setValue(0.8, 0.23, 0.03);
   root->addChild(myMaterial);
   root->addChild(fileContents);

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);


   // Establish camera positions.
   // First do a viewAll() on all three cameras. 
   // Then modify the position of the off-center camera.
   SbViewportRegion myRegion(myRenderArea->getSize());
   orthoViewAll->viewAll(root, myRegion);
   perspViewAll->viewAll(root, myRegion);
   perspOffCenter->viewAll(root, myRegion);
   SbVec3f initialPos;
   initialPos = perspOffCenter->position.getValue();
   float x, y, z;
   initialPos.getValue(x,y,z);
   perspOffCenter->position.setValue(x+x/2., y+y/2., z+z/4.);
   
   for(i=1;i<10;i++)
   {
   x = x - i;
   perspOffCenter->position.setValue(x+x/2., y+y/2., z+z/4.);
   }

   myRenderArea->setSceneGraph(root);
   myRenderArea->setTitle("Cameras");
   myRenderArea->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}


