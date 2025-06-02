//  Uses 3 translate1Draggers to change the x, y, and z
//  components of a translation. A calculator engine assembles
//  the components.
//  Arranges these draggers along edges of a box containing the
//  3D text to be moved.
//  The 3D text and the box are made with SoShapeKits

#include <Inventor/engines/SoCalculator.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoText3.h>
#include <Inventor/nodes/SoTransform.h>

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>

#include <Inventor/draggers/SoTranslate1Dragger.h>

main(int , char **argv)
{
   Widget myWindow = SoXt::init(argv[0]);
   if (myWindow == NULL) exit(1);

   SoSeparator *root = new SoSeparator;
   root->ref();

   // Create 3 translate1Draggers and place them in space.
   SoSeparator *xDragSep = new SoSeparator;
   SoSeparator *yDragSep = new SoSeparator;
   SoSeparator *zDragSep = new SoSeparator;
   root->addChild(xDragSep);
   root->addChild(yDragSep);
   root->addChild(zDragSep);
   // Separators will each hold a different transform
   SoTransform *xDragXf = new SoTransform;
   SoTransform *yDragXf = new SoTransform;
   SoTransform *zDragXf = new SoTransform;
   xDragXf->set("translation 0 -4 8");
   yDragXf->set("translation -8 0 8 rotation 0 0 1 1.57");
   zDragXf->set("translation -8 -4 0 rotation 0 1 0 -1.57");
   xDragSep->addChild(xDragXf);
   yDragSep->addChild(yDragXf);
   zDragSep->addChild(zDragXf);

   // Add the draggers under the separators, after transforms
   SoTranslate1Dragger *xDragger = new SoTranslate1Dragger;
   SoTranslate1Dragger *yDragger = new SoTranslate1Dragger;
   SoTranslate1Dragger *zDragger = new SoTranslate1Dragger;
   xDragSep->addChild(xDragger);
   yDragSep->addChild(yDragger);
   zDragSep->addChild(zDragger);

   // Create shape kit for the 3D text
   // The text says 'Slide Arrows To Move Me'
   SoShapeKit *textKit = new SoShapeKit;
   root->addChild(textKit);
   SoText3 *myText3 = new SoText3;
   textKit->setPart("shape", myText3);
   myText3->justification = SoText3::CENTER;
   myText3->string.set1Value(0,"Slide Arrows");
   myText3->string.set1Value(1,"To");
   myText3->string.set1Value(2,"Move Me");
   textKit->set("font { size 2}");
   textKit->set("material { diffuseColor 1 1 0}");

   // Create shape kit for surrounding box.
   // It's an unpickable cube, sized as (16,8,16)
   SoShapeKit *boxKit = new SoShapeKit;
   root->addChild(boxKit);
   boxKit->setPart("shape", new SoCube);
   boxKit->set("drawStyle { style LINES }");
   boxKit->set("pickStyle { style UNPICKABLE }");
   boxKit->set("material { emissiveColor 1 0 1 }");
   boxKit->set("shape { width 16 height 8 depth 16 }");

   // Create the calculator to make a translation
   // for the text. The x component of a translate1Dragger's
   // translation field shows how far it moved in that
   // direction. So our text's translation is:
   // (xDragTranslate[0],yDragTranslate[0],zDragTranslate[0])
   SoCalculator *myCalc = new SoCalculator;
   myCalc->ref();
   myCalc->A.connectFrom(&xDragger->translation);
   myCalc->B.connectFrom(&yDragger->translation);
   myCalc->C.connectFrom(&zDragger->translation);
   myCalc->expression = "oA = vec3f(A[0],B[0],C[0])";

   // Connect the the translation in textKit from myCalc
   SoTransform *textXf = (SoTransform *)
            textKit->getPart("transform",TRUE);
   textXf->translation.connectFrom(&myCalc->oA);

   SoXtExaminerViewer *myViewer = new
            SoXtExaminerViewer(myWindow);
   myViewer->setSceneGraph(root);
   myViewer->setTitle("Slider Box");
   myViewer->viewAll();
   myViewer->show();

   SoXt::show(myWindow);
   SoXt::mainLoop();
}
