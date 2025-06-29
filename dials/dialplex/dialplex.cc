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
SoMaterial *dialMaterials[8];

// Stub: current set index (replace with real logic later)
int currentSetIndex = 0;

SoSeparator *overlayRoot = NULL;
SoXtExaminerViewer *overlayViewer = NULL;

bool overlayVisible = false;

// Button box layout: 6 rows, variable columns
const int rowCols[6] = {4, 6, 6, 6, 6, 4};

struct ButtonBoxPos { int row, col; };
// Map each button index (0-31) to its (row, col) in the grid
const ButtonBoxPos buttonBoxMap[32] = {
    // Row 0 (4 cols)
    {0,0},{0,1},{0,2},{0,3},
    // Row 1 (6 cols)
    {1,0},{1,1},{1,2},{1,3},{1,4},{1,5},
    // Row 2 (6 cols)
    {2,0},{2,1},{2,2},{2,3},{2,4},{2,5},
    // Row 3 (6 cols)
    {3,0},{3,1},{3,2},{3,3},{3,4},{3,5},
    // Row 4 (6 cols)
    {4,0},{4,1},{4,2},{4,3},{4,4},{4,5},
    // Row 5 (4 cols)
    {5,0},{5,1},{5,2},{5,3}
};

void updateOverlaySceneGraph() {
    if (!overlayViewer) return;
    if (overlayRoot) overlayRoot->unref();
    overlayRoot = new SoSeparator;
    overlayRoot->ref();
    if (!overlayVisible) {
        overlayViewer->setOverlaySceneGraph(overlayRoot);
        return;
    }
    
    // Get window size (default to 1280x1024 if not available)
    SbVec2s winSize(1280, 1024);
    if (globalViewer) winSize = globalViewer->getSize();
    float winW = winSize[0];
    float winH = winSize[1];
    
    // Camera for overlay
    SoOrthographicCamera *cam = new SoOrthographicCamera;
    cam->position.setValue(winW/2, winH/2, 5);
    cam->height.setValue(winH);
    cam->nearDistance.setValue(1);
    cam->farDistance.setValue(10);
    overlayRoot->addChild(cam);

    // Grid parameters - centered on screen with more padding
    float cellSize = 80.0f;
    float gridWidth = 6 * cellSize; // max 6 columns
    float gridHeight = 6 * cellSize; // 6 rows
    float startX = (winW - gridWidth) / 2.0f;
    float startY = (winH - gridHeight) / 2.0f + 80.0f;

    // Draw button grid using line thickness to show state
    for (int btnIdx = 0; btnIdx < 32; ++btnIdx) {
        int row = buttonBoxMap[btnIdx].row;
        int col = buttonBoxMap[btnIdx].col;
        int cols = rowCols[row];
        float rowWidth = cols * cellSize;
        float rowStartX = startX + (gridWidth - rowWidth) / 2.0f;
        float x = rowStartX + col * cellSize;
        float y = startY + (5 - row) * cellSize; // flip Y
        
        // Use line thickness to show button state
        SoDrawStyle *style = new SoDrawStyle;
        if (btnIdx == currentSetIndex) {
            style->lineWidth.setValue(8.0f); // thick when pressed
        } else {
            style->lineWidth.setValue(2.0f); // thin when not pressed
        }
        overlayRoot->addChild(style);
        
        // Draw square outline
        SoCoordinate3 *coords = new SoCoordinate3;
        coords->point.set1Value(0, SbVec3f(x, y, 0));
        coords->point.set1Value(1, SbVec3f(x+cellSize, y, 0));
        coords->point.set1Value(2, SbVec3f(x+cellSize, y+cellSize, 0));
        coords->point.set1Value(3, SbVec3f(x, y+cellSize, 0));
        coords->point.set1Value(4, SbVec3f(x, y, 0));
        overlayRoot->addChild(coords);
        
        SoLineSet *lines = new SoLineSet;
        lines->numVertices.set1Value(0, 5);
        overlayRoot->addChild(lines);
        
        // Add an X pattern if button is pressed
        if (btnIdx == currentSetIndex) {
            SoCoordinate3 *xCoords = new SoCoordinate3;
            xCoords->point.set1Value(0, SbVec3f(x+10, y+10, 0));
            xCoords->point.set1Value(1, SbVec3f(x+cellSize-10, y+cellSize-10, 0));
            xCoords->point.set1Value(2, SbVec3f(x+cellSize-10, y+10, 0));
            xCoords->point.set1Value(3, SbVec3f(x+10, y+cellSize-10, 0));
            overlayRoot->addChild(xCoords);
            
            SoLineSet *xLines = new SoLineSet;
            xLines->numVertices.set1Value(0, 2);
            xLines->numVertices.set1Value(1, 2);
            overlayRoot->addChild(xLines);
        }
    }

    // Left side text overlay - Detailed dial descriptions  
    SoSeparator *leftSep = new SoSeparator;
    SoFont *leftFont = new SoFont;
    leftFont->name.setValue("Courier");
    leftFont->size.setValue(12.0f);
    leftSep->addChild(leftFont);
    
    SoTransform *leftTextPos = new SoTransform;
    leftTextPos->translation.setValue(30, winH - 50, 0);
    leftSep->addChild(leftTextPos);
    
    SoText2 *leftText = new SoText2;
    leftText->string.set1Value(0, "DIAL FUNCTION MATRIX");
    leftText->string.set1Value(1, "====================");
    if (currentSetIndex >= 0) {
        SbString setName("ACTIVE SET: ");
        setName += SbString(currentSetIndex + 1);
        leftText->string.set1Value(2, setName.getString());
    } else {
        leftText->string.set1Value(2, "ACTIVE SET: NONE");
    }
    leftText->string.set1Value(3, "");
    
    leftText->string.set1Value(4, " .--.  DIAL 0: BACKGROUND HUE");
    leftText->string.set1Value(5, "|BG  | Controls the ambient color spectrum.");
    leftText->string.set1Value(6, "|HUE | Shifts through deep cosmic hues.");
    leftText->string.set1Value(7, " '--'");
    leftText->string.set1Value(8, "");
    
    leftText->string.set1Value(9, " .--.  DIAL 1: SCENE ROTATE Y");
    leftText->string.set1Value(10, "|ROT | Rotates entire scene around Y axis.");
    leftText->string.set1Value(11, "| Y  | Provides orbital perspective control.");
    leftText->string.set1Value(12, " '--'");
    leftText->string.set1Value(13, "");
    
    leftText->string.set1Value(14, " .--.  DIAL 2: ZOOM DEPTH");
    leftText->string.set1Value(15, "|ZOOM| Controls camera distance to objects.");
    leftText->string.set1Value(16, "|DPTH| Dive deep or pull back for overview.");
    leftText->string.set1Value(17, " '--'");
    leftText->string.set1Value(18, "");
    
    leftText->string.set1Value(19, " .--.  DIAL 3: SCENE ROTATE X");
    leftText->string.set1Value(20, "|ROT | Tilts scene up and down on X axis.");
    leftText->string.set1Value(21, "| X  | Perfect for dramatic angle shifts.");
    leftText->string.set1Value(22, " '--'");
    leftText->string.set1Value(23, "");
    
    leftText->string.set1Value(24, " .--.  DIAL 4: CYLINDER HEIGHT");
    leftText->string.set1Value(25, "|CYL | Extends all dial cylinders upward.");
    leftText->string.set1Value(26, "|HGT | Creates towering control structures.");
    leftText->string.set1Value(27, " '--'");
    leftText->string.set1Value(28, "");
    
    leftText->string.set1Value(29, " .--.  DIAL 5: SCENE ROTATE Z");
    leftText->string.set1Value(30, "|ROT | Spins scene around Z depth axis.");
    leftText->string.set1Value(31, "| Z  | Adds rolling motion dynamics.");
    leftText->string.set1Value(32, " '--'");
    leftText->string.set1Value(33, "");
    
    leftText->string.set1Value(34, " .--.  DIAL 6: SCATTER CHAOS");
    leftText->string.set1Value(35, "|SCAT| Randomly displaces all objects.");
    leftText->string.set1Value(36, "|CHAO| Creates organic movement patterns.");
    leftText->string.set1Value(37, " '--'");
    leftText->string.set1Value(38, "");
    
    leftText->string.set1Value(39, " .--.  DIAL 7: BLOOM EFFECT");
    leftText->string.set1Value(40, "|BLOOM Radiates objects outward from center.");
    leftText->string.set1Value(41, "|EFCT| Transforms materials to electric blue.");
    leftText->string.set1Value(42, " '--'");
    
    leftSep->addChild(leftText);
    overlayRoot->addChild(leftSep);

    // Right side text overlay - Project documentation and cassette futurism
    SoSeparator *rightSep = new SoSeparator;
    SoFont *rightFont = new SoFont;
    rightFont->name.setValue("Courier");
    rightFont->size.setValue(12.0f);
    rightSep->addChild(rightFont);
    
    SoTransform *rightTextPos = new SoTransform;
    rightTextPos->translation.setValue(winW - 380, winH - 50, 0);
    rightSep->addChild(rightTextPos);
    
    SoText2 *rightText = new SoText2;
    rightText->string.set1Value(0, "DIALPLEX PROJECT OVERVIEW");
    rightText->string.set1Value(1, "==========================");
    if (currentSetIndex >= 0) {
        SbString btnText("ACTIVE SET: ");
        btnText += SbString(currentSetIndex + 1);
        rightText->string.set1Value(2, btnText.getString());
        rightText->string.set1Value(3, "STATUS: ENGAGED");
    } else {
        rightText->string.set1Value(2, "ACTIVE SET: STANDBY");
        rightText->string.set1Value(3, "STATUS: MONITORING");
    }
    rightText->string.set1Value(4, "");
    rightText->string.set1Value(5, "PROJECT ARCHITECTURE:");
    rightText->string.set1Value(6, "====================");
    rightText->string.set1Value(7, "This system demonstrates a modular");
    rightText->string.set1Value(8, "dial control interface using SGI");
    rightText->string.set1Value(9, "Open Inventor and hardware button");
    rightText->string.set1Value(10, "box input. The core innovation is");
    rightText->string.set1Value(11, "breaking dial functionality into");
    rightText->string.set1Value(12, "32 distinct function sets, each");
    rightText->string.set1Value(13, "triggered by a physical button.");
    rightText->string.set1Value(14, "");
    rightText->string.set1Value(15, "IMPLEMENTATION DETAILS:");
    rightText->string.set1Value(16, "========================");
    rightText->string.set1Value(17, "Eight dials control different");
    rightText->string.set1Value(18, "aspects of the 3D scene: camera");
    rightText->string.set1Value(19, "rotation, zoom, object transforms,");
    rightText->string.set1Value(20, "material properties, and visual");
    rightText->string.set1Value(21, "effects. Button presses activate");
    rightText->string.set1Value(22, "this overlay interface, providing");
    rightText->string.set1Value(23, "real-time feedback and function");
    rightText->string.set1Value(24, "reference. The result: 256 unique");
    rightText->string.set1Value(25, "control combinations (32 x 8).");
    rightText->string.set1Value(26, "");
    rightText->string.set1Value(27, "SYSTEM STATUS:");
    rightText->string.set1Value(28, "===============");
    rightText->string.set1Value(29, " DIALS: [||||||||] 8/8 ACTIVE");
    rightText->string.set1Value(30, " BTNS:  [||||||||] 32/32 READY");
    rightText->string.set1Value(31, " OVLY:  [||||||||] FUNCTIONAL");
    rightText->string.set1Value(32, " REND:  [||||||||] 60 FPS");
    rightText->string.set1Value(33, "");
    rightText->string.set1Value(34, "CASSETTE FUTURISM MODE: ACTIVE");
    rightText->string.set1Value(35, "RETRO INTERFACE: ENGAGED");
    rightText->string.set1Value(36, "NEURAL LINK: ESTABLISHED");
    
    rightSep->addChild(rightText);
    overlayRoot->addChild(rightSep);

    // Bottom center - Additional futuristic elements
    SoSeparator *bottomSep = new SoSeparator;
    SoFont *bottomFont = new SoFont;
    bottomFont->name.setValue("Courier");
    bottomFont->size.setValue(13.0f);
    bottomSep->addChild(bottomFont);
    
    SoTransform *bottomTextPos = new SoTransform;
    bottomTextPos->translation.setValue(winW/2 - 350, 100, 0);
    bottomSep->addChild(bottomTextPos);
    
    SoText2 *bottomText = new SoText2;
    bottomText->string.set1Value(0, "CYBERDYNE NEURAL NET PROCESSOR - 256 FUNCTION COMBINATIONS");
    bottomText->string.set1Value(1, "================================================================");
    bottomText->string.set1Value(2, "PRESS ANY BUTTON TO ACTIVATE FUNCTION SET | RELEASE TO RETURN TO STANDBY");
    bottomSep->addChild(bottomText);
    overlayRoot->addChild(bottomSep);

    overlayViewer->setOverlaySceneGraph(overlayRoot);
}

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
    const ButtonBoxEvent *ev = (const ButtonBoxEvent *) cb->getEvent();
    if (ev->getState() == SoButtonEvent::DOWN) {
        overlayVisible = true;
        int which = ev->getButton();
        printf("button %d pressed\n", which);
        if (which >= 1 && which <= 32) {
            currentSetIndex = which - 1;
            if (globalViewer) {
                globalViewer->render();
            }
        }
        updateOverlaySceneGraph();
    } else if (ev->getState() == SoButtonEvent::UP) {
        overlayVisible = false;
        updateOverlaySceneGraph();
        if (globalViewer) {
            globalViewer->render();
        }
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
                     float strength = 0.6f * sinf(dialAngles[6]);

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
                     float t = dialAngles[7];

                     float radius = t * 0.5f;
                     SbVec3f newOffset = bloomVectors[i] * radius;
                     SbVec3f delta = newOffset - lastScatter7[i];
                     lastScatter7[i] = newOffset;

                     SbVec3f current = scatterOffsets[i]->translation.getValue();
                     scatterOffsets[i]->translation.setValue(current + delta);

                     if (dialMaterials[i]) {
                        float absRadius = (radius < 0.0f) ? -radius : radius;
                        float norm = (absRadius < 3.5f) ? (absRadius / 3.5f) : 1.0f;
                        float fade = norm * norm;

                        // Strong transition from plastic → electric blue steel
                        float r = 0.6f * (1.0f - fade) + 0.1f * fade;
                        float g = 0.6f * (1.0f - fade) + 0.2f * fade;
                        float b = 0.6f * (1.0f - fade) + 1.0f * fade;

                        // Specular color morphs to white highlight
                        float spec = 0.0f * (1.0f - fade) + 1.0f * fade;

                        // Increase shininess rapidly — gives tight reflection
                        float shine = 0.0f * (1.0f - fade) + 1.0f * fade;

                        dialMaterials[i]->diffuseColor.setValue(r, g, b);
                        dialMaterials[i]->specularColor.setValue(spec, spec, spec);
                        dialMaterials[i]->shininess.setValue(shine);

                     }
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
   // Set cylinder detail level high
   SoComplexity *complexity = new SoComplexity;
   complexity->value.setValue(1.0f);  // full smoothness
   complexity->type.setValue(SoComplexity::OBJECT_SPACE);
   sep->addChild(complexity);

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
      SoMaterial *mat = new SoMaterial;
      mat->diffuseColor.setValue(0.2f, 0.25f, 0.35f);    // cool steel blue
      mat->specularColor.setValue(0.4f, 0.4f, 0.4f);     // soft highlight
      mat->shininess.setValue(0.6f);                    // semi-shiny surface
      mat->emissiveColor.setValue(0.03f, 0.03f, 0.05f);  // slight glow for depth

      potentiometer->setPart("material", mat);
      dialMaterials[x] = mat;  // store if still using dialMaterials[]

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
   cb->addEventCallback(DialEvent::getClassTypeId(), dialCB, NULL); // We'll look up the transform via dial ID
   cb->addEventCallback(ButtonBoxEvent::getClassTypeId(), buttonBoxCB, NULL);

   sep->addChild(zoomSep);
   return sep;
}

int
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
   overlayViewer = vwr;
   vwr->setSceneGraph(buildSceneGraph());
   vwr->setTitle("Dial Open Inventor Demo");
   vwr->setViewing(FALSE);   // come up in pick mode
   vwr->setHeadlight(TRUE); // we supply our own light

   // window settings
   vwr->setSize(SbVec2s(1280, 1024));
   vwr->setBackgroundColor(SbColor(0.1f, 0.1f, 0.1f));
   vwr->setDecoration(FALSE);

   updateOverlaySceneGraph();

   DialNButton *db = new DialNButton;
   vwr->registerDevice(db);
   
   vwr->show();
   SoXt::show(mainWindow);
   SoXt::mainLoop();
   
   return 0;
}
