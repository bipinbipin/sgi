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
#include <Inventor/nodes/SoRotor.h>

#define DIM 4

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
sr = 0.1;

SoSphere *dskblk = new SoSphere;
dskblk->radius = 0.5;

SoTransform *x = new SoTransform;
x->translation.setValue(1.5, 0, 0);
SoTransform *xReset = new SoTransform;
xReset->translation.setValue(-dim, 0, 0);
//xReset->scaleFactor.setValue((1/sr),(1/sr),(1/sr));

SoTransform *swell = new SoTransform;
swell->scaleFactor.setValue(1.5, 1.5, 1.4);

//SoRotor *myRotor = new SoRotor;
 //  myRotor->rotation.setValue(SbVec3f(0, 0, 1), 0); // z axis
 //  myRotor->speed = 0.1;
 //  root->addChild(myRotor);
//SoRotor *myRotor2 = new SoRotor;
 //  myRotor2->rotation.setValue(SbVec3f(0, 1, 0), 0); // y axis
 //  myRotor2->speed = 0.5;
 //  root->addChild(myRotor2);

  SoRotationXYZ *xRot = new SoRotationXYZ;
  xRot->axis = SoRotationXYZ::Y;
  xRot->angle = 0.5;

SoTransform *y = new SoTransform;
y->translation.setValue(0, 1, 0);
SoTransform *yReset = new SoTransform;
yReset->translation.setValue(0, -dim, 0);
SoTransform *z = new SoTransform;
z->translation.setValue(0, 0, 1);


SoSeparator *mark = new SoSeparator; 

SoMaterial *gold   = new SoMaterial;
SoMaterial *newgold   = new SoMaterial;

//for (k=1; k < dim; k++)
//{
	for (j=1; j < dim; j++)
	{
		sr = 0.1;
		for(i=1; i < dim;i++)
		{ 
			
			//Set material values
			gold->ambientColor.setValue(.3, 1, .1);
				//gold->diffuseColor.setValue(.8, i/10, .2);
			
			sr = sr + 0.01;
			printf("%f\n",sr);	
			gold->diffuseColor.setValue(.8, sr, .2);							//gold->specularColor.setValue(.4, .3, .1);
				//gold->shininess = .4;
SoRotor *myRotor = new SoRotor;
   myRotor->rotation.setValue(SbVec3f(0.5, 1, 0), 0); // z axis
   myRotor->speed = 0.2;
   mark->addChild(myRotor);

			mark->addChild(gold);
			mark->addChild(dskblk);
			mark->addChild(x);
			//mark->addChild(swell);
		}
		mark->addChild(gold);
		gold->diffuseColor.setValue(1, .8, .2);
		root->addChild(mark);
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
