///////////////////////////////////////////////////////////////////////////////
//
// TransportButton.c++
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


#include <Xm/PushB.h>
#include <Vk/VkApp.h>
#include <Vk/VkResource.h>
#include <Vk/xpm.h>
#include "TransportButton.h"

#define MAX(a,b) (a>b ? a : b)
#define PAD 2

///////////////////////////////////////////////////////////////////
// Icon Support Code, with some clean-up could move into viewkit
/////////////////////////////////////////////////////////////////


Pixel getTransportHilight(Widget w, Pixel ts)
{
    // Compute the buffy hilite color. Not really correct, but close
    // We can't call the real buffy routines, because it would mean that
    // we couldn't link with non-toto for now.
    XColor tsColor, highlightColor;
    Display *dpy = XtDisplay(w);
    Screen *scr = XtScreen(w);
    tsColor.pixel = ts;
    XQueryColor(dpy, DefaultColormapOfScreen(scr), &tsColor);
    highlightColor.red   = (int) (tsColor.red   * 0.99);
    highlightColor.green = (int) (tsColor.green * 0.99);
    highlightColor.blue  = (int) (tsColor.blue  * 0.99);
    if(XAllocColor(dpy, DefaultColormapOfScreen(scr), &highlightColor))
 	return highlightColor.pixel;
    return ts;
}


Pixmap CreateXPMTransportPixmap(Widget w, char** description, Pixel fill, Pixel bg, Pixel ds, Pixel ls)
{
    // Create an xpm pixmap for four colors
    // which must have symbolic names
    // "fillcolor"
    // "background"
    // "darkshadow"
    // "lightshadow"
    Pixmap pix;
    XpmAttributes attributes;
    XpmColorSymbol symbols[5];
    symbols[0].name  = "fillcolor";
    symbols[0].value = NULL;
    symbols[0].pixel = fill;
    symbols[1].name  = "background";
    symbols[1].value = NULL;
    symbols[1].pixel = bg;
    symbols[2].name  = "darkShadow";
    symbols[2].value = NULL;
    symbols[2].pixel = ds;
    symbols[3].name  = "lightShadow";
    symbols[3].value = NULL;
    symbols[3].pixel = ls;

    Display *display = XtDisplay(w);
    int      depth = DefaultDepth(display, DefaultScreen(display));
    Visual  *visual =  DefaultVisual(display,
 				     DefaultScreen(display));
    Colormap 	cmap = DefaultColormap(display,
 				       DefaultScreen(display));
    attributes.colorsymbols   = symbols;
    attributes.numsymbols     = 4;
    attributes.depth = depth;
    attributes.visual = visual;
    attributes.colormap = cmap;
    attributes.valuemask = (XpmDepth | XpmVisual | XpmColormap | XpmColorSymbols);

    int status =  XpmCreatePixmapFromData(display, 
 					  RootWindowOfScreen(XtScreen(w)),
 					  description, &pix, 
 					  NULL, 
 					  &attributes);
    if(status)
 	XpmFreeAttributes(&attributes);

    return (pix);
}



extern "C" Boolean _XmFontListGetDefaultFont(XmFontList fontlist,
					     XFontStruct **font_struct );


void TransportButton::enterHandler(Widget, XtPointer clientData, XEvent*, Boolean*)
{
    TransportButton * obj = (TransportButton*) clientData;
    obj->enter();
}

void TransportButton::leaveHandler(Widget, XtPointer clientData, XEvent*, Boolean*)
{
    TransportButton * obj = (TransportButton*) clientData;
    obj->leave();
}


void TransportButton::enter()
{
    // Do the locate hilite thing
    this->_lit = TRUE;
    XtVaSetValues(_baseWidget, 
		  XmNlabelPixmap,
		  this->_selected ? _selectHighlitePixmap : _highlitePixmap, 
		  NULL);
}


void TransportButton::leave()
{
    // Do the unlocate hilite thing
    this->_lit = FALSE;
    XtVaSetValues(_baseWidget, 
		  XmNlabelPixmap, this->_selected ? _selectPixmap : _pixmap, 
		  NULL);
}

void TransportButton::select()
{
    // make purty colors
    this->_selected = TRUE;
    XtVaSetValues(_baseWidget, 
		  XmNlabelPixmap, this->_lit ? _selectHighlitePixmap : _selectPixmap,
		  NULL);
    
}

void TransportButton::deselect()
{
    // make purty colors
    this->_selected = FALSE;
    XtVaSetValues(_baseWidget, 
		  XmNlabelPixmap, this->_lit ? _highlitePixmap : _pixmap,
		  NULL);
}


