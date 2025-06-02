#include <Inventor/SoDB.h>
#include <Inventor/SoInput.h>
#include <Inventor/nodekits/SoCameraKit.h>
#include <Inventor/nodekits/SoLightKit.h>
#include <Inventor/nodekits/SoSceneKit.h>
#include <Inventor/nodekits/SoWrapperKit.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoSeparator.h>

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtDirectionalLightEditor.h>
#include <Inventor/Xt/SoXtMaterialEditor.h>
#include <Inventor/Xt/SoXtRenderArea.h>

main(int , char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   // SCENE!
   SoSceneKit *myScene = new SoSceneKit;
   myScene->ref();

   // LIGHTS! Add an SoLightKit to the "lightList." The
   // SoLightKit creates an SoDirectionalLight by default.
   myScene->setPart("lightList[0]", new SoLightKit);

   // CAMERA!! Add an SoCameraKit to the "cameraList." The
   // SoCameraKit creates an SoPerspectiveCamera by default.
   myScene->setPart("cameraList[0]", new SoCameraKit);
   myScene->setCameraNumber(0);

   // Read an object from file.
   SoInput myInput;
   if (!myInput.openFile("Barcelona.iv"))
      return (1);
   SoSeparator *fileContents = SoDB::readAll(&myInput);
   if (fileContents == NULL) return (1);
   // OBJECT!! Create an SoWrapperKit and set its contents to
   // be what you read from file.
   SoWrapperKit *myDesk = new SoWrapperKit();
   myDesk->setPart("contents", fileContents);
   myScene->setPart("childList[0]", myDesk);
   // Give the desk a good starting color
   myDesk->set("material { diffuseColor .8 .3 .1 }");

   // MATERIAL EDITOR!!  Attach it to myDesk's material node.
   // Use the SO_GET_PART macro to get this part from myDesk.
   SoXtMaterialEditor *mtlEditor = new SoXtMaterialEditor();
   SoMaterial *mtl = SO_GET_PART(myDesk,"material",SoMaterial);
   mtlEditor->attach(mtl);
   mtlEditor->setTitle("Material of Desk");
   mtlEditor->show();

   // DIRECTIONAL LIGHT EDITOR!! Attach it to the
   // SoDirectionalLight node within the SoLightKit we made.
   SoXtDirectionalLightEditor *ltEditor =
                 new SoXtDirectionalLightEditor();
   SoPath *ltPath = myScene->createPathToPart(
      "lightList[0].light", TRUE);
   ltEditor->attach(ltPath);
   ltEditor->setTitle("Lighting of Desk");
   ltEditor->show();

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);

   // Set up Camera with ViewAll...
   // -- use the SO_GET_PART macro to get the camera node.
   // -- viewall is a method on the 'camera' part of
   //    the cameraKit, not on the cameraKit itself.  So the part
   //    we ask for is not 'cameraList[0]' (which is of type
   //    SoPerspectiveCameraKit), but
   //    'cameraList[0].camera' (which is of type
   //    SoPerspectiveCamera).
   SoPerspectiveCamera *myCamera = SO_GET_PART(myScene,
      "cameraList[0].camera", SoPerspectiveCamera);
   SbViewportRegion myRegion(myRenderArea->getSize());
   myCamera->viewAll(myScene, myRegion);
   myRenderArea->setSceneGraph(myScene);
   myRenderArea->setTitle("Main Window: Desk In A Scene Kit");
   myRenderArea->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}
