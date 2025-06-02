#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoSeparator.h>

main(int , char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]); // pass the app name
   if (myWindow == NULL) exit(1);

   // Make a scene containing a red cone
   SoSeparator *root = new SoSeparator;
   root->ref();
   SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
   root->addChild(myCamera);
   root->addChild(new SoDirectionalLight);

   // This transformation is modified to rotate the cone
   SoRotationXYZ *myRotZ = new SoRotationXYZ;
   root->addChild(myRotZ);
   SoRotationXYZ *myRotY = new SoRotationXYZ;
   root->addChild(myRotY);
   SoRotationXYZ *myRotX = new SoRotationXYZ;
   root->addChild(myRotX);

   SoMaterial *myMaterial = new SoMaterial;
   myMaterial->diffuseColor.setValue(1.0, 1.0, 0.0);   // Red
   root->addChild(myMaterial);
   root->addChild(new SoCone);

   //An engine rotates the object. The output of myCounter
   //is the time in seconds since the program started.
   //this output is connected to the angle field of myRotXYZ
   myRotZ->axis = SoRotationXYZ::Z;   //rotate about Z axis
   myRotY->axis = SoRotationXYZ::Y;   //rotate about Y axis
   myRotX->axis = SoRotationXYZ::X;   //rotate about X axis
   SoElapsedTime *myCounter = new SoElapsedTime;
   myRotZ->angle.connectFrom(&myCounter->timeOut);
   myRotY->angle.connectFrom(&myCounter->timeOut);
   myRotX->angle.connectFrom(&myCounter->timeOut);


   // Create a renderArea in which to see our scene graph.
   // The render area will appear within the main window.
   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);

   // Make myCamera see everything.
   myCamera->viewAll(root, myRenderArea->getViewportRegion());

   // Put our scene in myRenderArea, change the title
   myRenderArea->setSceneGraph(root);
   myRenderArea->setTitle("Hello Cone 3d rotation");
   myRenderArea->show();

   SoXt::show(myWindow);  // Display main window
   SoXt::mainLoop();      // Main Inventor event loop
}


