// CC -g -n32 -I/usr/include/Inventor -I/usr/include/X11 -c dialplex.cc && CC dialplex.o -L/usr/lib32 -lInventorInput -lInventorXt -lInventor -lX11 -lGL -lGLU -lm -o dialplex

#include <X11/Intrinsic.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodekits/SoShapeKit.h>
#include <Inventor/nodekits/SoSceneKit.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoComplexity.h>
#include <Inventor/nodes/SoSphere.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoCylinder.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/nodes/SoOrthographicCamera.h>
#include <Inventor/nodes/SoFaceSet.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoLineSet.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoFont.h>
#include <Inventor/nodes/SoText2.h>

// Had to compile each of these and link /usr/lib32/libInventorInput.so
#include <Inventor/Input/ButtonBoxEvent.h>
#include <Inventor/Input/DialEvent.h>
#include <Inventor/Input/DialNButton.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include "overlay.h"
#include "dialhandler.h"

// Forward declarations for dial handler factories
extern DialHandler* createDialSet0();
extern DialHandler* createDialSet1();
extern DialHandler* createDialSet2();
extern DialHandler* createDialSet3();
extern DialHandler* createDialSet4();
extern DialHandler* createDialSet5();
extern DialHandler* createDialSet6();
extern DialHandler* createDialSet7();

// Global scene objects and state
SceneObjects sceneObjects;
DialState dialState;

// Global viewer for overlay system compatibility
SoXtExaminerViewer *globalViewer = NULL;

// Current dial handler
DialHandler* currentDialHandler = NULL;
int currentSetIndex = 0;
bool overlayVisible = false;

// Initialize dial state
void initDialState() {
    for (int i = 0; i < 8; i++) {
        dialState.dialAngles[i] = 0.0f;
        dialState.lastDialValues[i] = 0;
        dialState.firstDialEvent[i] = true;
    }
    dialState.handlerData = NULL;
}

// Switch to a different dial handler
void switchDialHandler(int setIndex) {
    // Cleanup current handler
    if (currentDialHandler) {
        currentDialHandler->cleanup(&sceneObjects, &dialState);
        delete currentDialHandler;
        currentDialHandler = NULL;
    }
    
    // Reset dial state for new handler
    initDialState();
    
    // Create new handler based on set index
    switch (setIndex) {
        case 0:
            currentDialHandler = createDialSet0();
            break;
        case 1:
            currentDialHandler = createDialSet1();
            break;
        case 2:
            currentDialHandler = createDialSet2();
            break;
        case 3:
            currentDialHandler = createDialSet3();
            break;
        case 4:
            currentDialHandler = createDialSet4();
            break;
        case 5:
            currentDialHandler = createDialSet5();
            break;
        case 6:
            currentDialHandler = createDialSet6();
            break;
        case 7:
            currentDialHandler = createDialSet7();
            break;
        default:
            // Default to set 0 for any other button
            currentDialHandler = createDialSet0();
            currentSetIndex = 0;
            break;
    }
    
    if (currentDialHandler) {
        currentDialHandler->init(&sceneObjects, &dialState);
        printf("Switched to dial set %d: %s\n", setIndex, currentDialHandler->getDescription());
    }
}

// Reset scene to initial state
void resetScene() {
    printf("Resetting scene to initial state\n");
    
    // Reset all scene transforms
    sceneObjects.sceneZoom->scaleFactor.setValue(1.0f, 1.0f, 1.0f);
    sceneObjects.sceneRotX->rotation.setValue(SbVec3f(1, 0, 0), 0.0f);
    sceneObjects.sceneRotY->rotation.setValue(SbVec3f(0, 1, 0), 0.0f);
    sceneObjects.sceneRotZ->rotation.setValue(SbVec3f(0, 0, 1), 0.0f);
    
    // Reset all dial transforms and materials
    for (int i = 0; i < 8; i++) {
        if (sceneObjects.dialTransforms[i]) {
            sceneObjects.dialTransforms[i]->rotation.setValue(SbVec3f(0, 0, 1), 0.0f);
        }
        if (sceneObjects.scatterOffsets[i]) {
            sceneObjects.scatterOffsets[i]->translation.setValue(0.0f, 0.0f, 0.0f);
        }
        if (sceneObjects.cylinderOffsets[i]) {
            sceneObjects.cylinderOffsets[i]->translation.setValue(0.0f, 0.0f, 0.0f);
        }
        if (sceneObjects.dialMaterials[i]) {
            sceneObjects.dialMaterials[i]->diffuseColor.setValue(0.2f, 0.25f, 0.35f);
            sceneObjects.dialMaterials[i]->specularColor.setValue(0.4f, 0.4f, 0.4f);
            sceneObjects.dialMaterials[i]->shininess.setValue(0.6f);
            sceneObjects.dialMaterials[i]->emissiveColor.setValue(0.03f, 0.03f, 0.05f);
        }
        if (sceneObjects.dialCylinders[i]) {
            sceneObjects.dialCylinders[i]->height.setValue(0.2f);
        }
    }
    
    // Reset background to original
    if (sceneObjects.globalViewer) {
        sceneObjects.globalViewer->setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
    }
    
    // Reset dial state
    initDialState();
    
    // Cleanup current dial handler and reset to default (set 0)
    if (currentDialHandler) {
        currentDialHandler->cleanup(&sceneObjects, &dialState);
        delete currentDialHandler;
        currentDialHandler = NULL;
    }
    
    currentSetIndex = 0;
    switchDialHandler(0);
    
    // Force a render
    if (sceneObjects.globalViewer) {
        sceneObjects.globalViewer->render();
    }
}

