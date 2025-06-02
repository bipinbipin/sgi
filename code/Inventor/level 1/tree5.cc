#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoRotationXYZ.h>


void main(int argc, char **argv)
{

  // Initialize window and root node, 
  // and add camera and light to root
  Widget myWindow = SoXt::init(argv[0]);
  SoGroup *root = new SoGroup;
  root->ref();
  SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
  root->addChild(myCamera);
  root->addChild(new SoDirectionalLight);


  // tree with "Y" branches

  // materials
  SoMaterial *wood = new SoMaterial;
  wood->ambientColor = SbVec3f(.90, .323, 0.0);
  wood->diffuseColor = SbVec3f(.90, .323, 0.0);
  wood->specularColor = SbVec3f(.3, .3, .3);
  wood->shininess = 1.0;


  // Construct branch sholder and arm + rightHand and leftHand
  SoCylinder *sholder = new SoCylinder;
  sholder->radius = 1.9;
  sholder->height = 6;
  
  SoTransform *armXform = new SoTransform;
  armXform->translation.setValue(0.0, 6.0, 0.0);
  SoRotationXYZ *armRot = new SoRotationXYZ;
  armRot->axis = SoRotationXYZ::Y;
  armRot->angle = 0.5;

  SoCylinder *arm = new SoCylinder;
  arm->radius = 1.6;
  arm->height = 6.0;

  // transform right hand
  SoTransform *rthandXform = new SoTransform;
  rthandXform->translation = SbVec3f(1.0, 4.0, 0.0);
  SoRotationXYZ *rightHandrot = new SoRotationXYZ;
  rightHandrot->axis = SoRotationXYZ::Z;
  rightHandrot->angle = -0.785394;

  // transform left hand
  SoTransform *lfhandXform = new SoTransform;
  lfhandXform->translation = SbVec3f(-1.0, -1.0, 0.0);
  SoRotationXYZ *leftHandrot = new SoRotationXYZ;
  leftHandrot->axis = SoRotationXYZ::Z;
  leftHandrot->angle = 1.570788;


  


  // Put branch parts together (sholder and arm) + rightHand and leftHand
  SoGroup *branch = new SoGroup;
  branch->addChild(sholder);
  branch->addChild(armXform);
  branch->addChild(armRot);
  branch->addChild(arm);
  //branch->addChild(rthandXform);
  branch->addChild(rightHandrot);
  branch->addChild(arm);
  //branch->addChild(lfhandXform);
  branch->addChild(leftHandrot);
  branch->addChild(arm);

  // construct left branch
  SoTransform *leftTransform = new SoTransform;
  leftTransform->translation = SbVec3f(1.8, 7, 0);

  SoRotationXYZ *leftRotation = new SoRotationXYZ;
  leftRotation->axis = SoRotationXYZ::Z;
  leftRotation->angle = -0.785394;

  SoSeparator *leftBranch = new SoSeparator;
  leftBranch->addChild(leftTransform);
  leftBranch->addChild(leftRotation);
  leftBranch->addChild(branch);


  // construct right branch
  SoTransform *rightTransform = new SoTransform;
  rightTransform->translation = SbVec3f(-1.8, 7, 0);

  SoRotationXYZ *rightRotation = new SoRotationXYZ;
  rightRotation->axis = SoRotationXYZ::Z;
  rightRotation->angle = 0.785394;

  SoSeparator *rightBranch = new SoSeparator;
  rightBranch->addChild(rightTransform);
  rightBranch->addChild(rightRotation);
  rightBranch->addChild(branch);


  // Construct Parts for Body
  SoTransform *trunkTransform = new SoTransform;
  trunkTransform->translation = SbVec3f(0.0, 0.0, 0.0);

  SoCylinder *trunk = new SoCylinder;
  trunk->radius = 2;
  trunk->height = 12;


  // Attach Legs to body
  SoSeparator *body = new SoSeparator;
  body->addChild(trunkTransform);
  body->addChild(wood);
  body->addChild(trunk);
  body->addChild(leftBranch);
  body->addChild(rightBranch);


  // tree is just body
  SoSeparator *tree = new SoSeparator;
  tree->addChild(body);

  // plant robot in root node and initialize view
  root->addChild(tree);
  SoXtExaminerViewer *myViewer = new SoXtExaminerViewer(myWindow);
  myViewer->setSceneGraph(root);
  myViewer->setTitle("Tree 5");
  myViewer->show();
  myViewer->viewAll();

  // mainloop
  SoXt::show(myWindow);
  SoXt::mainLoop();

}


