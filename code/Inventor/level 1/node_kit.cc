#include <Inventor/engines/SoCalculator.h>
#include <Inventor/engines/SoElapsedTime.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTransform.h>

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>

main(int , char **argv)
{
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   SoSeparator *root = new SoSeparator;
   root->ref();

   // Create shape kits with the words "HAPPY" and "NICE"
   SoShapeKit *happyKit = new SoShapeKit;
   root->addChild(happyKit);
   happyKit->setPart("shape", new SoText3);
   happyKit->set("shape { parts ALL string \"HAPPY\"}");
   happyKit->set("font { size 2}");

   SoShapeKit *niceKit = new SoShapeKit;
   root->addChild(niceKit);
   niceKit->setPart("shape", new SoText3);
   niceKit->set("shape { parts ALL string \"NICE\"}");
   niceKit->set("font { size 2}");

   // Create the Elapsed Time engine
   SoElapsedTime *myTimer = new SoElapsedTime;
   myTimer->ref();



   // Create two calculators - one for HAPPY, one for NICE.
   SoCalculator *happyCalc = new SoCalculator;
   happyCalc->ref();
   happyCalc->a.connectFrom(&myTimer->timeOut);
   happyCalc->expression = "ta=cos(2*a); tb=sin(2*a);\
      oA = vec3f(3*pow(ta,3),3*pow(tb,3),1);         \
      oB = vec3f(fabs(ta)+.1,fabs(.5*fabs(tb))+.1,1);\
      oC = vec3f(fabs(ta),fabs(tb),.5)";

   // The second calculator uses different arguments to
   // sin() and cos(), so it moves out of phase.
   SoCalculator *niceCalc = new SoCalculator;
   niceCalc->ref();
   niceCalc->a.connectFrom(&myTimer->timeOut);
   niceCalc->expression = "ta=cos(2*a+2); tb=sin(2*a+2);\
      oA = vec3f(3*pow(ta,3),3*pow(tb,3),1);            \
      oB = vec3f(fabs(ta)+.1,fabs(.5*fabs(tb))+.1,1);   \
      oC = vec3f(fabs(ta),fabs(tb),.5)";

   // Connect the transforms from the calculators...
   SoTransform *happyXf
      = (SoTransform *) happyKit->getPart("transform",TRUE);
   happyXf->translation.connectFrom(&happyCalc->oA);
   happyXf->scaleFactor.connectFrom(&happyCalc->oB);
   SoTransform *niceXf
      = (SoTransform *) niceKit->getPart("transform",TRUE);
   niceXf->translation.connectFrom(&niceCalc->oA);
   niceXf->scaleFactor.connectFrom(&niceCalc->oB);

   // Connect the materials from the calculators...
   SoMaterial *happyMtl
      = (SoMaterial *) happyKit->getPart("material",TRUE);
   happyMtl->diffuseColor.connectFrom(&happyCalc->oC);
   SoMaterial *niceMtl
      = (SoMaterial *) niceKit->getPart("material",TRUE);
   niceMtl->diffuseColor.connectFrom(&niceCalc->oC);

   SoXtExaminerViewer *myViewer = new
            SoXtExaminerViewer(myWindow);
   myViewer->setSceneGraph(root);
   myViewer->setTitle("Frolicking Words");
   myViewer->viewAll();
   myViewer->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}

