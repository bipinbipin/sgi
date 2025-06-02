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

   pos = myCamera->position.getValue();
   rot = SbRotation(SbVec3f(0,1,0), ROTATION_ANGLE);
   mtx.setRotate(rot);
   mtx.multVecMatrix(pos, pos);
   myCamera->position.setValue(pos);
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
   
   pos = myCamera->position.getValue();
   rot = SbRotation(SbVec3f(1,0,0), ROTATION_ANGLE);
   mtx.setRotate(rot);
   mtx.multVecMatrix(pos, pos);
   myCamera->position.setValue(pos);  
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
   
   pos = myCamera->position.getValue();
   rot = SbRotation(SbVec3f(0,0,1), ROTATION_ANGLE);
   mtx.setRotate(rot);
   mtx.multVecMatrix(pos, pos);
   myCamera->position.setValue(pos);  
   myCamera->orientation.setValue(
            myCamera->orientation.getValue() * rot);
}

SbBool
myAppEventHandler(void *userData, XAnyEvent *anyevent)
{
   XButtonEvent *myButtonEvent;
   SbVec3f vec;
   SbBool handled = TRUE;

switch (anyevent->type) 
   {
   case ButtonPress:
      myButtonEvent = (XButtonEvent *) anyevent;
      if (myButtonEvent->button == Button1) 
         { myTicker->schedule(); }
      else if (myButtonEvent->button == Button2) 
	 { myTicker1->schedule(); }
      else if (myButtonEvent->button == Button3) 
         { myTicker2->schedule(); }
      break;
      
   case ButtonRelease:
      myButtonEvent = (XButtonEvent *) anyevent;
      if (myButtonEvent->button == Button1) 
         { myTicker->unschedule(); }
      else if (myButtonEvent->button == Button2) 
         { myTicker1->unschedule(); }
      else if (myButtonEvent->button == Button3) 
         { myTicker2->unschedule(); }
      break;

   default:
      handled = FALSE;
      break;
   }
return handled;
}

void
moveKit( void *userData, SoEventCallback *eventCB)
{
   const SoEvent *ev = eventCB->getEvent();

   // If Right or Left Arrow key, then continue...
   if (SO_KEY_PRESS_EVENT(ev, RIGHT_ARROW) ||
       SO_KEY_PRESS_EVENT(ev, LEFT_ARROW) ||
       SO_KEY_PRESS_EVENT(ev, UP_ARROW) ||
       SO_KEY_PRESS_EVENT(ev, DOWN_ARROW)) {
      SoShapeKit  *sun;
      SbVec3f  startRot, rayIncrement;      

      sun = (SoShapeKit *) userData;

//     ray1 = (SoShapeKit *)  sun->getPart("childList[0]",TRUE);
//     ray2 = (SoShapeKit *)  sun->getPart("childList[1]",TRUE);

      if (SO_KEY_PRESS_EVENT(ev, RIGHT_ARROW)) {
         rayIncrement.setValue(0, 1, 0);

      }
      else if (SO_KEY_PRESS_EVENT(ev, LEFT_ARROW)){
         rayIncrement.setValue(0, -1, 0);

      }
//      else if (SO_KEY_PRESS_EVENT(ev, UP_ARROW)){
         
//      }
//      else if (SO_KEY_PRESS_EVENT(ev, DOWN_ARROW)){
//         rayIncrement.setValue(SbVec3f(1, 0, 0), .1);
//         stringIncrement.setValue(SbVec3f(1, 0, 0), -.1);
//      }

      SoTransform *xf;
      xf = SO_GET_PART(sun, "transform", SoTransform);
      startRot = xf->translation.getValue();
      xf->translation.setValue(startRot +  rayIncrement);

//      xf = SO_GET_PART(ray1, "transform", SoTransform);
//      startRot = xf->rotation.getValue();
//      xf->rotation.setValue(startRot *  stringIncrement);

//      xf = SO_GET_PART(ray2, "transform", SoTransform);
//      startRot = xf->rotation.getValue();
//      xf->rotation.setValue(startRot *  stringIncrement);

      eventCB->setHandled();
   }
}
void
main(int , char **argv)
{
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   SoSceneKit *myScene = new SoSceneKit;
   myScene->ref();
   myScene->setPart("lightList[0]", new SoLightKit);
   myScene->setPart("cameraList[0]", new SoCameraKit);
   myScene->setCameraNumber(0);

///////////////////////
// begin render code //
///////////////////////



/////////////////////
// end render code //
/////////////////////

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   SoPerspectiveCamera *myCamera = SO_GET_PART(myScene,
         "cameraList[0].camera", SoPerspectiveCamera);
   myCamera->viewAll(myScene, myRenderArea->getSize());

   myRenderArea->setSceneGraph(myScene);
   myRenderArea->setTitle("callback kit");

   // Add EventCallback so Balance Responds to Events
   SoEventCallback *myCallbackNode = new SoEventCallback;
   myCallbackNode->addEventCallback(
        SoKeyboardEvent::getClassTypeId(),
             moveKit, sun);
   sun->setPart("callbackList[0]", myCallbackNode);

   // EventCallback that rotates camera on mouse click
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
