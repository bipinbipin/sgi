#include <Inventor/SoDB.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoCone.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoPointLight.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoShuttle.h>
#include <Inventor/nodes/SoTransformSeparator.h>

main(int , char **argv)
{
   // Initialize Inventor and Xt
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL)
      exit(1);

   SoSeparator *root = new SoSeparator;
   root->ref();

   // Add a directional light
   SoDirectionalLight *myDirLight = new SoDirectionalLight;
   myDirLight->direction.setValue(0, -1, -1);
   myDirLight->color.setValue(1, 0, 0);
   root->addChild(myDirLight);

   // Put the shuttle and the light below a transform separator.
   // A transform separator pushes and pops the transformation
   // just like a separator node, but other aspects of the state
   // are not pushed and popped. So the shuttle's translation
   // will affect only the light. But the light will shine on
   // the rest of the scene.
   SoTransformSeparator *myTransformSeparator =
       new SoTransformSeparator;
   root->addChild(myTransformSeparator);

   // A shuttle node translates back and forth between the two
   // fields translation0 and translation1. 
   // This moves the light.
   SoShuttle *myShuttle = new SoShuttle;
   myTransformSeparator->addChild(myShuttle);
   myShuttle->translation0.setValue(-2, -1, 3);
   myShuttle->translation1.setValue( 1,  2, -3);

   // Add the point light below the transformSeparator
   SoPointLight *myPointLight = new SoPointLight;
   myTransformSeparator->addChild(myPointLight);
   myPointLight->color.setValue(0, 1, 0);
 
   root->addChild(new SoCone);

   SoXtExaminerViewer *myViewer =
            new SoXtExaminerViewer(myWindow);
   myViewer->setSceneGraph(root);
   myViewer->setTitle("Lights");
   myViewer->setHeadlight(FALSE);
   myViewer->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}