void
buttonBoxCB(void *userData, SoEventCallback *cb)
{
    const ButtonBoxEvent *ev = (const ButtonBoxEvent *) cb->getEvent();
    if (ev->getState() == SoButtonEvent::DOWN) {
        overlayVisible = true;
        int which = ev->getButton();
        printf("button %d pressed\n", which);
        
        if (which >= 1 && which <= 32) {
            currentSetIndex = which - 1;
            
            // Handle reset button (button 32, index 31)
            if (currentSetIndex == 31) {
                resetScene();
                currentSetIndex = 0; // Reset to dial set 0
            }
            // Switch dial handler for buttons 1-8 (indices 0-7)
            else if (currentSetIndex >= 0 && currentSetIndex <= 7) {
                switchDialHandler(currentSetIndex);
            }
            
            if (sceneObjects.globalViewer) {
                sceneObjects.globalViewer->render();
            }
        }
        updateOverlaySceneGraph(globalViewer, currentSetIndex, overlayVisible, currentDialHandler);
    } else if (ev->getState() == SoButtonEvent::UP) {
        overlayVisible = false;
        updateOverlaySceneGraph(globalViewer, currentSetIndex, overlayVisible, currentDialHandler);
        if (sceneObjects.globalViewer) {
            sceneObjects.globalViewer->render();
        }
    }
}

void dialCB(void *userData, SoEventCallback *cb)
{
    const DialEvent *ev = (const DialEvent *) cb->getEvent();
    int dialIndex = ev->getDial();
    if (dialIndex < 0 || dialIndex >= 8) return;

    int rawValue = ev->getValue();
    
    // Delegate to current dial handler
    if (currentDialHandler) {
        currentDialHandler->handleDial(dialIndex, rawValue, &sceneObjects, &dialState);
    }
}

