#include <Inventor/SoDB.h>
#include <Inventor/SoSceneManager.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoRotation.h>
#include <Inventor/Xt/devices/SoXtDevice.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include "DialEvent.h" // Include the DialEvent header to handle dial events

// Event handler for rotating the cube
void rotateCube(const SoEvent *event, SoRotation *cubeRotation)
{
    if (DialEvent::isDialEvent(event, 1)) {
        const DialEvent *dialEvent = (const DialEvent *)event;
        float angle = dialEvent->getValue() * 0.01f; // Scale the dial value
        SbRotation rotation(SbVec3f(0, 1, 0), angle); // Rotate around the Y axis
        cubeRotation->rotation.setValue(rotation);
    }
}

int main(int argc, char **argv)
{
    // Initialize Inventor and Xt
    Widget myWindow = SoXt::init(argv[0]);

    // Create a scene with a rotating cube
    SoSeparator *root = new SoSeparator;
    root->ref();

    // Cube rotation node
    SoRotation *cubeRotation = new SoRotation;
    root->addChild(cubeRotation);

    // Add a cube
    SoCube *cube = new SoCube;
    root->addChild(cube);

    // Set up an examiner viewer to view the scene
    SoXtExaminerViewer *viewer = new SoXtExaminerViewer(myWindow);
    viewer->setSceneGraph(root);
    viewer->show();

    // Enable dial input (using DialNButton class as in Chapter 11)
    DialNButton *dialDevice = new DialNButton;
    dialDevice->enable(myWindow, [](Widget, XtPointer, XEvent *xEvent, Boolean *) {
        SoEvent *event = translateEvent(xEvent); // Translate X event to Inventor event
        rotateCube(event, cubeRotation);          // Handle rotation
    });

    // Main loop
    SoXt::mainLoop();

    // Cleanup
    root->unref();
    delete viewer;
    delete dialDevice;

    return 0;
}