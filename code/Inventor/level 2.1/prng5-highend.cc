#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>
#include <string.h>

#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtMaterialEditor.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/actions/SoBoxHighlightRenderAction.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoText2.h>
#include <Inventor/nodes/SoTexture2.h>
#include <Inventor/nodes/SoRotor.h>

#define PARTS_PER_LOOP 18

typedef struct UserData {
   SoSelection *sel;
   SoXtMaterialEditor *editor;
   SbBool ignore;
};

SoPath *
pickFilterCB(void *, const SoPickedPoint *pick)
{    
   // See which child of selection got picked
   SoPath *p = pick->getPath();
   int i;
   for (i = p->getLength() - 1; i >= 0; i--) {
      SoNode *n = p->getNode(i);
      if (n->isOfType(SoShapeKit::getClassTypeId()))
         break;
   }
   // Copy the path down to the nodekit
   return p->copy(0, i+1);
}

// Create a sample scene graph
SoNode *
buildScene()
{
   int i,j;
   SoGroup *g = new SoGroup;
   SoShapeKit *k;
   SoTransform *xf;

   SoTransform *zNotch = new SoTransform;
   //zNotch->translation.setValue(0,0,0);
   zNotch->rotation.setValue(SbVec3f(0, 1, 0.5), 90);

   SoRotor *partRotor = new SoRotor;
   
  for (j = 0; j < 3; j++) {

   	for (i = 1; i <= PARTS_PER_LOOP; i++) {
		k = new SoShapeKit;

		k->setPart("shape", new SoSphere);
		xf = (SoTransform *) k->getPart("transform", TRUE);
		xf->scaleFactor.setValue(0.75, 0.75, 0.75);
		xf->translation.setValue(8*sin(i*M_PI/4), 8*cos(i*M_PI/4), 0.0);

		partRotor->rotation.setValue(SbVec3f((i/10), 0, 1), 0); // z axis
		partRotor->speed = (0.001 + (0.0008 * i));
		g->addChild(partRotor);
		g->addChild(k);
		}
	g->addChild(zNotch);
   }


   return g;
}

// Update the material editor to reflect the selected object
void
selectCB(void *userData, SoPath *path)
{
   SoShapeKit *kit = (SoShapeKit *) path->getTail();
   SoMaterial *kitMtl = 
      (SoMaterial *) kit->getPart("material", TRUE);

   UserData *ud = (UserData *) userData;
   ud->ignore = TRUE;
   ud->editor->setMaterial(*kitMtl);
   ud->ignore = FALSE;
}

// This is called when the user chooses a new material
// in the material editor. This updates the material
// part of each selected node kit.
void
mtlChangeCB(void *userData, const SoMaterial *mtl)
{
   // Our material change callback is invoked when the
   // user changes the material, and when we change it
   // through a call to SoXtMaterialEditor::setMaterial.
   // In this latter case, we ignore the callback invocation.
   UserData *ud = (UserData *) userData;
   if (ud->ignore)
      return;

   SoSelection *sel = ud->sel;
    
   // Our pick filter guarantees the path tail will
   // be a shape kit.
   for (int i = 0; i < sel->getNumSelected(); i++) {
      SoPath *p = sel->getPath(i);
      SoShapeKit *kit = (SoShapeKit *) p->getTail();
      SoMaterial *kitMtl = 
         (SoMaterial *) kit->getPart("material", TRUE);
      kitMtl->copyFieldValues(mtl);
   }
}

void
main(int , char *argv[])
{
   // Initialization
   Widget mainWindow = SoXt::init(argv[0]);
    
   // Create our scene graph.
   SoSelection *sel = new SoSelection;
   sel->ref();
   
   // Choose a font
   SoFont *myFont = new SoFont;
   myFont->name.setValue("Times-Roman");
   myFont->size.setValue(24.0);
   sel->addChild(myFont);

   sel->addChild(buildScene());

   SoRotor *groupRotor = new SoRotor;
   groupRotor->rotation.setValue(SbVec3f(0, 1, 0), 0);
   groupRotor->speed = 0.003;
   sel->addChild(groupRotor);

   // Create a viewer with a render action that displays highlights
   SoXtExaminerViewer *viewer = new SoXtExaminerViewer(mainWindow);
   viewer->setSceneGraph(sel);
   viewer->setGLRenderAction(new SoBoxHighlightRenderAction());
   viewer->redrawOnSelectionChange(sel);
   viewer->setTitle("Living Menu *highend");
   viewer->show();

   // Create a material editor
   SoXtMaterialEditor *ed = new SoXtMaterialEditor();
   ed->show();

   // User data for our callbacks
   UserData userData;
   userData.sel = sel;
   userData.editor = ed;
   userData.ignore = FALSE;
   
   // Selection and material change callbacks
   ed->addMaterialChangedCallback(mtlChangeCB, &userData);
   sel->setPickFilterCallback(pickFilterCB);
   sel->addSelectionCallback(selectCB, &userData);
   
   SoXt::show(mainWindow);
   SoXt::mainLoop();
}

