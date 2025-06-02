///////////////////////////////////////////////////////////////////////////////
//
// TransportButton.h
//
//    An XmPushButton that lights up a supplied pixmap, suitable for transport
//    controls.  This is mostly a steal from VkIconButton which doesn't seem to 
//    be supported yet -- pw.
//
//
// Copyright 1995, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
///////////////////////////////////////////////////////////////////////////////

#ifndef _DMEDIA_TRANSPORT_BUTTON
#define _DMEDIA_TRANSPORT_BUTTON

#include <Vk/VkComponent.h>

Pixel ColorNameToPixel(Display*, char*);

class TransportButton : public VkComponent
{
  
public:

    TransportButton(char* name, Widget, char **, Boolean showLabel = TRUE,
		    char* selectColorName = NULL);
    ~TransportButton();

    virtual void select();
    virtual void deselect();
    Boolean selected() { return _selected; }
    virtual const char* className();

protected:

    static void enterHandler(Widget, XtPointer, XEvent *, Boolean *);
    static void leaveHandler(Widget, XtPointer, XEvent *, Boolean *);

    void enter();
    void leave();

    Pixmap _pixmap, _highlitePixmap, _selectPixmap, _selectHighlitePixmap, _stipplePixmap;
    Pixel _hilite, _bg, _selectColor;
    Boolean _selected, _lit;

    static XtResource _resources[];
};

#endif
