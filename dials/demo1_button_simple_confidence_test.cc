// CC -g -n32 -I/usr/include/Inventor -I/usr/include/X11 -c demo1_button_game.cc && CC demo1_button_game.o -L/usr/lib32 -lInventorInput -lInventorXt -lInventor -lX11 -lGL -lGLU -lm -o demo1_button_game

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

SoMaterial *buttonMaterials[32];
SoTransform *buttonScales[32];

void buttonBoxCB(void *userData, SoEventCallback *cb)
{
    const ButtonBoxEvent *ev = (const ButtonBoxEvent *) cb->getEvent();
    if (!ev) return;

    int idx = ev->getButton() - 1;
    if (idx < 0 || idx >= 32) return;

    if (ev->getState() == SoButtonEvent::DOWN) {
        // Highlight: scale up and change color
        if (buttonScales[idx])
            buttonScales[idx]->scaleFactor.setValue(1.2f, 1.2f, 1.2f);

        if (buttonMaterials[idx]) {
            buttonMaterials[idx]->diffuseColor.setValue(0.8f, 0.7f, 0.2f);    // metallic yellow
            buttonMaterials[idx]->specularColor.setValue(1.0f, 1.0f, 0.6f);
            buttonMaterials[idx]->shininess.setValue(1.0f);
        }

        printf("Button %d pressed\n", idx);
    }
    else if (ev->getState() == SoButtonEvent::UP) {
        // Restore: scale and material
        if (buttonScales[idx])
            buttonScales[idx]->scaleFactor.setValue(1.0f, 1.0f, 1.0f);

        if (buttonMaterials[idx]) {
            buttonMaterials[idx]->diffuseColor.setValue(0.1f, 0.15f, 0.3f);    // back to dark blue
            buttonMaterials[idx]->specularColor.setValue(0.5f, 0.6f, 0.9f);
            buttonMaterials[idx]->shininess.setValue(0.95f);
        }

        printf("Button %d released\n", idx);
    }
}

SoNode *
buildSceneGraph_ButtonsOnly()
{
    SoSeparator *root = new SoSeparator;
    root->ref();

    SoDirectionalLight *light = new SoDirectionalLight;
    light->direction.setValue(SbVec3f(-0.5f, -1.0f, -1.0f));
    light->intensity.setValue(1.0f);  // full brightness
    root->addChild(light);

    SoComplexity *complexity = new SoComplexity;
    complexity->value.setValue(1.0f);
    complexity->type.setValue(SoComplexity::OBJECT_SPACE);
    root->addChild(complexity);

    SoEventCallback *cb = new SoEventCallback;
    root->addChild(cb);

    const float spacingX = 1.6f;
    const float spacingY = 1.6f;

    struct ButtonRow {
        int count;
        float xOffset;
    };

    ButtonRow layout[] = {
        {4, -1.5f * spacingX},  // Row 0
        {6, -2.5f * spacingX},  // Row 1
        {6, -2.5f * spacingX},  // Row 2
        {6, -2.5f * spacingX},  // Row 3
        {6, -2.5f * spacingX},  // Row 4
        {4, -1.5f * spacingX},  // Row 5
    };

    int totalRows = sizeof(layout) / sizeof(layout[0]);
    int buttonIndex = 0;

    for (int row = 0; row < totalRows; row++) {
        int count = layout[row].count;
        float offsetX = layout[row].xOffset;
        float y = (totalRows / 2.0f - row) * spacingY;

        for (int col = 0; col < count; col++) {
            if (buttonIndex >= 32) break;

            float x = offsetX + col * spacingX;

            SoTransform *pos = new SoTransform;
            pos->translation.setValue(x, y, 0);

            SoTransform *scale = new SoTransform;
            scale->scaleFactor.setValue(1.0f, 1.0f, 1.0f);
            buttonScales[buttonIndex] = scale;

            SoMaterial *mat = new SoMaterial;
            mat->diffuseColor.setValue(0.1f, 0.15f, 0.3f);     // deeper blue tone
            mat->specularColor.setValue(0.5f, 0.6f, 0.9f);     // strong cool shine
            mat->shininess.setValue(0.95f);                   // tight specular highlight            
            buttonMaterials[buttonIndex] = mat;

            SoCube *cube = new SoCube;
            cube->width = 1.0f;
            cube->height = 1.0f;
            cube->depth = 1.0f;

            SoSeparator *buttonGroup = new SoSeparator;
            buttonGroup->addChild(pos);
            buttonGroup->addChild(scale);
            buttonGroup->addChild(mat);
            buttonGroup->addChild(cube);

            root->addChild(buttonGroup);

            buttonIndex++;
        }
    }

    cb->addEventCallback(ButtonBoxEvent::getClassTypeId(), buttonBoxCB, NULL);
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
   vwr->setSceneGraph(buildSceneGraph_ButtonsOnly());
   vwr->setTitle("Dial Open Inventor Demo");
   vwr->setViewing(FALSE);   // come up in pick mode
   vwr->setHeadlight(TRUE); // we supply our own light

   // window settings
   vwr->setSize(SbVec2s(1280, 1024));
   vwr->setBackgroundColor(SbColor(0.05f, 0.07f, 0.15f));  // dark navy blue
   vwr->setDecoration(FALSE);


   DialNButton *db = new DialNButton;
   vwr->registerDevice(db);
   
   vwr->show();
   SoXt::show(mainWindow);
   SoXt::mainLoop();
}
