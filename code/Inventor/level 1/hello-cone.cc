#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>

main(int , char **argv)
{
   // Initialize Inventor. This returns a main window to use.
   // If unsuccessful, exit.
   Widget myWindow = SoXt::init(argv[0]); // pass the app name
   if (myWindow == NULL) exit(1);

   // Make a scene containing a red cone
   SoSeparator *root = new SoSeparator;
   SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
   SoMaterial *myMaterial = new SoMaterial;
   root->ref();
   root->addChild(myCamera);
   root->addChild(new SoDirectionalLight);
   myMaterial->diffuseColor.setValue(1.0, 1.0, 0.0);   // Red
   root->addChild(myMaterial);
   root->addChild(new SoCone);

   // Create a renderArea in which to see our scene graph.
   // The render area will appear within the main window.
   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);

   // Make myCamera see everything.
   myCamera->viewAll(root, myRenderArea->getViewportRegion());

   // Put our scene in myRenderArea, change the title
   myRenderArea->setSceneGraph(root);
   myRenderArea->setTitle("Hello Cone");
   myRenderArea->show();

   SoXt::show(myWindow);  // Display main window
   SoXt::mainLoop();      // Main Inventor event loop
}


