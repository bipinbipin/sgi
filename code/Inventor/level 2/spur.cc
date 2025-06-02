#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSphere.h>

#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoRotationXYZ.h>

#define DIM 11

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
  int i,j,k;
  double dim, sr;

dim = DIM;
for(k=0,sr=1.5;k<=dim;k++)
{sr= sr*1.5; }

SoSphere *dskblk = new SoSphere;
dskblk->radius = 0.5;

SoTransform *x = new SoTransform;
x->translation.setValue(1.5, 0, 0);
SoTransform *xReset = new SoTransform;
xReset->translation.setValue(-sr, 0, 0);
xReset->scaleFactor.setValue((1/sr),(1/sr),(1/sr));

SoTransform *swell = new SoTransform;
swell->scaleFactor.setValue(1.5, 1.5, 1.5);


  SoRotationXYZ *xRot = new SoRotationXYZ;
  xRot->axis = SoRotationXYZ::Y;
  xRot->angle = 0.37;

SoTransform *y = new SoTransform;
y->translation.setValue(0, 1, 0);
SoTransform *yReset = new SoTransform;
yReset->translation.setValue(0, -dim, 0);
SoTransform *z = new SoTransform;
z->translation.setValue(0, 0, 1);

//for (k=0; k < dim; k++)
//{
	for (j=0; j < dim*3; j++)
	{
		for(i=0; i < dim;i++)
		{ 
			root->addChild(dskblk);
			root->addChild(x);
			root->addChild(swell);
		}
		root->addChild(xReset);
		root->addChild(xRot);
		root->addChild(y);
	}
	//root->addChild(yReset);
	//root->addChild(z);
//}

  // SET VIEWING WINDOW AND viewAll of root node 
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
