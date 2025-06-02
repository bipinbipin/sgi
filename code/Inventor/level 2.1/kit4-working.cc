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


// Timer sensor - Rotate 90 degrees every second, update 30 times a second
SoTimerSensor *myTicker;
SoTimerSensor *myTicker1;
SoTimerSensor *myTicker2;

#define UPDATE_RATE 1.0/30.0
#define ROTATION_ANGLE M_PI/60.0


void
tickerCallback(void *userData, SoSensor *)
{
   SoCamera *myCamera = (SoCamera *) userData;
   SbRotation rot;
   SbMatrix mtx;
   SbVec3f pos;
   
   // Adjust the position
   pos = myCamera->position.getValue();
   rot = SbRotation(SbVec3f(0,1,0), ROTATION_ANGLE);
   mtx.setRotate(rot);
   mtx.multVecMatrix(pos, pos);
   myCamera->position.setValue(pos);
   
   // Adjust the orientation
   myCamera->orientation.setValue(
            myCamera->orientation.getValue() * rot);
}
void
tickerCallback1(void *userData, SoSensor *)
{
   SoCamera *myCamera = (SoCamera *) userData;
   SbRotation rot;
   SbMatrix mtx;
   SbVec3f pos;
   
   // Adjust the position
   pos = myCamera->position.getValue();
   rot = SbRotation(SbVec3f(1,0,0), ROTATION_ANGLE);
   mtx.setRotate(rot);
   mtx.multVecMatrix(pos, pos);
   myCamera->position.setValue(pos);
   
   // Adjust the orientation
   myCamera->orientation.setValue(
            myCamera->orientation.getValue() * rot);
}
void
tickerCallback2(void *userData, SoSensor *)
{
   SoCamera *myCamera = (SoCamera *) userData;
   SbRotation rot;
   SbMatrix mtx;
   SbVec3f pos;
   
   // Adjust the position
   pos = myCamera->position.getValue();
   rot = SbRotation(SbVec3f(0,0,1), ROTATION_ANGLE);
   mtx.setRotate(rot);
   mtx.multVecMatrix(pos, pos);
   myCamera->position.setValue(pos);
   
   // Adjust the orientation
   myCamera->orientation.setValue(
            myCamera->orientation.getValue() * rot);
}



SbBool
myAppEventHandler(void *userData, XAnyEvent *anyevent)
{
   XButtonEvent *myButtonEvent;
   SbVec3f vec;
   SbBool handled = TRUE;

 switch (anyevent->type) {
   
   case ButtonPress:
      myButtonEvent = (XButtonEvent *) anyevent;
      if (myButtonEvent->button == Button1) {
         myTicker->schedule();  
      } else if (myButtonEvent->button == Button2) {
	 myTicker1->schedule();  
      } else if (myButtonEvent->button == Button3) {
         myTicker2->schedule();  
      }
      break;
      
   case ButtonRelease:
      myButtonEvent = (XButtonEvent *) anyevent;
      if (myButtonEvent->button == Button1) {
         myTicker->unschedule(); 
      } else if (myButtonEvent->button == Button2) {
         myTicker1->unschedule();  
      } else if (myButtonEvent->button == Button3) {
         myTicker2->unschedule();  
      }
      break;
      
//  case MotionNotify:
//      myMotionEvent = (XMotionEvent *) anyevent;
//      if (myMotionEvent->state & Button1Mask) {  
//         myProjectPoint(myRenderArea, 
//                  myMotionEvent->x, myMotionEvent->y, vec);
//         myAddPoint(myRenderArea, vec);
//      }
//      break;
      
   default:
      handled = FALSE;
      break;
   }
return handled;
}

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
  

void
main(int , char **argv)
{
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   SbRotation r;
   SbVec3f xAxis(1, 0, 0), yAxis(0, 1, 0), zAxis(0, 0, 1);
   SbVec3f trans1, trans2;
   //float x, y, z;
   float angle[3];
    angle[0] = (M_PI/2); angle[1] = M_PI; angle[2] = (3*M_PI/2);  
   SoTransform *xf1 = new SoTransform;
   SoTransform *xf2 = new SoTransform;

   SoSceneKit *myScene = new SoSceneKit;
   myScene->ref();
   myScene->setPart("lightList[0]", new SoLightKit);
   myScene->setPart("cameraList[0]", new SoCameraKit);
   myScene->setCameraNumber(0);


   SoShapeKit *sun = new SoShapeKit();
   sun->setPart("shape", new SoSphere);
   sun->set("shape { radius 4 }");
   sun->set("material { diffuseColor 1 1 0 }");
   myScene->setPart("childList[0]", sun);


   SoShapeKit *ray1 = new SoShapeKit;
   ray1->setPart("shape", new SoCylinder);
   ray1->set("shape { radius .005 height 8}");
   xf1->rotation = r.setValue(zAxis, angle[2]);
   xf1->translation = trans1.setValue(4.0, 0, 0);
   ray1->setPart("transform", xf1);
   sun->setPart("childList[0]", ray1);
   // bound
   SoShapeKit *planet1 = new SoShapeKit;
   planet1->setPart("shape", new SoSphere);
   planet1->set("shape { radius 1 }");
   planet1->set("material { diffuseColor 1 0 1 }");
   planet1->set("transform { translation 0 4 0 }");
   ray1->setPart("childList[0]", planet1);


   SoShapeKit *ray2 = new SoShapeKit;
   ray2->setPart("shape", new SoCylinder);
   ray2->set("shape { radius .005 height 16}");
   xf2->rotation = r.setValue(xAxis, angle[0]);
   xf2->translation = trans2.setValue(0, 0, 8);
   ray2->setPart("transform", xf2);
   sun->setPart("childList[1]", ray2);
   // bound
   SoShapeKit *planet2 = new SoShapeKit;
   planet2->setPart("shape", new SoSphere);
   planet2->set("shape { radius 1.7 }");
   planet2->set("material { diffuseColor 0 1 1 }");
   planet2->set("transform { translation 0 8 0 }");
   ray2->setPart("childList[0]", planet2);

   SoGroup *builtMoon = buildMoon(6, 2);
   myScene->setPart("childList[1]", builtMoon);
   
   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   SoPerspectiveCamera *myCamera = SO_GET_PART(myScene,
         "cameraList[0].camera", SoPerspectiveCamera);
   myCamera->viewAll(myScene, myRenderArea->getSize());

   myRenderArea->setSceneGraph(myScene);
   myRenderArea->setTitle("Solar Kit");

   myTicker = new SoTimerSensor(tickerCallback, myCamera);
   myTicker->setInterval(UPDATE_RATE);
   myTicker1 = new SoTimerSensor(tickerCallback1, myCamera);
   myTicker1->setInterval(UPDATE_RATE);
   myTicker2 = new SoTimerSensor(tickerCallback2, myCamera);
   myTicker2->setInterval(UPDATE_RATE);

   myRenderArea->setEventCallback(
               myAppEventHandler, myRenderArea);

   myRenderArea->show();
   SoXt::show(myWindow);
   SoXt::mainLoop();
}
