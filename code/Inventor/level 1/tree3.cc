#include <Inventor/Xt/SoXt.h>
// #include <Inventor/Xt/SoXtRenderArea.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoLinearProfile.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoRotationXYZ.h>


main(int argc, char **argv)
{

  Widget myWindow = SoXt::init(argv[0]);
  if(myWindow == NULL) exit(1);
  
// Construct root node
SoGroup *root = new SoGroup;
root->ref();

// Set up camera.
SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
//myCamera->position.setValue(0, -(argc - 1) / 2, 10);
//myCamera->nearDistance.setValue(5.0);
//myCamera->farDistance.setValue(15.0);
root->addChild(myCamera);
root->addChild(new SoDirectionalLight);

// Construct all parts
SoGroup *waterMolecule = new SoGroup;   // water molecule

SoGroup *oxygen = new SoGroup;          // oxygen atom
//SoTransform *trunkXform1 = new SoTransform;
SoMaterial *redPlastic = new SoMaterial;
SoCylinder *cyl1 = new SoCylinder;
cyl1->radius = 0.8;
cyl1->height = 4.0;


SoGroup *hydrogen1 = new SoGroup;       // hydrogen atoms
SoGroup *hydrogen2 = new SoGroup;
SoTransform *hydroXform1 = new SoTransform;
SoRotationXYZ *branchRotation = new SoRotationXYZ;
branchRotation->axis = SoRotationXYZ::Z;  // rotate about Z axis
branchRotation->angle = 2.4;
SoTransform *hydroXform2 = new SoTransform;
SoMaterial *whitePlastic = new SoMaterial;
SoCylinder *cyl2 = new SoCylinder;
cyl2->radius = 1.0;
cyl2->height = 5.5;
SoCylinder *cyl3 = new SoCylinder;
cyl3->radius = 1.0;
cyl3->height = 5.0;

// Set all field values for the oxygen atom
//trunkXform1->translation.setValue(-6.0, 0.0, 0.0);
redPlastic->ambientColor.setValue(0.90, 0.323, 0.0);
redPlastic->diffuseColor.setValue(0.90, 0.323, 0.0);
redPlastic->specularColor.setValue(0.3, 0.3, 0.3);
redPlastic->shininess = 0.5;

// Set all field values for the hydrogen atorms
hydroXform1->scaleFactor.setValue(0.75, 0.75, 0.75);
hydroXform1->translation.setValue(0.0, 4.0, 0.0);
hydroXform2->translation.setValue(1.8, 0.0, 0.0);
whitePlastic->ambientColor.setValue(1.0, 0.420, 0.0);
whitePlastic->diffuseColor.setValue(1.0, 0.420, 0.0);
whitePlastic->specularColor.setValue(0.5, 0.5, 0.5);
whitePlastic->shininess = 0.4;

// Create a hierarchy
waterMolecule->addChild(oxygen);
waterMolecule->addChild(hydrogen1);
waterMolecule->addChild(hydrogen2);

// Set fields to associated group
//oxygen->addChild(trunkXform1);
oxygen->addChild(redPlastic);
oxygen->addChild(cyl1);
hydrogen1->addChild(hydroXform1);
hydrogen1->addChild(whitePlastic);
hydrogen1->addChild(cyl2);
hydrogen2->addChild(hydroXform2);
hydrogen2->addChild(branchRotation);
hydrogen2->addChild(cyl3);

// plant
root->addChild(waterMolecule);

// render scene in examiner view
SoXtExaminerViewer *myViewer = new SoXtExaminerViewer(myWindow);
myViewer->setSceneGraph(root);
myViewer->setTitle("tree");
myViewer->show();
myViewer->viewAll();


// mainloop
SoXt::show(myWindow);
SoXt::mainLoop();

}
