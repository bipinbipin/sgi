#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>

#include <Inventor/nodes/SoTranslation.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoRotor.h>
#include <Inventor/nodes/SoDrawStyle.h>

#include <Inventor/engines/SoTimeCounter.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/engines/SoCompose.h>


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
  /////////////////////////////////

// BEGIN RENDER CODE
int i;

SoMaterial *earth = new SoMaterial;
earth->ambientColor = SbVec3f(.2, 0.6, 0.6);
earth->diffuseColor = SbVec3f(.4, .4, .4);
earth->specularColor = SbVec3f(.1, .5, .1);
earth->shininess = 0.5;

SoTransform *yUp = new SoTransform;
yUp->translation.setValue(0,1000,0);


SoCube *land = new SoCube;
land->height = 1;
land->width = 1000000;
land->depth = 10000;

for (i=0; i < 20; i++) {
		root->addChild(earth);
		root->addChild(land);
		root->addChild(yUp);
		}

// END RENDER CODE

  // SET VIEWING WINDOW AND viewAll of root node 
  SoXtExaminerViewer *myViewer =
            new SoXtExaminerViewer(myWindow);
  myViewer->setSceneGraph(root);
  myViewer->setTitle("land");
  myViewer->show();
  myViewer->viewAll();
  

  // NOTHING AFTER THESE 2 LINES
  SoXt::show(myWindow);
  SoXt::mainLoop();
}

