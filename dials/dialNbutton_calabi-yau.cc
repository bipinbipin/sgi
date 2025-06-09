// CC -g -n32 -no_auto_include -I/usr/include/Inventor -I/usr/include/X11 -c dialNbutton_calabi-yau.cc && CC dialNbutton_calabi-yau.o -L/usr/lib32 -lInventorInput -lInventorXt -lInventor -lX11 -lGL -lGLU -lm -o dialNbutton_calabi-yau

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoIndexedFaceSet.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoTransform.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/Xt/devices/SoXtDevice.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/Input/ButtonBoxEvent.h>
#include <Inventor/Input/DialEvent.h>
#include <Inventor/Input/DialNButton.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoDrawStyle.h>
#include <Inventor/nodes/SoDirectionalLight.h>
#include <Inventor/SbColor.h>

#include <math.h>
#include <vector>
#include <stdio.h>

SoXtExaminerViewer *globalViewer = NULL;

SoSeparator *calabiYauRoot = NULL;
SoCoordinate3 *cyCoords = NULL;
SoIndexedFaceSet *cyFaces = NULL;
SoTransform *cyScale = NULL;
SoMaterial *cyMaterial = NULL;

int currentCYResolution = 20;       // starting resolution
int resolutionDialAccum = 0;        // how much dial has turned since last step
int lastDial0Value = 0;
bool cyMeshInitialized = false;


float getDialFloat(int d) {
    return (float)d;
}

void updateCalabiYauMesh(int resolution)
{
   
    if (resolution < 4) resolution = 4;
      printf("updateCalabriYau: %d\n", resolution);


   currentCYResolution = resolution;
   cyMeshInitialized = true;


    if (!cyCoords) cyCoords = new SoCoordinate3;
    if (!cyFaces) cyFaces = new SoIndexedFaceSet;
    if (!calabiYauRoot) calabiYauRoot = new SoSeparator;

    std::vector<SbVec3f> points;
    std::vector<int> indices;

    for (int u = 0; u < resolution; ++u) {
      for (int v = 0; v < resolution; ++v) {
         float U = u * 2.0f * M_PI / (resolution - 1);
         float V = v * 2.0f * M_PI / (resolution - 1);

         float R = 2.0f + cos(V) + 0.4f * sin(3 * U);
         float x = R * cos(U);
         float y = R * sin(U);
         float z = sin(V) + 0.5f * cos(3 * U);

         points.push_back(SbVec3f(x, y, z));
      }
   }
  

    for (int u = 0; u < resolution - 1; ++u) {
        for (int v = 0; v < resolution - 1; ++v) {
            int idx0 = u * resolution + v;
            int idx1 = idx0 + 1;
            int idx2 = idx0 + resolution;
            int idx3 = idx2 + 1;

            indices.push_back(idx0);
            indices.push_back(idx1);
            indices.push_back(idx2);
            indices.push_back(SO_END_FACE_INDEX);

            indices.push_back(idx1);
            indices.push_back(idx3);
            indices.push_back(idx2);
            indices.push_back(SO_END_FACE_INDEX);
        }
    }

   // ===== Finalize geometry (skip if data is bad) =====
   if (!cyCoords || !cyFaces || points.empty() || indices.empty()) return;

   // Set geometry data
   cyCoords->point.setValues(0, points.size(), &points[0]);
   cyFaces->coordIndex.setValues(0, indices.size(), &indices[0]);

   // === Per-vertex gradient color ===
   SoMaterial *mat = new SoMaterial;
   SbColor *colors = new SbColor[points.size()];

   for (int i = 0; i < points.size(); ++i) {
      float z = points[i][2];  // Use Z height for gradient
      float c = (z + 1.0f) * 0.5f;  // Normalize to [0,1]
      colors[i] = SbColor(c * 0.8f, 0.2f + c * 0.6f, 1.0f - c);  // purple-orange shift
   }
   mat->diffuseColor.setValues(0, points.size(), colors);

   SoMaterialBinding *matBind = new SoMaterialBinding;
   matBind->value = SoMaterialBinding::PER_VERTEX_INDEXED;

   // === Draw styles ===
   SoDrawStyle *solidStyle = new SoDrawStyle;
   solidStyle->style = SoDrawStyle::FILLED;

   SoDrawStyle *wireStyle = new SoDrawStyle;
   wireStyle->style = SoDrawStyle::LINES;
   wireStyle->lineWidth = 1;

   // === Lighting ===
   SoDirectionalLight *light = new SoDirectionalLight;
   light->direction.setValue(SbVec3f(-0.5f, -1.0f, -0.8f));
   light->intensity.setValue(1.0f);

   // === Build scene ===
   calabiYauRoot->removeAllChildren();
   calabiYauRoot->addChild(light);
   calabiYauRoot->addChild(cyScale);

   // solid mesh branch
   SoSeparator *solidBranch = new SoSeparator;
   solidBranch->addChild(mat);
   solidBranch->addChild(matBind);
   solidBranch->addChild(solidStyle);
   solidBranch->addChild(cyCoords);
   solidBranch->addChild(cyFaces);

   // wireframe overlay branch
   SoSeparator *wireBranch = new SoSeparator;
   wireBranch->addChild(wireStyle);
   wireBranch->addChild(cyCoords);
   wireBranch->addChild(cyFaces);

   // Add both branches
   calabiYauRoot->addChild(solidBranch);
   calabiYauRoot->addChild(wireBranch);

   delete[] colors;  // cleanup

}

