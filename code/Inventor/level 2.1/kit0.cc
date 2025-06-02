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

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>

#include <Inventor/actions/SoWriteAction.h>

#include <math.h>
void
main(int , char **argv)
{
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   SbRotation r;
   SbVec3f xAxis(1, 0, 0), yAxis(0, 1, 0), zAxis(0, 0, 1);
   SbVec3f trans;
   //float x, y, z;
   float angle[3];
    angle[0] = (M_PI/2); angle[1] = M_PI; angle[2] = (3*M_PI/2);  
   SoTransform *xf = new SoTransform;
   

   SoSceneKit *myScene = new SoSceneKit;
   myScene->ref();
   // base
   myScene->setPart("lightList[0]", new SoLightKit);
   myScene->setPart("cameraList[0]", new SoCameraKit);
   myScene->setCameraNumber(0);


   SoShapeKit *sun = new SoShapeKit();
   sun->setPart("shape", new SoSphere);
   sun->set("shape { radius 2 }");
   myScene->setPart("childList[0]", sun);


   SoShapeKit *ray1 = new SoShapeKit;
   ray1->setPart("shape", new SoCylinder);
   ray1->set("shape { radius .03 height 8}");
   xf->rotation = r.setValue(zAxis, angle[2]);
   xf->translation = trans.setValue(4.0, 0, 0);
   ray1->setPart("transform", xf);
   sun->setPart("childList[0]", ray1);
   // bound
   SoShapeKit *planet1 = new SoShapeKit;
   planet1->setPart("shape", new SoSphere);
   planet1->set("shape { radius 0.4 }");
   planet1->set("transform { translation 0 4 0 } ");
   ray1->setPart("childList[0]", planet1);


   SoShapeKit *ray2 = new SoShapeKit;
   ray2->setPart("shape", new SoCylinder);
   ray2->set("shape { radius .03 height 8}");
//
   sun->setPart("childList[1]", ray2);
   // bound
   SoShapeKit *planet2 = new SoShapeKit;
   planet2->setPart("shape", new SoSphere);
   planet2->set("shape { radius 0.4 }");
   //planet2->set("transform { translation 0 -1 0 } ");
   ray2->setPart("childList[0]", planet2);



   SoXtRenderArea *myRenderArea = new SoRenderArea(myWindow);

   SoPerspectiveCamera *myCamera = SO_GET_PART(myScene,
      "cameraList[0].camera", SoPerspectiveCamera);
   myCamera->viewAll(myScene, myRenderArea->getSize());

   myRenderArea->setSceneGraph(myScene);
   myRenderArea->setTitle("Solar Kit");
   myRenderArea->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}
