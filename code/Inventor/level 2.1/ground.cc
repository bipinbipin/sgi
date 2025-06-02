#include <math.h>

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>

#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/nodekits/SoCameraKit.h>
#include <Inventor/nodekits/SoLightKit.h>
#include <Inventor/nodekits/SoSceneKit.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoPointSet.h> 
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/sensors/SoTimerSensor.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoTranslation.h>

SoGroup *
buildMoon(float rayLen, float moonRadius)
{
   SoGroup *moonGroup = new SoGroup;
   SoShapeKit *ray = new SoShapeKit;
   SoShapeKit *moon = new SoShapeKit;

   SoTransform *xf1 = new SoTransform;
     SbRotation r(SbVec3f(1, 0, 0), (3*M_PI/2));
     xf1->rotation = r;
     xf1->translation.setValue(SbVec3f((rayLen/2), 0, 0));

   SoTransform *xf2 = new SoTransform;
     xf2->translation.setValue(SbVec3f(0, (rayLen/2), 0));

   SoCylinder *rayShape = new SoCylinder;
     rayShape->radius = 0.005;
     rayShape->height = rayLen;

   SoSphere *moonShape = new SoSphere;
     moonShape->radius = moonRadius;
   
   ray->setPart("shape", rayShape);
   ray->setPart("transform", xf1);

   moon->setPart("shape", moonShape);
   moon->setPart("transform", xf2);
   moon->set("material { diffuseColor 1 0 1 }");
   ray->setPart("childList[0]", moon);

   moonGroup->addChild(ray);

   return moonGroup;
}
  


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

///////////////////////
// BEGIN RENDER CODE //
///////////////////////
  int i,j;


  SoGroup *ground = new SoGroup;
  SoTranslation *lower = new SoTranslation;
  lower->translation = SbVec3f(0,-100000,0);

  ground->addChild(lower);

  SoCube *acre = new SoCube;
  acre->height = 1;
  acre->width = 1000;
  acre->depth= 1000;

  SoTranslation *deep = new SoTranslation;
  deep->translation = SbVec3f(1000,0,0);

  SoTranslation *deepReset = new SoTranslation;
  deepReset->translation = SbVec3f(-100*1000,0,0);

  SoTranslation *horizon = new SoTranslation;
  horizon->translation = SbVec3f(0,0,1000);

  for(j=0; j < 100; j++)
   {
     
  	for(i=0; i < 100; i++)
 	  {
 	    ground->addChild(acre);
 	    ground->addChild(deep);
 	  }
     ground->addChild(deepReset);
     ground->addChild(horizon);
   }   
 

 root->addChild(ground);


/////////////////////
// END RENDER CODE //
/////////////////////
  
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
