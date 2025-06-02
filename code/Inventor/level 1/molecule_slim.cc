#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
// #include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoLinearProfile.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoSphere.h>


main(int argc, char **argv)
{

  Widget myWindow = SoXt::init(argv[0]);
  if(myWindow == NULL) exit(1);
  
// Construct root node
SoGroup *root = new SoGroup;
root->ref();

// Set up camera.
SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
// myCamera->position.setValue(0, -(argc - 1) / 2, 10);
// myCamera->nearDistance.setValue(5.0);
// myCamera->farDistance.setValue(15.0);
root->addChild(myCamera);
root->addChild(new SoDirectionalLight);

// Construct all parts
SoGroup *waterMolecule = new SoGroup;   // water molecule

SoGroup *oxygen = new SoGroup;          // oxygen atom
SoMaterial *redPlastic = new SoMaterial;
SoSphere *sphere1 = new SoSphere;

SoGroup *hydrogen1 = new SoGroup;       // hydrogen atoms
SoGroup *hydrogen2 = new SoGroup;
SoTransform *hydroXform1 = new SoTransform;
SoTransform *hydroXform2 = new SoTransform;
SoMaterial *whitePlastic = new SoMaterial;
SoSphere *sphere2 = new SoSphere;
SoSphere *sphere3 = new SoSphere;

// Set all field values for the oxygen atom
redPlastic->ambientColor.setValue(1.0, 0.0, 0.0);
redPlastic->diffuseColor.setValue(1.0, 0.0, 0.0);
redPlastic->specularColor.setValue(0.5, 0.5, 0.5);
redPlastic->shininess = 0.5;

// Set all field values for the hydrogen atorms
hydroXform1->scaleFactor.setValue(0.75, 0.75, 0.75);
hydroXform1->translation.setValue(0.0, -1.2, 0.0);
hydroXform2->translation.setValue(1.1852, 1.3877, 0.0);
whitePlastic->ambientColor.setValue(1.0, 1.0, 1.0);
whitePlastic->diffuseColor.setValue(1.0, 1.0, 1.0);
whitePlastic->specularColor.setValue(0.5, 0.5, 0.5);
whitePlastic->shininess = 0.6;


// Create a hierarchy
waterMolecule->addChild(oxygen);
waterMolecule->addChild(hydrogen1);
waterMolecule->addChild(hydrogen2);

// Set fields to associated group
oxygen->addChild(redPlastic);
oxygen->addChild(sphere1);
hydrogen1->addChild(hydroXform1);
hydrogen1->addChild(whitePlastic);
hydrogen1->addChild(sphere2);
hydrogen2->addChild(hydroXform2);
hydrogen2->addChild(sphere3);

// plant
root->addChild(waterMolecule);

// Render scene for viewing
SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
myCamera->viewAll(root, myRenderArea->getViewportRegion());
myRenderArea->setSceneGraph(waterMolecule);
myRenderArea->setTitle("H2O molecule");
myRenderArea->show();

// mainloop
SoXt::show(myWindow);
SoXt::mainLoop();

}
