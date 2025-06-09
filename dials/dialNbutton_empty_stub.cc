// CC -g -n32 -I/usr/include/Inventor -I/usr/include/X11 -c dialNbutton_event-test.cc && CC dialNbutton_event-test.o -L/usr/lib32 -lInventorInput -lInventorXt -lInventor -lX11 -lGL -lGLU -lm -o dialNbutton_event-test

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

// Had to compile each of these and link /usr/lib32/libInventorInput.so
#include <Inventor/Input/ButtonBoxEvent.h>
#include <Inventor/Input/DialEvent.h>
#include <Inventor/Input/DialNButton.h>

SoXtExaminerViewer *globalViewer = NULL;


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

void dialCB(void *userData, SoEventCallback *cb)
{
    const DialEvent *ev = (const DialEvent *) cb->getEvent();
    int dialIndex = ev->getDial();
    if (dialIndex < 0 || dialIndex >= 8) return;

    int rawValue = ev->getValue();

    switch (dialIndex) {
         case 0:
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

         case 6: {
            break;
         }
         case 7: {
            break;
         }

    }
}

SoNode *
buildSceneGraph()
{
   SoGroup *sep = new SoGroup;
   
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
   globalViewer = vwr;
   vwr->setSceneGraph(buildSceneGraph());
   vwr->setTitle("Dial Open Inventor Demo");
   vwr->setViewing(FALSE);   // come up in pick mode
   vwr->setHeadlight(TRUE);  // we supply our own light

   // window settings
   vwr->setSize(SbVec2s(1280, 1024));
   vwr->setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
   vwr->setDecoration(FALSE);

   DialNButton *db = new DialNButton;
   vwr->registerDevice(db);
   
   vwr->show();
   SoXt::show(mainWindow);
   SoXt::mainLoop();
}
