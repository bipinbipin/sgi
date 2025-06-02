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

SbBool
myAppEventHandler(void *userData, XAnyEvent *anyevent)
{
//   SoXtRenderArea *myRenderArea = (SoXtRenderArea *) userData;
   XButtonEvent *myButtonEvent;
//   XMotionEvent *myMotionEvent;
   SbVec3f vec;
   SbBool handled = TRUE;

   switch (anyevent->type) {
   
   case ButtonPress:
      myButtonEvent = (XButtonEvent *) anyevent;
      if (myButtonEvent->button == Button1) {
         myTicker->schedule();  
      } else if (myButtonEvent->button == Button2) {
         myTicker->schedule();  
      } else if (myButtonEvent->button == Button3) {
         myTicker->schedule();  
      }
      break;
      
   case ButtonRelease:
      myButtonEvent = (XButtonEvent *) anyevent;
      if (myButtonEvent->button == Button1) {
         myTicker->unschedule(); 
      } else if (myButtonEvent->button == Button2) {
         myTicker->unschedule();  
      } else if (myButtonEvent->button == Button3) {
         myTicker->unschedule();  
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
   // base
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
   ray1->set("shape { radius .075 height 8}");
   xf1->rotation = r.setValue(zAxis, angle[2]);
   xf1->translation = trans1.setValue(4.0, 0, 0);
   ray1->setPart("transform", xf1);
   sun->setPart("childList[0]", ray1);
   // bound
   SoShapeKit *planet1 = new SoShapeKit;
   planet1->setPart("shape", new SoSphere);
   planet1->set("shape { radius 0.6 }");
   planet1->set("material { diffuseColor 1 0 1 }");
   planet1->set("transform { translation 0 4 0 }");
   ray1->setPart("childList[0]", planet1);


   SoShapeKit *ray2 = new SoShapeKit;
   ray2->setPart("shape", new SoCylinder);
   ray2->set("shape { radius .075 height 16}");
   xf2->rotation = r.setValue(xAxis, angle[0]);
   xf2->translation = trans2.setValue(0, 0, 8);
   ray2->setPart("transform", xf2);
   sun->setPart("childList[1]", ray2);
   // bound
   SoShapeKit *planet2 = new SoShapeKit;
   planet2->setPart("shape", new SoSphere);
   planet2->set("shape { radius 1 }");
   planet2->set("material { diffuseColor 0 1 1 }");
   planet2->set("transform { translation 0 8 0 }");
   ray2->setPart("childList[0]", planet2);



   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   SoPerspectiveCamera *myCamera = SO_GET_PART(myScene,
      "cameraList[0].camera", SoPerspectiveCamera);
   myCamera->viewAll(myScene, myRenderArea->getSize());

   myRenderArea->setSceneGraph(myScene);
   myRenderArea->setTitle("Solar Kit");
   // Timer sensor to tick off time while middle mouse is down
   myTicker = new SoTimerSensor(tickerCallback, myCamera);
   myTicker->setInterval(UPDATE_RATE);

   myRenderArea->setEventCallback(
               myAppEventHandler, myRenderArea);

   myRenderArea->show();
   SoXt::show(myWindow);
   SoXt::mainLoop();
}
