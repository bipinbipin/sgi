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

#define PARTS_PER_LOOP 12

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
SoGroup *g = new SoGroup;
SoShapeKit *k;
SoTransform *xf;

for (int i = 0; i < PARTS_PER_LOOP; i++) {
	k = new SoShapeKit;

	k->setPart("shape", new SoCube);
	xf = (SoTransform *) k->getPart("transform", TRUE);
	xf->translation.setValue(8*sin(i*M_PI/6), 8*cos(i*M_PI/6), 0.0);
	
	g->addChild(k);
	}

root->addChild(g);

//root->addChild(g);


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
