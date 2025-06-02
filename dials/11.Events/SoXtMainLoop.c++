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

/*-------------------------------------------------------------
 *  This is an example from The Inventor Toolmaker,
 *  chapter 11.
 *
 *  This is the source code for SoXt::mainLoop().
 *------------------------------------------------------------*/

void
SoXt::mainLoop()
{
    XtAppContext context = getAppContext();
    XEvent event;
    
    for (;;) {
        SoXt::nextEvent(context, &event);
        SoXt::dispatchEvent(&event);
    };
}

void
SoXt::nextEvent(XtAppContext appContext, XEvent *event)
{
    XtAppNextEvent(appContext, event);
}

Boolean
SoXt::dispatchEvent(XEvent *event)
{
    Boolean success = True;
     
    if(event->type >= LASTEvent) {
        XtEventHandler proc;
        XtPointer clientData;
        Widget w;
        Boolean dummy;
        
        // Get the event handling function which was
        // registered with Inventor for handling this
        // event type in this widget
        SoXt::getExtensionEventHandler(event, w, proc, clientData);
        
        // Call the event handler!
        if (proc == NULL)
            success = False;
        else
            (*proc) (w, clientData, event, &dummy);
        
    }
    
    // else it is not an extension event - let Xt dispatch it
    else success = XtDispatchEvent(event);
    
    return success;
}
