#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoTranslation.h>

#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>

main(int argc, char **argv)
{
   Widget myWindow = SoXt::init(argv[0]);
   if(myWindow == NULL) exit(1);

   SoGroup *root = new SoGroup;
   root->ref();

   // Choose a font.
   SoFont *myFont = new SoFont;
   myFont->name.setValue("Times-Roman");
   myFont->size.setValue(24.0);
   root->addChild(myFont);

   // Add the globe, a sphere with a texture map.
   // Put it within a separator.
   SoSeparator *sphereSep = new SoSeparator;
   SoTexture2  *myTexture2 = new SoTexture2;
   root->addChild(sphereSep);
   sphereSep->addChild(myTexture2);
   sphereSep->addChild(new SoSphere);
   myTexture2->filename = "globe.rgb";

   // Add Text2 for AFRICA, translated to proper location.
   SoSeparator *africaSep = new SoSeparator;
   SoTranslation *africaTranslate = new SoTranslation;
   SoText2 *africaText = new SoText2;
   africaTranslate->translation.setValue(.25,.0,1.25);
   africaText->string = "AFRICA";
   root->addChild(africaSep);
   africaSep->addChild(africaTranslate);
   africaSep->addChild(africaText);

   // Add Text2 for ASIA, translated to proper location.
   SoSeparator *asiaSep = new SoSeparator;
   SoTranslation *asiaTranslate = new SoTranslation;
   SoText2 *asiaText = new SoText2;
   asiaTranslate->translation.setValue(.8,.8,0);
   asiaText->string = "ASIA";
   root->addChild(asiaSep);
   asiaSep->addChild(asiaTranslate);
   asiaSep->addChild(asiaText);

   SoXtExaminerViewer *myViewer =
            new SoXtExaminerViewer(myWindow);
   myViewer->setSceneGraph(root);
   myViewer->setTitle("2D Text");
   myViewer->setBackgroundColor(SbColor(0.35, 0.35, 0.35));
   myViewer->show();
   myViewer->viewAll();
  
   SoXt::show(myWindow);
   SoXt::mainLoop();
}
