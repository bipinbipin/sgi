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
// to change a transformation node. Spaceball events
// are passed via a callback on an SoEventCallback node.
//

#include <X11/Intrinsic.h>

#include <Inventor/So.h>
#include <Inventor/nodes/SoEventCallback.h>
#include <Inventor/events/SoEvents.h>
#include <Inventor/Xt/SoXt.h>
#include <Inventor/Xt/devices/SoXtSpaceball.h>
#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>

static void
eventCB(void *userData, SoEventCallback *cb)
{
    const SoEvent *event = cb->getEvent();
    SoTransform *xform = (SoTransform *) userData;
    
    if (event->isOfType(SoMotion3Event::getClassTypeId())) {
	const SoMotion3Event *motion = (const SoMotion3Event *) event;
	
	xform->translation.setValue(
	    xform->translation.getValue() + motion->getTranslation().getValue());
	
	xform->rotation.setValue(
	    xform->rotation.getValue() * motion->getRotation().getValue());
    }
    
    else if (SO_SPACEBALL_PRESS_EVENT(event, PICK)) {
	// reset!
	xform->rotation.setValue(0, 0, 0, 1);
	xform->translation.setValue(0, 0, 0);
    }
}

static SoNode *
setupTest()
{
    SoSeparator *root = new SoSeparator;
    SoTransform *xform = new SoTransform;
    SoEventCallback *cbNode = new SoEventCallback;
    
    // camera and lights will be created by viewer
    root->ref();
    root->addChild(xform);
    root->addChild(new SoCube);
    root->addChild(cbNode);
    
    cbNode->addEventCallback(SoMotion3Event::getClassTypeId(), eventCB, xform);  
    cbNode->addEventCallback(SoSpaceballButtonEvent::getClassTypeId(),
			     eventCB, xform);  
    
    return root;
}

void
usage()
{
    printf("The spaceball can be used to translate and rotate the cube.\n");
    printf("Hit the spaceball pick button (located on the ball itself)\n");
    printf("to reset the transform affecting the cube.\n");
}

void
main(unsigned int, char *argv[])
{
    Widget mainWindow = SoXt::init(argv[0]);

    if (! SoXtSpaceball::exists()) {
	printf("Could not find a spaceball device connected to this display\n");
	return;
    }
    
    usage();    
    
    // build and initialize the viewer
    SoXtExaminerViewer *viewer = new SoXtExaminerViewer(mainWindow);
    viewer->setSceneGraph(setupTest());
    viewer->setViewing(FALSE); // so events will pass through to us!
    
    // add spaceball device
    SoXtSpaceball *spaceball = new SoXtSpaceball;
    viewer->registerDevice(spaceball);
    
    // display the main window
    viewer->show();
    SoXt::show(mainWindow);
    
    // loop forever
    SoXt::mainLoop();
}

