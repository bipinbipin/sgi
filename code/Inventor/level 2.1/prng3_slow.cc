#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoRotor.h>

#define PARTS_PER_LOOP 6

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
int i = 0;
int j = 0;

SoGroup *g = new SoGroup;
SoShapeKit *k;
SoTransform *xf;

SoTransform *zNotch = new SoTransform;
zNotch->translation.setValue(0,0,1);
zNotch->rotation.setValue(SbVec3f(0, 1, 0.5), 45);

SoRotor *myRotor = new SoRotor;
// myRotor->rotation.setValue(SbVec3f(0.5, 1, 0), 0); // z axis
myRotor->speed = 0.001;

for (j = 0; j < (PARTS_PER_LOOP/3); j++) {

   for (i = 1; i < PARTS_PER_LOOP; i++) {
	k = new SoShapeKit;

	k->setPart("shape", new SoSphere);
	xf = (SoTransform *) k->getPart("transform", TRUE);
	xf->translation.setValue(8*sin(i*M_PI/(PARTS_PER_LOOP/2)), 8*cos(i*M_PI/(PARTS_PER_LOOP/2)), 0.0);
	
	g->addChild(k);
	myRotor->rotation.setValue(SbVec3f((i/10), 1, 0), 0); // z axis
	myRotor->speed = (0.0001 * i);
	g->addChild(myRotor);
	}
root->addChild(g);

root->addChild(zNotch);
    }

//root->addChild(g);


/////////////////////
// END RENDER CODE //
/////////////////////
  
  SoXtExaminerViewer *myViewer =
            new SoXtExaminerViewer(myWindow);
  myViewer->setSceneGraph(root);
  myViewer->setTitle("waz");
  myViewer->show();
  myViewer->viewAll();
  

  // NOTHING AFTER THESE 2 LINES
  SoXt::show(myWindow);
  SoXt::mainLoop();
}
