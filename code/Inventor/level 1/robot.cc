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

void main(int argc, char **argv)
{

  Widget myWindow = SoXt::init(argv[0]);

  // Construct root node and setup camera
  SoGroup *root = new SoGroup;
  root->ref();
  SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
  root->addChild(myCamera);
  root->addChild(new SoDirectionalLight);


  // robot with legs

  // Construct Leg parts (thigh, calf, foot)
  SoCube *thigh = new SoCube;
  thigh->width = 1.2;
  thigh->height = 2.2;
  thigh->depth = 1.1;
  
  SoTransform *calfTransform = new SoTransform;
  calfTransform->translation.setValue(0, -2.25, 0.0);

  SoCube *calf = new SoCube;
  calf->width = 1;
  calf->height = 2.2;
  calf->depth = 1;

  SoTransform *footTransform = new SoTransform;
  footTransform->translation.setValue(0, -1.5, .5);

  SoCube *foot = new SoCube;
  foot->width = 0.8;
  foot->height = 0.8;
  foot->depth = 2;


  // Put leg parts together
  SoGroup *leg = new SoGroup;
  leg->addChild(thigh);
  leg->addChild(calfTransform);
  leg->addChild(calf);
  leg->addChild(footTransform);
  leg->addChild(foot);

  SoTransform *leftTransform = new SoTransform;
  leftTransform->translation = SbVec3f(1, -4.25, 0);

  // Left Leg
  SoSeparator *leftLeg = new SoSeparator;
  leftLeg->addChild(leftTransform);
  leftLeg->addChild(leg);

  SoTransform *rightTransform = new SoTransform;
  rightTransform->translation = SbVec3f(-1, -4.25, 0);

  // Right Leg
  SoSeparator *rightLeg = new SoSeparator;
  rightLeg->addChild(rightTransform);
  rightLeg->addChild(leg);


  // Construct Parts for Body
  SoTransform *bodyTransform = new SoTransform;
  bodyTransform->translation = SbVec3f(0.0, 3.0, 0.0);

  SoMaterial *bronze = new SoMaterial;
  bronze->ambientColor = SbVec3f(.33, .22, .27);
  bronze->diffuseColor = SbVec3f(.78, .57, .11);
  bronze->specularColor = SbVec3f(.99, .94, .81);
  bronze->shininess = .38;

  SoCube *bodyCube = new SoCube;
  bodyCube->width = 4;
  bodyCube->height = 6;
  bodyCube->depth = 3;


  // Attach Legs to body
  SoSeparator *body = new SoSeparator;
  body->addChild(bodyTransform);
  body->addChild(bronze);
  body->addChild(bodyCube);
  body->addChild(leftLeg);
  body->addChild(rightLeg);


  // Construct Head parts
  SoTransform *headTransform = new SoTransform;
  headTransform->translation = SbVec3f(0, 7.5, 0);
  headTransform->scaleFactor = SbVec3f(1.5, 1.5, 1.5);

  SoMaterial *silver = new SoMaterial;
  silver->ambientColor = SbVec3f(.2, .2, .2);
  silver->diffuseColor = SbVec3f(.6, .6, .6);
  silver->specularColor = SbVec3f(.5, .5, .5);
  silver->shininess = .5;


  // constuct head
  SoSphere *headSphere = new SoSphere;
  SoSeparator *head = new SoSeparator;
  head->addChild(headTransform);
  head->addChild(silver);
  head->addChild(headSphere);


  // Robot is just head and body
  SoSeparator *robot = new SoSeparator;
  robot->addChild(body);
  robot->addChild(head);

  // plant robot in root node and initialize view
  root->addChild(robot);
  SoXtExaminerViewer *myViewer = new SoXtExaminerViewer(myWindow);
  myViewer->setSceneGraph(root);
  myViewer->setTitle("robot");
  myViewer->show();
  myViewer->viewAll();

  // mainloop
  SoXt::show(myWindow);
  SoXt::mainLoop();

}


