// CC -g -n32 -I/usr/include/Inventor -I/usr/include/X11 -c dialNbutton_event-test.cc && CC dialNbutton_event-test.o -L/usr/lib32 -lInventorInput -lInventorXt -lInventor -lX11 -lGL -lGLU -lm -o dialNbutton_event-test

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

// Had to compile each of these and link /usr/lib32/libInventorInput.so
#include <Inventor/Input/ButtonBoxEvent.h>
#include <Inventor/Input/DialEvent.h>
#include <Inventor/Input/DialNButton.h>

SoXtExaminerViewer *globalViewer = NULL;
SoTransform *sceneRotX = NULL;
SoTransform *sceneRotY = NULL;
SoTransform *sceneRotZ = NULL;
SoTransform *sceneZoom  = NULL;


int dialVals[8] = {0, 0, 0, 0, 0, 0, 0, 0};
SoTransform *dialTransforms[8];     // one for each dial's controlled object
float dialAngles[8] = {0};          // accumulated rotation angles
int lastDialValues[8] = {0};        // last raw values from device
bool firstDialEvent[8] = {true};    // used to skip first jump
SoCylinder *dialCylinders[8];       // one per dial for changing dial depth
SoTransform *cylinderOffsets[8];
SoTransform *scatterOffsets[8];
SbVec3f lastScatter6[8];
SbVec3f lastScatter7[8];
SbVec3f bloomVectors[8];



void setBackgroundFromDial(float angle)
{
    if (!globalViewer) return;

    float phase = fmodf(angle / (4.0f * M_PI), 1.0f);
    if (phase < 0) phase += 1.0f;

    float r = 0.2f + 0.1f * sinf(2 * M_PI * phase + M_PI / 3);   // low red
    float g = 0.2f + 0.1f * sinf(2 * M_PI * phase + M_PI);       // dark green
    float b = 0.3f + 0.3f * sinf(2 * M_PI * phase);              // deep blue/purple

    // Clamp to [0,1] manually
    r = (r < 0.0f) ? 0.0f : ((r > 1.0f) ? 1.0f : r);
    g = (g < 0.0f) ? 0.0f : ((g > 1.0f) ? 1.0f : g);
    b = (b < 0.0f) ? 0.0f : ((b > 1.0f) ? 1.0f : b);

    globalViewer->setBackgroundColor(SbColor(r, g, b));
    globalViewer->render();
}

void
buttonBoxCB(void *userData, SoEventCallback *cb)
{
   const ButtonBoxEvent *ev = 
      (const ButtonBoxEvent *) cb->getEvent();
	
   if (ev->getState() == SoButtonEvent::DOWN) {
      int which = ev->getButton();
      printf("button %d pressed\n", which);
   }
}

void dialCB(void *userData, SoEventCallback *cb)
{
    const DialEvent *ev = (const DialEvent *) cb->getEvent();
    int dialIndex = ev->getDial();
    if (dialIndex < 0 || dialIndex >= 8) return;

    int rawValue = ev->getValue();

    if (firstDialEvent[dialIndex]) {
        lastDialValues[dialIndex] = rawValue;
        firstDialEvent[dialIndex] = false;
        return;
    }

    int delta = rawValue - lastDialValues[dialIndex];
    lastDialValues[dialIndex] = rawValue;

    float deltaAngle = delta * (M_PI / 128.0f);
    dialAngles[dialIndex] += deltaAngle;

    switch (dialIndex) {
         case 0:
            setBackgroundFromDial(dialAngles[0]);
            break;

         case 1:
            if (sceneRotY)
                sceneRotY->rotation.setValue(SbVec3f(0, 1, 0), dialAngles[1]);
            break;

         case 2:
            if (sceneZoom) {
               float zoomZ = -5.0f + dialAngles[2] * 2.0f;  // tweak scale as needed
               sceneZoom->translation.setValue(0, 0, zoomZ);
            }
            break;

         case 3:
            if (sceneRotX)
                sceneRotX->rotation.setValue(SbVec3f(1, 0, 0), dialAngles[3]);
            break;
         
         case 4:
            for (int i = 0; i < 8; i++) {
               if (dialCylinders[i] && cylinderOffsets[i]) {
                     float baseHeight = 0.2f;
                     float angle = (dialAngles[4] > 0.0f) ? dialAngles[4] : 0.0f;
                     float scale = 1.0f + angle * 0.2f;

                     // Set cylinder height
                     float newHeight = baseHeight * scale;
                     dialCylinders[i]->height = newHeight;

                     // Offset cylinder upward by half height to grow downward
                     cylinderOffsets[i]->translation.setValue(0, newHeight / 2.0f, 0);
               }
            }
            break;

         case 5:
            if (sceneRotZ)
                sceneRotZ->rotation.setValue(SbVec3f(0, 0, 1), dialAngles[5]);
            break;

         case 6: {
            for (int i = 0; i < 8; i++) {
               if (scatterOffsets[i]) {
                     float strength = 0.2f * sinf(dialAngles[6]);

                     // Pseudo-random direction
                     float dx = strength * sinf(i * 1.7f + dialAngles[6]);
                     float dy = strength * cosf(i * 2.3f + dialAngles[6]);
                     float dz = strength * sinf(i * 3.1f - dialAngles[6]);

                     SbVec3f newOffset(dx, dy, dz);

                     // Apply delta
                     SbVec3f delta = newOffset - lastScatter6[i];
                     lastScatter6[i] = newOffset;

                     // Apply to current transform
                     SbVec3f current = scatterOffsets[i]->translation.getValue();
                     scatterOffsets[i]->translation.setValue(current + delta);
               }
            }
            break;
         }
         case 7: {
            for (int i = 0; i < 8; i++) {
               if (scatterOffsets[i]) {
                     float t = dialAngles[7];  // dial value

                     float radius = t * 0.5f;  // control bloom scale
                     SbVec3f newOffset = bloomVectors[i] * radius;

                     SbVec3f delta = newOffset - lastScatter7[i];
                     lastScatter7[i] = newOffset;

                     SbVec3f current = scatterOffsets[i]->translation.getValue();
                     scatterOffsets[i]->translation.setValue(current + delta);
               }
            }
            break;
         }

    }

    // Default: rotate own object
    if (dialTransforms[dialIndex]) {
        dialTransforms[dialIndex]->rotation.setValue(SbVec3f(0, 1, 0), dialAngles[dialIndex]);
    }
}