SoNode *
buildSceneGraph()
{
   SoGroup *sep = new SoGroup;
   // Set cylinder detail level high
   SoComplexity *complexity = new SoComplexity;
   complexity->value.setValue(1.0f);  // full smoothness
   complexity->type.setValue(SoComplexity::OBJECT_SPACE);
   sep->addChild(complexity);

   SoEventCallback *cb = new SoEventCallback;

   // Create transforms and store in sceneObjects
   sceneObjects.sceneZoom = new SoTransform;
   sceneObjects.sceneRotX = new SoTransform;
   sceneObjects.sceneRotY = new SoTransform;
   sceneObjects.sceneRotZ = new SoTransform;

   // Create separators
   SoSeparator *zoomSep = new SoSeparator;
   SoSeparator *xRotSep = new SoSeparator;
   SoSeparator *yRotSep = new SoSeparator;
   SoSeparator *zRotSep = new SoSeparator;

   // Assemble hierarchy
   zoomSep->addChild(sceneObjects.sceneZoom);
   zoomSep->addChild(xRotSep);
   xRotSep->addChild(sceneObjects.sceneRotX);
   xRotSep->addChild(yRotSep);
   yRotSep->addChild(sceneObjects.sceneRotY);
   yRotSep->addChild(zRotSep);
   zRotSep->addChild(sceneObjects.sceneRotZ);
   zRotSep->addChild(cb);

   // Logical layout: dial index => (row, col)
   int dialLayout[8][2] = {
       {3, 0},  // dial 0 → bottom left
       {3, 1},  // dial 1 → bottom right
       {2, 0},
       {2, 1},
       {1, 0},
       {1, 1},
       {0, 0},
       {0, 1}   // dial 7 → top right
   };

   for (int x = 0; x < 8; x++) {
      
      SoSeparator *dialGroup = new SoSeparator;

      // Get layout grid position
      int row = dialLayout[x][0];
      int col = dialLayout[x][1];

      // Step 1: Layout offset (row/col positioning)
      SoTransform *layoutOffset = new SoTransform;
      layoutOffset->translation.setValue(col * 3.0f, -row * 2.5f, 0);
      dialGroup->addChild(layoutOffset);

      // Step 2: Scatter transform (added *after* layout but *before* visual elements)
      SoTransform *scatterTransform = new SoTransform;
      dialGroup->addChild(scatterTransform);
      sceneObjects.scatterOffsets[x] = scatterTransform;  // Store for manipulation

      // Step 3: Face camera
      SoTransform *faceCamera = new SoTransform;
      faceCamera->rotation.setValue(SbVec3f(1, 0, 0), M_PI / 2);
      dialGroup->addChild(faceCamera);

      // Step 4: Rotatable transform (per dial)
      sceneObjects.dialTransforms[x] = new SoTransform;
      dialGroup->addChild(sceneObjects.dialTransforms[x]);

      // Step 5: Cylinder (potentiometer)
      SoShapeKit *potentiometer = new SoShapeKit;
      SoCylinder *cylinder = new SoCylinder;
      cylinder->height = 0.2f;
      potentiometer->setPart("shape", cylinder);
      SoMaterial *mat = new SoMaterial;
      mat->diffuseColor.setValue(0.2f, 0.25f, 0.35f);    // cool steel blue
      mat->specularColor.setValue(0.4f, 0.4f, 0.4f);     // soft highlight
      mat->shininess.setValue(0.6f);                    // semi-shiny surface
      mat->emissiveColor.setValue(0.03f, 0.03f, 0.05f);  // slight glow for depth

      potentiometer->setPart("material", mat);
      sceneObjects.dialMaterials[x] = mat;  // store in sceneObjects
      sceneObjects.dialCylinders[x] = cylinder;
      dialGroup->addChild(potentiometer);

      // Step 6: Cylinder offset for downward growth
      SoTransform *cylinderOffset = new SoTransform;
      dialGroup->addChild(cylinderOffset);
      sceneObjects.cylinderOffsets[x] = cylinderOffset;

      // Step 7: Pointer offset
      SoTransform *pointerOffset = new SoTransform;
      pointerOffset->translation.setValue(0, 0.1f, 0.5f);
      dialGroup->addChild(pointerOffset);

      // Step 8: Pointer
      SoShapeKit *pointer = new SoShapeKit;
      pointer->setPart("shape", new SoCube);
      pointer->set("shape { width 0.2 height 0.1 depth 1 }");
      pointer->set("material { diffuseColor 0.78 0.57 0.11 }");
      dialGroup->addChild(pointer);

      // Final: Add the whole dialGroup to your scene
      zRotSep->addChild(dialGroup);
   }

   // Setup callbacks
   cb->addEventCallback(DialEvent::getClassTypeId(), dialCB, NULL);
   cb->addEventCallback(ButtonBoxEvent::getClassTypeId(), buttonBoxCB, NULL);

   sep->addChild(zoomSep);
   
   return sep;
}

int
main(int , char *argv[])
{
   // Initialize dial state
   initDialState();

   Widget mainWindow = SoXt::init(argv[0]);
   
   if (! DialNButton::exists()) {
       fprintf(stderr, "Sorry, no dial and button box on this display!\n");
       return 1;
   }
   
   ButtonBoxEvent::initClass();
   DialEvent::initClass();
   
   SoXtExaminerViewer *vwr = new SoXtExaminerViewer(mainWindow);
   sceneObjects.globalViewer = vwr;
   globalViewer = vwr;  // Set global variable for overlay system compatibility
   vwr->setSceneGraph(buildSceneGraph());
   vwr->setTitle("Dial Open Inventor Demo");
   vwr->setViewing(FALSE);   // come up in pick mode
   vwr->setHeadlight(TRUE); // we supply our own light

   // window settings
   vwr->setSize(SbVec2s(1280, 1024));
   vwr->setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
   vwr->setDecoration(FALSE);

   // Initialize overlay system
   initOverlay(vwr);
   updateOverlaySceneGraph(globalViewer, currentSetIndex, overlayVisible, currentDialHandler);

   // Initialize with default dial handler (set 0)
   switchDialHandler(0);

   DialNButton *db = new DialNButton;
   vwr->registerDevice(db);
   
   vwr->show();
   SoXt::show(mainWindow);
   SoXt::mainLoop();
   
   return 0;
}
