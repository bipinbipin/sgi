#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>

#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoRotor.h>

#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoCompose.h>

void main(int argc, char **argv)
{
  //  INITIALIZE FOR RENDERING   //
  Widget myWindow = SoXt::init(argv[0]);
  // Construct root node and setup camera
  SoGroup *root = new SoGroup;
  root->ref();
  SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
  root->addChild(myCamera);
  root->addChild(new SoDirectionalLight);
  /////////////////////////////////

// BEGIN RENDER CODE

SoMaterial *bronze = new SoMaterial;

// set field values
bronze->ambientColor.setValue(.33, .22, .27);
bronze->diffuseColor.setValue(.78, .57, .11);
bronze->specularColor.setValue(.99, .94, .81);
bronze->shininess = .28;


SoCube *dks = new SoCube;

   // Set up transformations
   SoTranslation *slideTranslation = new SoTranslation;
   root->addChild(slideTranslation);
   SoTransform *initialTransform = new SoTransform;
   initialTransform->translation.setValue(-5., 0., 0.);
   initialTransform->scaleFactor.setValue(10., 10., 10.);
   initialTransform->rotation.setValue(SbVec3f(1,0,0), M_PI/2.);
   root->addChild(initialTransform);
   root->addChild(bronze);
   root->addChild(dks);

   // Make the X translation value change over time.
   SoElapsedTime *myCounter = new SoElapsedTime;
   SoComposeVec3f *slideDistance = new SoComposeVec3f;
   slideDistance->y.connectFrom(&myCounter->timeOut);
   slideTranslation->translation.connectFrom(
            &slideDistance->vector);






// END RENDER CODE

  // SET VIEWING WINDOW AND viewAll of root node 
  SoXtExaminerViewer *myViewer =
            new SoXtExaminerViewer(myWindow);
  myViewer->setSceneGraph(root);
  myViewer->setTitle("build by bit");
  myViewer->show();
  myViewer->viewAll();
  

  // NOTHING AFTER THESE 2 LINES
  SoXt::show(myWindow);
  SoXt::mainLoop();
}
