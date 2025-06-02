#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSphere.h>

#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoRotationXYZ.h>

#define DIM 8

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
k=0;
SoSphere *dskblk = new SoSphere;
dskblk->radius = 0.5;

SoTransform *x = new SoTransform;
x->translation.setValue(1.5, 0, 0);
SoTransform *xReset = new SoTransform;
xReset->translation.setValue(-dim, 0, 0);
//xReset->scaleFactor.setValue((1/sr),(1/sr),(1/sr));

//SoTransform *swell = new SoTransform;
//swell->scaleFactor.setValue(1.5, 1.5, 1.5);


  SoRotationXYZ *xRot = new SoRotationXYZ;
  xRot->axis = SoRotationXYZ::Y;
  xRot->angle = 0.23;

SoTransform *y = new SoTransform;
y->translation.setValue(0, 1, 0);
SoTransform *yReset = new SoTransform;
yReset->translation.setValue(0, -dim, 0);
SoTransform *z = new SoTransform;
z->translation.setValue(0, 0, 1);

SoMaterial *gold   = new SoMaterial;
//Set material values
gold->ambientColor.setValue(.3, .1, .1);
gold->diffuseColor.setValue(.8, k, .2);
gold->specularColor.setValue(.4, .3, .1);
gold->shininess = .4;

SoSeparator *mark = new SoSeparator; 




//for (k=1; k < dim; k++)
//{
	for (j=0; j < dim*dim; j++)
	{
		for(i=0; i < dim;i++)
		{ 
			gold->ambientColor.setValue(.3, .1, 0.1+(j/10));
			mark->addChild(gold);
			mark->addChild(dskblk);
			mark->addChild(x);
			//root->addChild(swell);
		}
		root->addChild(mark);
		//gold->ambientColor.setValue(.3, .1, 0.1+(j/10));
		gold->diffuseColor.setValue(.8, (0.7-(j/10)), .2);
		//gold->specularColor.setValue(.4, .3, .1);
		//gold->shininess = .4;
		root->addChild(xReset);
		root->addChild(xRot);
		root->addChild(y);
	}
	//gold->diffuseColor.setValue(.8, (k/10), .2);
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
