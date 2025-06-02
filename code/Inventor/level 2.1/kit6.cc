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
       SO_KEY_PRESS_EVENT(ev, DOWN_ARROW) || 
       SO_KEY_PRESS_EVENT(ev, HOME) || 
       SO_KEY_PRESS_EVENT(ev, END)) {

      SoShapeKit  *grnd;
      SbVec3f  startRot, rayIncrement;      
      grnd = (SoShapeKit *) userData;

      if (SO_KEY_PRESS_EVENT(ev, RIGHT_ARROW)) 
         { rayIncrement.setValue(100, 0, 0); }
      else if (SO_KEY_PRESS_EVENT(ev, LEFT_ARROW))
         { rayIncrement.setValue(-100, 0, 0); }
      else if (SO_KEY_PRESS_EVENT(ev, UP_ARROW))
	 { rayIncrement.setValue(0, 100, 0);  }
      else if (SO_KEY_PRESS_EVENT(ev, DOWN_ARROW))
	 { rayIncrement.setValue(0, -100, 0); }
      else if (SO_KEY_PRESS_EVENT(ev, HOME))
	 { rayIncrement.setValue(0, 0, 100);  }
      else if (SO_KEY_PRESS_EVENT(ev, END))
	 { rayIncrement.setValue(0, 0, -100); }

      SoTransform *xf;
      xf = SO_GET_PART(grnd, "transform", SoTransform);
      startRot = xf->translation.getValue();
      xf->translation.setValue(startRot +  rayIncrement);

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

   SoShapeKit *ground = new SoShapeKit();
   ground->set("shape { width 10000 height 1 depth 10000 }");
   ground->set("material { diffuseColor 1 1 0 }");
   myScene->setPart("childList[0]", ground);


/////////////////////
// end render code //
/////////////////////

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   SoPerspectiveCamera *myCamera = SO_GET_PART(myScene,
         "cameraList[0].camera", SoPerspectiveCamera);
   myCamera->viewAll(myScene, myRenderArea->getSize());

   myRenderArea->setSceneGraph(myScene);
   myRenderArea->setTitle("ground kit");

   // Add EventCallback so Balance Responds to Events
   SoEventCallback *myCallbackNode = new SoEventCallback;
   myCallbackNode->addEventCallback(
        SoKeyboardEvent::getClassTypeId(),
             moveKit, ground);
   ground->setPart("callbackList[0]", myCallbackNode);

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
