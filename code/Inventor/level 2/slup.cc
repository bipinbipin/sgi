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

#include <Inventor/engines/SoTimeCounter.h>
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

SoCube *dks = new SoCube;

SoTranslation *jumpTranslation = new SoTranslation;
root->addChild(jumpTranslation);

   // Set up transformations
   SoTranslation *slideTranslation = new SoTranslation;
   root->addChild(slideTranslation);
   SoTransform *initialTransform = new SoTransform;
   initialTransform->translation.setValue(-5., 0., 0.);
   initialTransform->scaleFactor.setValue(10., 10., 10.);
   initialTransform->rotation.setValue(SbVec3f(1,0,0), M_PI/2.);
   root->addChild(initialTransform);

   root->addChild(dks);

  
// Create two counters, and connect to X and Y translations.
// The Y counter is small and high frequency.
// The X counter is large and low frequency.
// This results in small jumps across the screen,
// left to right, again and again and again.
SoTimeCounter *jumpHeightCounter = new SoTimeCounter;
SoTimeCounter *jumpWidthCounter = new SoTimeCounter;
SoComposeVec3f *jump = new SoComposeVec3f;
  
jumpHeightCounter->max = 4;
jumpHeightCounter->frequency = 3.5;
jumpWidthCounter->max = 40;
jumpWidthCounter->frequency = 0.15;
  
jump->x.connectFrom(&jumpWidthCounter->output);
jump->y.connectFrom(&jumpHeightCounter->output);
jumpTranslation->translation.connectFrom(&jump->vector);





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
