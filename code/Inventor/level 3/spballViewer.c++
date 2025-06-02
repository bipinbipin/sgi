/*
 * Copyright (c) 1991-95 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the name of Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SILICON GRAPHICS BE LIABLE FOR ANY SPECIAL, INCIDENTAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND, OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER OR NOT ADVISED OF THE
 * POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF LIABILITY, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

//
// This demonstrates using the spaceball device
// to change a camera position and orientation.
// The spaceball motion events are delivered via
// a callback on the viewer.
//

#include <X11/Intrinsic.h>

#include <Inventor/So.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/events/SoEvents.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/devices/SoXtMouse.h>
#include <Inventor/Xt/devices/SoXtKeyboard.h>
#include <Inventor/Xt/devices/SoXtSpaceball.h>
#include <Inventor/Xt/viewers/SoXtWalkViewer.h>

// global devices
SoXtSpaceball *spaceball = NULL;

static SbBool
processSpballEvents(void *userData, XAnyEvent *anyevent)
{
    const SoEvent *event = spaceball->translateEvent(anyevent);
    SoXtWalkViewer *viewer = (SoXtWalkViewer *) userData;
    SoCamera *camera = viewer->getCamera();
    
    if (event != NULL) {
	if (event->isOfType(SoMotion3Event::getClassTypeId())) {
	    const SoMotion3Event *motion = (const SoMotion3Event *) event;
	    
	    // move the camera position by the translation amount
	    // in the forward direction.
	    // we will ignore 'y' motion since this is the walk viewer.
	    // first, orient the spaceball data to our camera orientation.
	    float x, y, z;
	    motion->getTranslation().getValue(x, y, z);

x = (x/4); y = (y/4); z = (z/4);

	    SbVec3f t(x, 0, z), xlate;
	    
	    SbMatrix m;
	    m.setRotate(camera->orientation.getValue());
	    m.multVecMatrix(t, xlate);
	    
	    // now update the position
	    camera->position.setValue(camera->position.getValue() + xlate);
	
	    // change the orientation of the camera about the up direction.
	    // ignore 'x' and 'z' rotation since this is the walk viewer.
	    // first, get the spaceball rotation as an axis/angle.
	    SbVec3f axis, newAxis;
	    float angle;
	    motion->getRotation().getValue(axis, angle);
	    
	    // ignore x and z
	    axis *= angle;
	    axis.getValue(x, y, z);
	    axis.setValue(0, y, 0);
	    
	    // reset axis and angle to only be about 'y'
	    angle = axis.length();
	    axis.normalize();
	    
	    // orient the spaceball rotation to our camera orientation.
	    m.multVecMatrix(axis, newAxis);
	    SbRotation rot(newAxis, angle);
	
	    // now update the orientation
	    camera->orientation.setValue(camera->orientation.getValue() * rot);
	}
	
	else if (SO_SPACEBALL_PRESS_EVENT(event, PICK)) {
	    // reset!
	    viewer->resetToHomePosition();
	}
    }
    
    return (event != NULL); // return whether we handled the event or not
}

static SoNode *
setupTest(const char *filename)
{
    SoInput	in;
    SoNode	*node;
    SoSeparator *root = new SoSeparator;
    SbBool	ok;

    root->ref();
    if (in.openFile(filename)) {
	while ((ok = SoDB::read(&in, node)) != FALSE && node != NULL)
	    root->addChild(node);
    
	if (! ok) {
	    fprintf(stderr, "Bad data. Bye!\n");
	    return NULL;
	}
	if (root->getNumChildren() == 0) {
	    fprintf(stderr, "No data read. Bye!\n");
	    return NULL;
	}
    
	in.closeFile();
    }
    else printf("Error opening file %s\n", filename);

    return root;
}

void
msg()
{
    printf("The spaceball can be used to translate and rotate the camera.\n");
    printf("Hit the spaceball pick button (located on the ball itself)\n");
    printf("to reset the camera to its home position.\n");
}

void
main(unsigned int argc, char *argv[])
{
    if (argc != 2) {
	printf("Usage: %s filename\n", argv[0]);
	printf("(A good file to walk through is /usr/share/data/models/buildings/Barcelona.iv)\n");
	exit(0);
    }
    
    Widget mainWindow = SoXt::init(argv[0]);

    if (! SoXtSpaceball::exists()) {
	printf("Could not find a spaceball device connected to this display\n");
	return;
    }
    
    msg();
    
    if (mainWindow != NULL) {
	// build and initialize the inventor render area widget
	SoXtWalkViewer *viewer = new SoXtWalkViewer(mainWindow);
    	viewer->setSceneGraph(setupTest(argv[1]));

	// handle spaceball 
	spaceball = new SoXtSpaceball;
	viewer->registerDevice(spaceball);
	viewer->setEventCallback(processSpballEvents, viewer); // viewer is userData
	
	// display the main window
        viewer->show();
	SoXt::show(mainWindow);

	// loop forever
	SoXt::mainLoop();
    }
}