SoNode *
buildSceneGraph()
{
   for (int x = 0; x < 8; x++) {
      float angle = x * (M_PI / 4.0f);  // 8 directions evenly spaced
      bloomVectors[x] = SbVec3f(cosf(angle), sinf(angle), ((x % 2 == 0) ? 0.5f : -0.5f));  // radial + Z twist
   }

   SoGroup *sep = new SoGroup;
   SoEventCallback *cb = new SoEventCallback;

   // Create transforms
   sceneZoom = new SoTransform;
   sceneRotX = new SoTransform;
   sceneRotY = new SoTransform;
   sceneRotZ = new SoTransform;

   // Create separators
   SoSeparator *zoomSep = new SoSeparator;
   SoSeparator *xRotSep = new SoSeparator;
   SoSeparator *yRotSep = new SoSeparator;
   SoSeparator *zRotSep = new SoSeparator;

   // Assemble hierarchy
   zoomSep->addChild(sceneZoom);
   zoomSep->addChild(xRotSep);
   xRotSep->addChild(sceneRotX);
   xRotSep->addChild(yRotSep);
   yRotSep->addChild(sceneRotY);
   yRotSep->addChild(zRotSep);
   zRotSep->addChild(sceneRotZ);
   zRotSep->addChild(cb);

   SbVec3f Yaxis(0, 1, 0);

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

      // ✅ Step 2: Scatter transform (added *after* layout but *before* visual elements)
      SoTransform *scatterTransform = new SoTransform;
      dialGroup->addChild(scatterTransform);
      scatterOffsets[x] = scatterTransform;  // Store for Dial 6 to manipulate

      // Step 3: Face camera
      SoTransform *faceCamera = new SoTransform;
      faceCamera->rotation.setValue(SbVec3f(1, 0, 0), M_PI / 2);
      dialGroup->addChild(faceCamera);

      // Step 4: Rotatable transform (per dial)
      dialTransforms[x] = new SoTransform;
      dialGroup->addChild(dialTransforms[x]);

      // Step 5: Cylinder (potentiometer)
      SoShapeKit *potentiometer = new SoShapeKit;
      SoCylinder *cylinder = new SoCylinder;
      cylinder->height = 0.2f;
      potentiometer->setPart("shape", cylinder);
      potentiometer->set("material { diffuseColor 0.6 0.6 0.6 }");
      dialCylinders[x] = cylinder;
      dialGroup->addChild(potentiometer);

      // Step 6: Cylinder offset for downward growth
      SoTransform *cylinderOffset = new SoTransform;
      dialGroup->addChild(cylinderOffset);
      cylinderOffsets[x] = cylinderOffset;

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
   cb->addEventCallback(DialEvent::getClassTypeId(), dialCB, NULL); // We’ll look up the transform via dial ID
   cb->addEventCallback(ButtonBoxEvent::getClassTypeId(), buttonBoxCB, NULL);

   sep->addChild(zoomSep);
   return sep;
}



void
main(int , char *argv[])
{
   for (int i = 0; i < 8; i++) {
      lastScatter6[i] = SbVec3f(0, 0, 0);
      lastScatter7[i] = SbVec3f(0, 0, 0);
   }

   Widget mainWindow = SoXt::init(argv[0]);
   
   if (! DialNButton::exists()) {
       fprintf(stderr, "Sorry, no dial and button box on this display!\n");
       return;
   }
   
   ButtonBoxEvent::initClass();
   DialEvent::initClass();
   
   SoXtExaminerViewer *vwr = new SoXtExaminerViewer(mainWindow);
   globalViewer = vwr;
   vwr->setSceneGraph(buildSceneGraph());
   vwr->setTitle("Dial Open Inventor Demo");
   vwr->setViewing(FALSE);   // come up in pick mode
   vwr->setHeadlight(TRUE); // we supply our own light

   // window settings
   vwr->setSize(SbVec2s(1280, 1024));
   vwr->setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
   vwr->setDecoration(FALSE);


   DialNButton *db = new DialNButton;
   vwr->registerDevice(db);
   
   vwr->show();
   SoXt::show(mainWindow);
   SoXt::mainLoop();
}
