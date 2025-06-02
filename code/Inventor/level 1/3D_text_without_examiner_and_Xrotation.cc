#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtRenderArea.h>
//#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/engines/SoElapsedTime.h>

#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoLinearProfile.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoPerspectiveCamera.h>
#include <Inventor/nodes/SoProfileCoordinate2.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoRotationXYZ.h>
#include <Inventor/nodes/SoSeparator.h>

main(int argc, char **argv)
{

   Widget myWindow = SoXt::init(argv[0]);
   if(myWindow == NULL) exit(1);

   SoGroup *root = new SoGroup;
   root->ref();

   // Set up camera.
   SoPerspectiveCamera *myCamera = new SoPerspectiveCamera;
   myCamera->position.setValue(0, -(argc - 1) / 2, 10);
   myCamera->nearDistance.setValue(5.0);
   myCamera->farDistance.setValue(15.0);
   root->addChild(myCamera);
   root->addChild(new SoDirectionalLight);

   // This transformation is modified to rotate the cone
   SoRotationXYZ *myRotXYZ = new SoRotationXYZ;
   root->addChild(myRotXYZ);

   // Let's make the front of the text white,
   // and the sides and back yellow.
   SoMaterial *myMaterial = new SoMaterial;
   SbColor colors[3];
   // diffuse
   colors[0].setValue(1, 1, 1);
   colors[1].setValue(1, 1, 0);
   colors[2].setValue(1, 1, 0);
   myMaterial->diffuseColor.setValues(0, 3, colors);
   // specular
   colors[0].setValue(1, 1, 1);
   colors[1].setValue(1, 1, 0);
   colors[2].setValue(1, 1, 0);
   myMaterial->specularColor.setValues(0, 3, colors);
   myMaterial->shininess.setValue(.1);
   root->addChild(myMaterial);

   // Choose a font.
   SoFont *myFont = new SoFont;
   myFont->name.setValue("Times-Roman");
   root->addChild(myFont);

   // Specify a beveled cross-section for the text.
   SoProfileCoordinate2 *myProfileCoords =
            new SoProfileCoordinate2;
   SbVec2f coords[4];
   coords[0].setValue(.00, .00);
   coords[1].setValue(.25, .25);
   coords[2].setValue(1.25, .25);
   coords[3].setValue(1.50, .00);
   myProfileCoords->point.setValues(0, 4, coords);
   root->addChild(myProfileCoords);

   SoLinearProfile *myLinearProfile = new SoLinearProfile;
   long    index[4] ;
   index[0] = 0;
   index[1] = 1;
   index[2] = 2;
   index[3] = 3;
   myLinearProfile->index.setValues(0, 4, index);
   root->addChild(myLinearProfile);

   // Set the material binding to PER_PART.
   SoMaterialBinding *myMaterialBinding = new SoMaterialBinding;
   myMaterialBinding->
            value.setValue(SoMaterialBinding::PER_PART);
   root->addChild(myMaterialBinding);

   // Add the text.
   SoText3 *myText3 = new SoText3;
   myText3->string.setValue("You Have New Mail");
   myText3->justification.setValue(SoText3::CENTER);
   myText3->parts.setValue(SoText3::ALL);
   root->addChild(myText3);

   myRotXYZ->axis = SoRotationXYZ::X;   //rotate about X axis
   SoElapsedTime *myCounter = new SoElapsedTime;
   myRotXYZ->angle.connectFrom(&myCounter->timeOut);

   SoXtRenderArea *myRenderArea = new SoXtRenderArea(myWindow);
   myCamera->viewAll(root, myRenderArea->getViewportRegion());
   myRenderArea->setSceneGraph(root);
   myRenderArea->setTitle("3D text without examiner");
   myRenderArea->show();


//   SoXtExaminerViewer *myViewer =
//            new SoXtExaminerViewer(myWindow);
//   myViewer->setSceneGraph(root);
//   myViewer->setTitle("Complex 3D Text");
//   myViewer->show();
//   myViewer->viewAll();
  
   SoXt::show(myWindow);
   SoXt::mainLoop();
}
