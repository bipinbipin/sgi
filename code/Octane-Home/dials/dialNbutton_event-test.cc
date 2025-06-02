#include <X11/Intrinsic.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodekits/SoSceneKit.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>

#include <Input/ButtonBoxEvent.h>
#include <Input/DialEvent.h>
#include <Input/DialNButton.h>

int dialVals[8] = {0, 0, 0, 0, 0, 0, 0, 0};

void
buttonBoxCB(void *userData, SoEventCallback *cb)
{
   const ButtonBoxEvent *ev = 
      (const ButtonBoxEvent *) cb->getEvent();
	
   if (ev->getState() == SoButtonEvent::DOWN) {
      int which = ev->getButton();
      printf("button %d pressed\n", which);
   }
}

// Convert a dial value into the range [0.0,1.0]
float
getDialFloat(int d)
{
   float v;
   
   // Clamp the dial value to [-128,128].
   //if (d <= -128)
   //   v = 0.0;
   //else if (d >= 128)
   //   v = 1.0;
   //else
   //   v = (d + 128)/256.0;
   //
   v = d;

   return v;
}

// Convert a dial value into the range [-1.0,1.0]
float
getDialFloat2(int d)
{
   float v;
   
   // Clamp the dial value to [-128,128]
   if (d <= -128)
      v = -1.0;
   else if (d >= 128)
      v = 1.0;
   else
      v = d/128.0;
   
   return v;
}

void
dialCB(void *userData, SoEventCallback *cb)
{
   const DialEvent *ev = 
      (const DialEvent *) cb->getEvent();

   SbVec3f Xaxis(0,1,0);   
   SoTransform *turn = (SoTransform *) userData;
   SoTransform *init = new SoTransform;

   float x, y, z, r, g, b, f;
   switch (ev->getDial()) {
      case 0:
	init->translation.setValue(0, -.1, .5);
  	f = (getDialFloat(ev->getValue()) / 100);
	printf("dial 1 = %f\n", f); 
	turn->rotation.setValue(Xaxis,f);
         break;

      case 1:
         break;

      case 2:
         break;

      case 3:
         break;

      case 4:
         break;

      case 5:
         break;

      case 6:
         break;

      case 7:
	f = (getDialFloat(ev->getValue()) / 10);
	printf("dial 8 = %f\n", f); 
	turn->rotation.setValue(Xaxis,f);
         break;
 
     default: 
	 break;
   }
}   

SoNode *
buildSceneGraph()
{
   int x;
   SbVec3f Xaxis(1,0,0);
   SbVec3f Yaxis(0,1,0);
   SbVec3f Zaxis(0,0,1);

   SoGroup *sep = new SoGroup;
   SoSceneKit *myScene = new SoSceneKit;
   myScene->ref();
   SoShapeKit *potentiometer;
   SoShapeKit *pointer;
   SoEventCallback *cb = new SoEventCallback;

   SoCylinder *cylinder = new SoCylinder;
    cylinder->height = 0.2;
   SoCube *indicator = new SoCube;
    indicator->width = 0.2;
    indicator->height = 0.1;
    indicator->depth = 1;

   SoTransform *xform = new SoTransform;
   SoTransform *xform2 = new SoTransform;
   SoTransform *position = new SoTransform;
   SoTransform *position2 = new SoTransform;
   SoTransform *position3 = new SoTransform;
   SoTransform *position4 = new SoTransform;
   SoTransform *rotation = new SoTransform;

   xform->rotation.setValue(Xaxis, 1.6);
   position->translation.setValue(2,0,0);
   position2->translation.setValue(-8,0,-2);
   position3->translation.setValue(0,0.1,-0.5);
   position4->translation.setValue(0,-0.1,0.5);


   sep->addChild(cb);
   sep->addChild(xform);
   xform2->rotation.setValue(Yaxis, -1.57);
   sep->addChild(xform2);

for (x = 0; x < 8; x++)
{
   potentiometer = new SoShapeKit;
   potentiometer->setPart("shape", new SoCylinder);
   potentiometer->set("shape { height 0.2 }");
   potentiometer->set("material { diffuseColor 0.6 0.6 0.6 }");
   rotation = (SoTransform *) potentiometer->getPart("transform", TRUE);
//potentiometer->set("transform { translation  0 .1 -.5 }");

   sep->addChild(potentiometer);
   sep->addChild(position3);

   pointer = new SoShapeKit;
   pointer->setPart("shape", new SoCube);
   pointer->set("shape { width .2  height .1  depth 1 }");
   pointer->set("material { diffuseColor 0.78 0.57 0.11 }");
   //pointer->set("transform { translation 0 .1 1 }");
   //rotation = (SoTransform *) pointer->getPart("transform", TRUE);

   sep->addChild(pointer);
   sep->addChild(position4);

   if (x == 3)
      { sep->addChild(position2); }

   sep->addChild(position);
}


   // Set up event callbacks
   cb->addEventCallback(
      ButtonBoxEvent::getClassTypeId(),
      buttonBoxCB, 
      xform);
   cb->addEventCallback(
      DialEvent::getClassTypeId(),
      dialCB, 
      rotation);
   
   return sep;
}



void
main(int , char *argv[])
{
   Widget mainWindow = SoXt::init(argv[0]);
   
   if (! DialNButton::exists()) {
       fprintf(stderr, "Sorry, no dial and button box on this display!\n");
       return;
   }
   
   ButtonBoxEvent::initClass();
   DialEvent::initClass();
   
   SoXtExaminerViewer *vwr = new SoXtExaminerViewer(mainWindow);
   vwr->setSceneGraph(buildSceneGraph());
   vwr->setTitle("Dial And Button Device");
   vwr->setViewing(FALSE);   // come up in pick mode
   vwr->setHeadlight(TRUE); // we supply our own light

   DialNButton *db = new DialNButton;
   vwr->registerDevice(db);
   
   vwr->show();
   SoXt::show(mainWindow);
   SoXt::mainLoop();
}