void animateCYRebuild(int newRes)
{
    if (!cyScale) {
        cyScale = new SoTransform;
        cyScale->scaleFactor.setValue(1.0f, 1.0f, 1.0f);
    }

    cyScale->scaleFactor.setValue(0.01f, 0.01f, 0.01f);  // shrink briefly
    updateCalabiYauMesh(newRes);
    cyScale->scaleFactor.setValue(1.0f, 1.0f, 1.0f);
}

void dialCB(void *userData, SoEventCallback *cb)
{
    int mode = 0, i = 0;
    float raw = 0.0f, hue = 0.0f, fade = 0.0f, q = 0.0f, val = 0.0f;
    float r = 0.0f, g = 0.0f, b = 0.0f;

    const DialEvent *ev = (const DialEvent *) cb->getEvent();
    int dialIndex = ev->getDial();
    if (dialIndex < 0 || dialIndex >= 8) return;
    printf("Dial %d turned, value = %d\n", dialIndex, ev->getValue());

    if (!cyMaterial || !cyScale || !calabiYauRoot) return;

    switch (dialIndex) {
         case 0: {
               int raw = ev->getValue();
               int delta = raw - lastDial0Value;
               lastDial0Value = raw;
         
               resolutionDialAccum += delta;
         
               while (resolutionDialAccum >= 10) {
                  if (currentCYResolution < 60) {
                     currentCYResolution++;
                     updateCalabiYauMesh(currentCYResolution);
                  }
                  resolutionDialAccum -= 10;
               }
         
               while (resolutionDialAccum <= -10) {
                  if (currentCYResolution > 10) {
                     currentCYResolution--;
                     updateCalabiYauMesh(currentCYResolution);
                  }
                  resolutionDialAccum += 10;
               }
               break;
         }

        case 1: {
            raw = getDialFloat(ev->getValue());
            hue = fmod(fabs(raw * 0.005f), 1.0f);
            i = int(hue * 6);
            fade = hue * 6 - i;
            q = 1 - fade;

            switch (i % 6) {
                case 0: r=1; g=fade; b=0; break;
                case 1: r=q; g=1; b=0; break;
                case 2: r=0; g=1; b=fade; break;
                case 3: r=0; g=q; b=1; break;
                case 4: r=fade; g=0; b=1; break;
                case 5: r=1; g=0; b=q; break;
            }

            cyMaterial->diffuseColor.setValue(r * 0.6f, g * 0.6f, b * 0.6f);
            break;
        }

        case 2: {
            val = getDialFloat(ev->getValue());
            mode = int(fabs(val / 20)) % 3;

            switch (mode) {
                case 0: // matte
                    cyMaterial->ambientColor.setValue(0.1f, 0.1f, 0.1f);
                    cyMaterial->specularColor.setValue(0.0f, 0.0f, 0.0f);
                    cyMaterial->shininess.setValue(0.1f);
                    break;
                case 1: // metallic
                    cyMaterial->ambientColor.setValue(0.2f, 0.2f, 0.2f);
                    cyMaterial->specularColor.setValue(0.9f, 0.9f, 0.9f);
                    cyMaterial->shininess.setValue(1.0f);
                    break;
                case 2: // glow
                    cyMaterial->ambientColor.setValue(0.4f, 0.2f, 0.6f);
                    cyMaterial->emissiveColor.setValue(0.2f, 0.1f, 0.3f);
                    cyMaterial->specularColor.setValue(0.4f, 0.2f, 0.6f);
                    cyMaterial->shininess.setValue(0.2f);
                    break;
            }
            break;
        }
    }
}

SoSeparator *buildSceneGraph()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    // Initialize Calabi–Yau root separator
    if (!calabiYauRoot)
        calabiYauRoot = new SoSeparator;

    // Initialize material
    if (!cyMaterial) {
        cyMaterial = new SoMaterial;
        cyMaterial->diffuseColor.setValue(0.3f, 0.2f, 0.7f);  // starter color
    }

    // Initialize transform
    if (!cyScale) {
        cyScale = new SoTransform;
        cyScale->scaleFactor.setValue(1.0f, 1.0f, 1.0f);
    }

    // Initialize coordinate and face nodes
    if (!cyCoords) cyCoords = new SoCoordinate3;
    if (!cyFaces)  cyFaces  = new SoIndexedFaceSet;

    // Build the mesh once with default resolution
    currentCYResolution = 40;
    updateCalabiYauMesh(currentCYResolution);

    // Add calabiYauRoot to the main scene
    root->addChild(calabiYauRoot);

    // Set up event handling
    SoEventCallback *cb = new SoEventCallback;
    cb->addEventCallback(DialEvent::getClassTypeId(), dialCB, NULL);
    root->addChild(cb);

    return root;
}


void
main(int , char *argv[])
{
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
   vwr->setHeadlight(TRUE);  // we supply our own light

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