Pixel ColorNameToPixel(Display* display, char* colorName)
{
    XColor color, unused;
    XAllocNamedColor(display, DefaultColormap(display, DefaultScreen(display)),
		     colorName, &color, &unused);
    return color.pixel;
}

XtResource TransportButton::_resources [] = {
  {
  "selectColor",
  "SelectColor",
  XmRPixel,
  sizeof (Pixel ),
  XtOffset (TransportButton*, _selectColor ),
  XmRString,
  (XtPointer) "green",
  }

};
TransportButton::TransportButton(char* name, Widget parent, char** xpmIcon, Boolean showLabel,
				 char* selectColorName) : VkComponent(name)
{
    XmString label;
    XmFontList font;
    GC gc, inverseGc, highliteGc, stippleGc;
    Dimension width, height;
    unsigned int pixmapWidth, pixmapHeight, bw, d;
    XGCValues values;
    int junk, depth;
    Pixel fg, ts, bs, arm;
    Display* display = XtDisplay(parent);
    Boolean sgiMode =
	(Boolean) (ptrdiff_t) VkGetResource(parent, "sgiMode", "sgiMode", XmRBoolean, FALSE);

    _lit = _selected = FALSE;

    _baseWidget = XmCreatePushButton(parent, _name, NULL, 0);
    installDestroyHandler();

    if (sgiMode) {
	XtAddEventHandler(_baseWidget, EnterWindowMask, FALSE, 
			  &TransportButton::enterHandler, (XtPointer) this);
	XtAddEventHandler(_baseWidget, LeaveWindowMask, FALSE, 
			  &TransportButton::leaveHandler, (XtPointer) this);

	getResources ( _resources, XtNumber(_resources) );

    }

    XtVaGetValues(_baseWidget, 
		  XmNlabelString,     &label,
		  XmNfontList,        &font,
		  XmNforeground,      &values.foreground, 
		  XmNbackground,      &values.background, 
		  XmNtopShadowColor,  &ts, 
		  XmNbottomShadowColor,  &bs, 
		  XmNarmColor,        &arm, 
		  XmNdepth,           &depth, 
		  NULL);


    fg = values.foreground;
    _bg = values.background;

    if (selectColorName)
	_selectColor = ColorNameToPixel(display, selectColorName);

    XFontStruct     *fs = (XFontStruct *) NULL;
    _XmFontListGetDefaultFont(font, &fs);

    values.font = fs->fid;

    gc = XtGetGC(_baseWidget, GCForeground | GCBackground | GCFont, &values);
    

    // Stippling to mark a button as insensitve is a hack which just uses a border
    // color as the foreground.  It works "ok".

    values.foreground = bs;
    stippleGc = XtGetGC(_baseWidget, GCForeground | GCBackground | GCFont, &values);

    // Set up inverse of normal

    values.foreground = values.background;
    inverseGc = XtGetGC(_baseWidget, GCForeground | GCBackground | GCFont, &values);

    values.foreground = fg;
    if (sgiMode)
	_hilite = values.background =  getTransportHilight(_baseWidget, ts);
    else
	_hilite = values.background = arm;

    highliteGc = XtGetGC(_baseWidget, GCForeground | GCBackground | GCFont, &values);
    values.foreground = values.background;
    GC _inverseHiliteGc = XtGetGC(_baseWidget, GCForeground | GCBackground | GCFont, &values);
	

//     values.foreground = fg;
//     values.background = _hilite;

//     values.foreground = values.background;

    // Create Xpm Pixmaps

    Pixmap nPixmap =  CreateXPMTransportPixmap(_baseWidget, xpmIcon, fg, _bg, bs, ts);

    Pixmap stipPixmap =  CreateXPMTransportPixmap(_baseWidget, xpmIcon, bs, _bg, bs, ts);

    Pixmap hPixmap =  CreateXPMTransportPixmap(_baseWidget, xpmIcon, fg, _hilite, bs, ts);

    Pixmap sPixmap =  CreateXPMTransportPixmap(_baseWidget, xpmIcon, _selectColor, _bg, bs, ts);

    Pixmap shPixmap =  CreateXPMTransportPixmap(_baseWidget, xpmIcon, _selectColor, _hilite, bs, ts);


    // Compute sizes

    if(showLabel)
	XmStringExtent(font, label, &width, &height);
    else {
	width = height = 0;
    }

    XGetGeometry (display, nPixmap,
		  (Window*) &junk,   
		  (int*) &junk, (int*) &junk,     /* x, y of pixmap */
		  &pixmapWidth, &pixmapHeight,       /* width, height of pixmap */
		  &bw, &d);

    int totalWidth = MAX(pixmapWidth, width);
    int totalHeight = pixmapHeight + height + PAD;

    // Create final pixmaps in final size

    Drawable rootDrawable = RootWindowOfScreen(XtScreen(_baseWidget));

    _pixmap = XCreatePixmap(display, rootDrawable, totalWidth, totalHeight, depth);
    _stipplePixmap = XCreatePixmap(display, rootDrawable, totalWidth, totalHeight, depth);
    _highlitePixmap = XCreatePixmap(display, rootDrawable, totalWidth, totalHeight, depth);
    _selectPixmap = XCreatePixmap(display, rootDrawable, totalWidth, totalHeight, depth);
    _selectHighlitePixmap = XCreatePixmap(display, rootDrawable, totalWidth, totalHeight, depth);

    XFillRectangle(display, _pixmap, inverseGc, 0, 0, totalWidth, totalHeight);
    XFillRectangle(display, _stipplePixmap, inverseGc, 0, 0, totalWidth, totalHeight);
    XFillRectangle(display, _highlitePixmap, _inverseHiliteGc, 0, 0, totalWidth, totalHeight);
    XFillRectangle(display, _selectPixmap, inverseGc, 0, 0, totalWidth, totalHeight);
    XFillRectangle(display, _selectHighlitePixmap, _inverseHiliteGc, 0, 0, totalWidth, totalHeight);

    // Copy pixmaps

    int dest_x = (totalWidth - pixmapWidth) / 2; 

    XCopyArea(display, nPixmap, _pixmap, gc, 0, 0, pixmapWidth, pixmapHeight, dest_x, 0);
    XCopyArea(display, stipPixmap, _stipplePixmap, gc, 0, 0, pixmapWidth, pixmapHeight, dest_x, 0);
    XCopyArea(display, hPixmap, _highlitePixmap, highliteGc, 0, 0, pixmapWidth, pixmapHeight, 
	      dest_x, 0);
    XCopyArea(display, sPixmap, _selectPixmap, gc, 0, 0, pixmapWidth, pixmapHeight, dest_x, 0);
    XCopyArea(display, shPixmap, _selectHighlitePixmap, highliteGc, 0, 0, pixmapWidth, pixmapHeight, 
	      dest_x, 0);

    if(showLabel) {
	int label_x = (totalWidth - width) / 2;
	int label_y = pixmapHeight + PAD;

	XmStringDraw (display, _pixmap, font, label, gc, label_x, label_y, width, 
		      XmALIGNMENT_END, XmSTRING_DIRECTION_L_TO_R, NULL);

	XmStringDraw (display, _stipplePixmap, font, label, stippleGc, label_x, label_y, width, 
		      XmALIGNMENT_END, XmSTRING_DIRECTION_L_TO_R, NULL);

	XmStringDraw (display, _highlitePixmap, font, label, highliteGc, label_x, label_y, width, 
		      XmALIGNMENT_END, XmSTRING_DIRECTION_L_TO_R, NULL);

	XmStringDraw (display, _selectHighlitePixmap, font, label, highliteGc,
		      label_x, label_y, width,
		      XmALIGNMENT_END, XmSTRING_DIRECTION_L_TO_R, NULL);

	XmStringDraw (display, _selectPixmap, font, label, gc, label_x, label_y, width,
		      XmALIGNMENT_END, XmSTRING_DIRECTION_L_TO_R, NULL);
    }

    XtVaSetValues(_baseWidget, 
 		  XmNlabelType, XmPIXMAP,
 		  XmNlabelPixmap, _pixmap,
		  XmNlabelInsensitivePixmap, _stipplePixmap,
		  XmNarmPixmap, _highlitePixmap,
 		  XmNshadowThickness, 4,
 		  XmNhighlightThickness, 0,
		  NULL);


    XFreePixmap(display, hPixmap);
    XFreePixmap(display, nPixmap);
    XFreePixmap(display, sPixmap);
    XFreePixmap(display, shPixmap);
    XtReleaseGC(_baseWidget, gc);
    XtReleaseGC(_baseWidget, inverseGc);
    XtReleaseGC(_baseWidget, highliteGc);
    XtReleaseGC(_baseWidget, _inverseHiliteGc);

}


TransportButton::~TransportButton()
{
    // Empty
}

const char* TransportButton::className()
{
  return "TransportButton";
}


