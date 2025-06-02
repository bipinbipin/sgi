#include <X11/StringDefs.h>
#include <X11/Intrinsic.h>

#include <Inventor/SoPath.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/SoXtMaterialEditor.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/actions/SoBoxHighlightRenderAction.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoSelection.h>
#include <Inventor/nodes/SoTransform.h>

typedef struct UserData {
   SoSelection *sel;
   SoXtMaterialEditor *editor;
   SbBool ignore;
};

//////////////////////////////////////////////////////////////
// CODE FOR The Inventor Mentor STARTS HERE  (part 1)

// Truncate the pick path so a nodekit is selected
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

// CODE FOR The Inventor Mentor ENDS HERE  
///////////////////////////////////////////////////////////////

// Create a sample scene graph
SoNode *
buildScene()
{
   SoGroup *g = new SoGroup;
   SoShapeKit *k;
   SoTransform *xf;
    
   // Place a dozen shapes in circular formation
   for (int i = 0; i < 12; i++) {
      k = new SoShapeKit;
      k->setPart("shape", new SoCube);
      xf = (SoTransform *) k->getPart("transform", TRUE);
      xf->translation.setValue(
         8*sin(i*M_PI/6), 8*cos(i*M_PI/6), 0.0);
      g->addChild(k);
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
   sel->addChild(buildScene());

   // Create a viewer with a render action that displays highlights
   SoXtExaminerViewer *viewer = new SoXtExaminerViewer(mainWindow);
   viewer->setSceneGraph(sel);
   viewer->setGLRenderAction(new SoBoxHighlightRenderAction());
   viewer->redrawOnSelectionChange(sel);
   viewer->setTitle("Select Node Kits");
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

