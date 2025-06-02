///////////////////////////////////////////////////////////////////////////////
//
// TrackBin.h
//
// 	ClipBin iterface for the cdplayer
//
//
// Copyright 1996, Silicon Graphics, Inc.
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


#include <stdio.h>
#include <Xm/TextF.h>
#include <Vk/VkInfoDialog.h>
#include <Vk/VkResource.h>

#include "Database.h"
#include "ScsiPlayerApp.h"
#include "TrackBin.h"

//#define DBG(x) {x;}
#define DBG(x) {}  


TrackBin::TrackBin ( const char * name, Widget parent )
	: CdBin (name, parent )
{
  _maxEntries = 99;
  _pixmap = NULL;
  setupFields();
}

void TrackBin::setupFields()
{
  // ADD: allocate the correct number of fields for each entry
  _fieldList = new ABfield[5];
  _numFields = 5;

  _entryHeight = 25;
  _entryWidth = 0;

  // ADD: define the label, width, and editable values for each field
    
  // the icon field
  _fieldList[0].x = _entryWidth;
  _fieldList[0].name = strdup ( "Icon" );
  _fieldList[0].width = 25; // get this fom the pixmap width
  _fieldList[0].editable = False;
  _entryWidth += _fieldList[0].width;
    
  // the index field
  _fieldList[1].x = _entryWidth;
  _fieldList[1].name = strdup (VkGetResource("*TrackBin*trackTitleString", XmCString) );
  _fieldList[1].width = 35; 
  _fieldList[1].editable = False;
  _entryWidth += _fieldList[1].width;

  // the name field
  _fieldList[2].x = _entryWidth;
  //_fieldList[2].name = strdup ( "Title" );
  _fieldList[2].name = strdup (VkGetResource("*TrackBin*titleTitleString", XmCString) );
  _fieldList[2].width = 170;
  _fieldList[2].editable = True;
  _entryWidth += _fieldList[2].width;

  // the timeString field
  _fieldList[3].x = _entryWidth;
  _fieldList[3].name = strdup (VkGetResource("*TrackBin*startTitleString", XmCString) );
  _fieldList[3].width = 45;
  _fieldList[3].editable = False;
  _entryWidth += _fieldList[3].width;

  // the durString field
  _fieldList[4].x = _entryWidth;
  _fieldList[4].name = strdup (VkGetResource("*TrackBin*durationTitleString", XmCString));
  _fieldList[4].width = 50;
  _fieldList[4].editable = False;
  _entryWidth += _fieldList[4].width;
}


void TrackBin::finishEditing ( const Boolean writeChanges )
{
  XtUnmapWidget ( _textEntry );
  
  if ( writeChanges ) {
    switch ( _editingField ) {
    case 0:
      // icon -- not editable
      break;
    case 1:
      // index -- not editable
      break;
    case 2:
      _fileList[_editingEntry].name = XmTextFieldGetString ( _textEntry );
      theScsiPlayerApp->database()->setTrackTitle(_editingEntry+1,
						  XmTextFieldGetString ( _textEntry ));
      theScsiPlayerApp->database()->save();
      break;
    case 3:
      // timeString -- not editable
      break;
    case 4:
      // durString -- not editable
      break;
    }
    }

    _editingEntry = -1;
    _editingField = -1;
    buildPixmap();
}


void TrackBin::handleEntryClick(const int entryIndex,
				const int x,
				const int y,
				const unsigned int state,
				const Time time )
{
  CdBin::handleEntryClick(entryIndex, x, y, state, time);
  DBG(fprintf(stderr,"clicking on field %d\n", whichField(x)));
  if ((theScsiPlayerApp->player() != NULL)
      && (_numSelected == 1) && (whichField(x) != 2)) {
    theScsiPlayerApp->player()->stop();
    theScsiPlayerApp->player()->killThread();
    theScsiPlayerApp->player()->seekTrack(entryIndex+1);
  }
}


void TrackBin::mouseDrag( Widget w, XEvent *event )
{
  if (theScsiPlayerApp->player() != NULL) {
    theScsiPlayerApp->player()->stop();
    theScsiPlayerApp->player()->killThread();
    theScsiPlayerApp->player()->closeDevice();
    theScsiPlayerApp->busy();
    if (theScsiPlayerApp->scsiDevice() == CdDev)
      CdBin::mouseDrag(w, event);
    else
      theInfoDialog->postAndWait(VkGetResource("DatPlayer*noDragOutMsg", XmCString));
    theScsiPlayerApp->notBusy();
  }
  DBG(fprintf(stderr,"Exiting mouseDrag.\n"));
}

void TrackBin::redisplayAll()
{
   hide();
   show();
}

void TrackBin::deleteFields()
{
  DBG(fprintf(stderr,"TrackBin::deleteFields\n"));
  _numEntries = 0;
  if (_pixmap) 
    XFreePixmap( XtDisplay(baseWidget() ), _pixmap );
  allocPixmap();
  buildPixmap();
  hide();
}


