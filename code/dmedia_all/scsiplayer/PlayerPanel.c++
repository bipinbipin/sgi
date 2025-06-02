///////////////////////////////////////////////////////////////////////////////
//
// PlayerPanel.c++
//
// 	Main GUI panel
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
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <math.h>
#include <strings.h>
#include <sys/stat.h>

#include <X11/Xlib.h>
#include <X11/IntrinsicP.h>
#include <X11/Xresource.h>
#include <Xm/Form.h> 
#include <Xm/Frame.h> 
#include <Xm/Label.h> 
#include <Xm/TextF.h> 
#include <Xm/PushB.h> 
#include <Xm/ToggleB.h> 
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/ScrolledW.h> 

#include <Sgm/SpringBox.h>
#include <Vk/VkResource.h>
#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>

#include <dmedia/moviefile.h>

#include "PlayerState.h"
#include "Database.h"
#include "CdAudio.h"
#include "ScsiPlayerApp.h"
#include "PlayerPanel.h" 

#include "DATlogo.xbm"
#include "CDlogo.xbm"

const int PlayerPanel::TRACKS_PER_ROW = 10;

// This is untrue.  There are 33.33 frames per sec., but I need a number for the
// movielib routines.
//
#define DAT_FRAMES_PER_SECOND 33
#define GRID_BUTTON_SPACING  3
#define GRID_BUTTON_WIDTH 32
#define GRID_BUTTON_HEIGHT 25

//#define DBG(x) {x;}
#define DBG(x) {}  

extern ScsiPlayerApp* theScsiPlayerApp;

PlayerPanel* thePlayerPanel;

PlayerPanel::PlayerPanel(const char *name, Widget parent)
    : VkComponent(name) 
{
  theScsiPlayerApp->_highestDisplayTrack = 0;

  bzero(_depressedButtonVector, 1000*sizeof(int));
  bzero(_trackButtons, 1000*sizeof(_trackButtons[0]));

  _mainWindow = (MainWindow*)theScsiPlayerApp->mainWindow();

  _baseWidget = XtVaCreateManagedWidget(_name, xmFormWidgetClass, parent, 
					XmNtraversalOn, True,
					XmNresizePolicy, XmRESIZE_ANY, 
					XmNfractionBase, 100,
					XmNhorizontalSpacing, 4,
 					XmNbottomAttachment, XmATTACH_NONE,
					(XtPointer) NULL );

  _controlPanel = XtVaCreateManagedWidget(_name, xmFormWidgetClass, _baseWidget, 
					  //XmNtraversalOn, False,
					  XmNtraversalOn, True,
					  XmNresizePolicy, XmRESIZE_ANY, 
					  XmNfractionBase, 100,
					  XmNleftAttachment, XmATTACH_FORM,
					  XmNtopAttachment, XmATTACH_FORM,
					  XmNbottomAttachment, XmATTACH_FORM,
					  (XtPointer) NULL ); 

  // install a callback to guard against unexpected widget destruction
  installDestroyHandler();

  thePlayerPanel = this;

  // display the Track number

  _trackNumberDisplay = new NumberDisplay("trackNumberDisplay",
					  _controlPanel, LabelOnBottom,  
					  AV_INT,
					  (VkCallbackMethod)PlayerPanel::trackChanged); 

  XtVaSetValues(_trackNumberDisplay->baseWidget(),
		XmNleftAttachment, XmATTACH_FORM, 
		XmNtopAttachment, XmATTACH_FORM, 
		(XtPointer)NULL ); 
		  
  _trackNumberDisplay->show();

  // display the time

  _indexDisplay = new NumberDisplay("indexNumberDisplay", _controlPanel,
				    LabelOnBottom,
				    AV_INT,
				    NULL); 

  XtVaSetValues(_indexDisplay->baseWidget(),
		XmNtopAttachment, XmATTACH_FORM, 
		XmNleftAttachment, XmATTACH_WIDGET, 
		XmNleftWidget, _trackNumberDisplay->baseWidget(),
		(XtPointer)NULL ); 
		  
  _indexDisplay->show();

  _absoluteTimeDisplay = new NumberDisplay("absoluteTimeDisplay", _controlPanel,
					   LabelOnBottom,
					   AV_DIGITALCLOCKTIME,
					   (VkCallbackMethod)PlayerPanel::absoluteTimeChanged); 

  XtVaSetValues(_absoluteTimeDisplay->baseWidget(),
		XmNtopAttachment, XmATTACH_FORM, 
		XmNleftAttachment, XmATTACH_WIDGET, 
		XmNleftWidget, _indexDisplay->baseWidget(),
		(XtPointer)NULL ); 
		  
  _absoluteTimeDisplay->show();

  _remainingTimeDisplay = new NumberDisplay("remainingTimeDisplay", _controlPanel,
					    LabelOnBottom,
					    AV_DIGITALCLOCKTIME,
					    NULL); 

  XtVaSetValues(_remainingTimeDisplay->baseWidget(),
		XmNtopAttachment, XmATTACH_FORM, 
		XmNleftAttachment, XmATTACH_WIDGET, 
		XmNleftWidget, _absoluteTimeDisplay->baseWidget(),
		(XtPointer)NULL ); 

  _remainingTimeDisplay->show();


  // Sony endorsement

  _logoLabel = XtVaCreateManagedWidget("logoLabel",
				       xmLabelWidgetClass,
				       //_buttonSpringBox, 
				       _controlPanel,
	 			       XmNlabelType, XmPIXMAP, 
				       XmNtopAttachment, XmATTACH_FORM,
				       XmNrightAttachment, XmATTACH_FORM, 
				       (XtPointer) NULL ); 

  int fg, bg, depth;  
  XtVaGetValues(_logoLabel,
		XmNforeground, &fg, XmNbackground, &bg, XmNdepth, &depth,
		NULL);
  Display* display = XtDisplay(_logoLabel);
  Screen* screen = XtScreen(_logoLabel);
  Pixmap pm;
  if (theScsiPlayerApp->scsiDevice() == CdDev)
    pm = XCreatePixmapFromBitmapData(display, RootWindowOfScreen(screen),
				     (char*)CDlogo_bits,
				     CDlogo_width, CDlogo_height,
				     fg, bg, depth);
  else
    pm = XCreatePixmapFromBitmapData(display, RootWindowOfScreen(screen),
				     (char*)DATlogo_bits,
				     DATlogo_width, DATlogo_height,
				     bg, fg, depth);

  XtVaSetValues(_logoLabel, XmNlabelPixmap, pm, NULL);


  // the track button grid.

  _trackGridFrame = XtVaCreateManagedWidget("trackGridFrame",
					    xmFrameWidgetClass,
					    _controlPanel, 
					    XmNshadowType, XmSHADOW_ETCHED_IN, 
					    XmNresizable, True,
					    XmNleftAttachment, XmATTACH_FORM, 
					    XmNtopAttachment, XmATTACH_WIDGET,
					    XmNtopWidget, _trackNumberDisplay->baseWidget(),
					    XmNrightAttachment, XmATTACH_FORM, 
					    XmNbottomAttachment, XmATTACH_NONE, 
					    XmNmarginHeight, GRID_BUTTON_SPACING,
					    XmNmarginWidth, GRID_BUTTON_SPACING,
					    NULL);

  _trackGridRC = XtVaCreateManagedWidget("trackGridRC",
					 xmRowColumnWidgetClass,
					 _trackGridFrame, 
					 XmNresizeHeight, True,
					 XmNentryAlignment, XmALIGNMENT_CENTER, 
					 XmNpacking, XmPACK_NONE, 
 					 XmNorientation, XmHORIZONTAL, 
					 XmNchildHorizontalAlignment, XmALIGNMENT_CENTER, 
					 XmNtraversalOn, False,
					 (XtPointer) NULL ); 

  _timeline = new AvTimeline("timeline",
				_controlPanel,
				(VkCallbackObject *)this,
				(void*)NULL,
				(VkCallbackMethod)&PlayerPanel::timelineLocate,
				(VkCallbackMethod)&PlayerPanel::timelineSelection);

  XtVaSetValues(_timeline->baseWidget(),
		XmNtopAttachment, XmATTACH_WIDGET,
		XmNtopWidget, _trackGridFrame,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM, 
		XmNbottomAttachment, XmATTACH_NONE, 
		NULL);

  //_timeline->setHandleUpdates(False);
  _timeline->show();

  _programTimeDisplay = new NumberDisplay("programTimeDisplay", _controlPanel,
					  NoLabel,
					  AV_NORMALTIME,
					  (VkCallbackMethod)PlayerPanel::programTimeChanged,
					  (VkCallbackMethod)PlayerPanel::programSelectionChanged); 

  XtVaSetValues(_programTimeDisplay->baseWidget(),
		XmNtopAttachment, XmATTACH_WIDGET, 
		XmNtopWidget, _timeline->baseWidget(),
		XmNleftAttachment, XmATTACH_FORM, 
		XmNbottomAttachment, XmATTACH_NONE, 
		(XtPointer)NULL ); 
		  
  _programTimeDisplay->show();

  // to encapsulate the buttons.
 
  _buttonSpringBox = XtVaCreateManagedWidget("buttonSpringBox",
					     sgSpringBoxWidgetClass,
					     _controlPanel,
					     XmNtraversalOn, False,
					     XmNresizePolicy, XmRESIZE_ANY, 
					     XmNfractionBase, 100,
					     XmNleftAttachment, XmATTACH_WIDGET,
					     XmNleftWidget, _programTimeDisplay->baseWidget(),
					     XmNtopAttachment, XmATTACH_WIDGET,
					     XmNtopWidget, _timeline->baseWidget(),
					     XmNrightAttachment, XmATTACH_FORM,
					     (XtPointer) NULL ); 

  _avTransport = new AvTransport("avTransport", _buttonSpringBox, this, (void*)NULL, 
				 (VkCallbackMethod) PlayerPanel::PlayCallback,
				 (VkCallbackMethod) PlayerPanel::StopCallback,
				 (VkCallbackMethod) NULL,
				 (VkCallbackMethod) PlayerPanel::RtzCallback,
				 (VkCallbackMethod) NULL,
				 (VkCallbackMethod) PlayerPanel::PreviousTrackCallback,
				 (VkCallbackMethod) PlayerPanel::NextTrackCallback,
				 (VkCallbackMethod) NULL,
				 (VkCallbackMethod) NULL);


  _avTransport->show();

  _volume = new AvVolume("volume", _buttonSpringBox, this, (void *)NULL, 
			 (VkCallbackMethod) PlayerPanel::volumeChanged);
  _volume->setMinMax(0, 100);
  _volume->setVolume((int)(theScsiPlayerApp->_initialGain * 100));

  _volume->show();

  setupTrackPanel();

}


// setup the Desktop stuff
//
void PlayerPanel::setupTrackPanel()
{

  Widget separator =
    XtVaCreateManagedWidget("separator", xmSeparatorWidgetClass,
			    _baseWidget, 
			    XmNorientation, XmVERTICAL,
			    XmNleftAttachment, XmATTACH_WIDGET,
			    XmNleftWidget, _controlPanel,
			    XmNtopAttachment, XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_FORM,
			    (XtPointer) NULL );

  _trackInfoForm =
    XtVaCreateManagedWidget("trackInfoForm", xmFormWidgetClass,
			    _baseWidget, 
			    XmNresizable, True,
			    // When I set this to true, I get lose the insertion cursor.
			    // XmNtraversalOn, False,
			    XmNresizePolicy, XmRESIZE_ANY, 
			    XmNfractionBase, 100,
			    XmNleftAttachment, XmATTACH_WIDGET,
			    XmNleftWidget, separator,
			    XmNtopAttachment, XmATTACH_FORM,
			    XmNrightAttachment, XmATTACH_FORM,
			    XmNbottomAttachment, XmATTACH_FORM,
			    (XtPointer) NULL );

  _titleForm =
    XtVaCreateManagedWidget("titleForm", xmFormWidgetClass, _trackInfoForm,
			    XmNresizePolicy, XmRESIZE_ANY, 
			    XmNfractionBase, 100,
			    XmNresizable, True,
			    XmNtopAttachment, XmATTACH_FORM,
			    XmNleftAttachment, XmATTACH_FORM,
			    XmNrightAttachment, XmATTACH_FORM,
			    (XtPointer) NULL );

  Widget titleLabel = XtVaCreateManagedWidget("titleLabel", xmLabelWidgetClass,
					      _titleForm,
					      XmNlabelType, XmSTRING, 
					      XmNleftAttachment, XmATTACH_FORM,
 					      XmNtopAttachment, XmATTACH_FORM,
					      (XtPointer) NULL ); 

  Widget artistLabel =  XtVaCreateManagedWidget("artistLabel", xmLabelWidgetClass,
						_titleForm,
						XmNlabelType, XmSTRING, 
						XmNleftAttachment, XmATTACH_POSITION,
						XmNleftPosition, 50,
 						XmNtopAttachment, XmATTACH_FORM,
						(XtPointer) NULL); 

  _titleTextF = XtVaCreateManagedWidget("titleTextF", xmTextFieldWidgetClass,
					_titleForm,
					XmNeditable, True,
					XmNcursorPositionVisible, True,
					XmNleftAttachment, XmATTACH_FORM,
					XmNtopAttachment, XmATTACH_WIDGET,
					XmNtopWidget, titleLabel,
 					XmNrightAttachment, XmATTACH_POSITION,
 					XmNrightPosition, 50,
					XmNbottomAttachment, XmATTACH_FORM,
					XmNtraversalOn, True,
					NULL);


  XtAddCallback(_titleTextF, XmNactivateCallback,
		&PlayerPanel::ChangeCDTitleCallback,
		(XtPointer) this ); 

  XtAddCallback(_titleTextF, XmNlosingFocusCallback,
		&PlayerPanel::ChangeCDTitleCallback,
		(XtPointer) this ); 


  _artistTextF = XtVaCreateManagedWidget("artistTextF", xmTextFieldWidgetClass,
					 _titleForm,
					 XmNeditable, True,
					 XmNcursorPositionVisible, True,
					 XmNleftAttachment, XmATTACH_POSITION,
					 XmNleftPosition, 50,
					 XmNtopAttachment, XmATTACH_WIDGET,
					 XmNtopWidget, artistLabel,
					 XmNrightAttachment, XmATTACH_FORM,
					 XmNbottomAttachment, XmATTACH_FORM,
					 XmNtraversalOn, True,
					 NULL);

  XtAddCallback(_artistTextF, XmNactivateCallback,
		&PlayerPanel::ChangeCDArtistCallback,
		(XtPointer) this ); 

  XtAddCallback(_artistTextF, XmNlosingFocusCallback,
		&PlayerPanel::ChangeCDArtistCallback,
		(XtPointer) this ); 

  makeTrackBin();
}

void PlayerPanel::makeTrackBin() 
{
  _trackBin = new TrackBin("TrackBin", _trackInfoForm);

  XtVaSetValues(_trackBin->baseWidget(), 
		   XmNleftAttachment, XmATTACH_FORM,
		   XmNtopAttachment, XmATTACH_WIDGET,
		   XmNtopWidget, _titleForm,
		   XmNrightAttachment, XmATTACH_FORM,
		   XmNbottomAttachment, XmATTACH_FORM,
		   NULL);
}

void PlayerPanel::populateTrackBin()
{
  for (int i = 1; i <= theScsiPlayerApp->transport()->lastTrack; i++) 
    addTrackListElement(i);
  _trackBin->redisplayAll();
}


void PlayerPanel::addTrackListElement(int trackNum)
{
  ScsiPlayer* transport = theScsiPlayerApp->transport();

  char fileString[20];
  char startString[20];
  char durString[20];

  if  (!theScsiPlayerApp->player()
       ||  !theScsiPlayerApp->player()->validTrackNumber(trackNum))
    return;

  sprintf(startString, "%2d:%02d", transport->trackInfo[trackNum]->start_min,
	  transport->trackInfo[trackNum]->start_sec);
  sprintf(durString, "%2d:%02d",  transport->trackInfo[trackNum]->total_min,
	    transport->trackInfo[trackNum]->total_sec);

  if (theScsiPlayerApp->scsiDevice() == CdDev)
    //sprintf(fileString, "/CDROM/Track%02d.aiff", trackNum);
    sprintf(fileString, "%s/Track%02d.aiff",
	    theScsiPlayerApp->_cdMountDirectory, trackNum);
  else
    sprintf(fileString, "/usr/tmp/Track%02d.aiff", trackNum);

  DBG(fprintf(stderr,"adding track list entry for %d\n", trackNum));
  _trackBin->addEntry(trackNum,
		      theScsiPlayerApp->database()->getTrackTitle(trackNum),
		      fileString, 
		      startString,
		      durString);
}

PlayerPanel::~PlayerPanel() 
{
  XrmDestroyDatabase(theScsiPlayerApp->_trackInfoDb);
}

const char * PlayerPanel::className()
{
  return ("PlayerPanel");
} 


//   AudioLevelFromSliderValue:  map linear slider range [0 .. 100]
// 				    to exponenetial Audio Library range 
// 				    [0 .. 255]
//
float AudioLevelFromSliderValue(int value)
{
  float level;

  if (value == 0) 
    level = 0;
  /*  10^(0.01*value*log10(255)),      0.01*log10(255) = 0.02406540183 */
  else
    //level = (pow(10, 0.02406540183*double(value)) + 0.5) / 255.0;
    level =  value*.01;

  DBG(printf("AudioLevelFromSliderValue(): value=%d -> level=%f\n", value, level));


  return ((level > 1.0) ? 1.0 : level);
}


//   SliderValueFromAudioLevel: 
// 				return slider value (range [0..100]) from
// 				Audio Library level parameter (range 0..255)
//
int SliderValueFromAudioLevel(float level)
{
  if (level == 0)
    return (0);

  // 100*log10(level)/log10(255),     100/log10(255) = 41.5534304 
  float value = 0.5 + 41.5534304*log10((double) (level*255));

  // bound slider range to [0..100] 
  if	(value < 0)
    value = 0;
  else if (value > 100)
    value = 100;

  DBG(printf("SliderValueFromAudioLevel(): level=%d -> value=%g\n", level, value));

  return ((int) value);
} 

void PlayerPanel::volumeChanged(VkCallbackObject *, void */*clientData*/, 
			     void *callData)
{
  float newGain = AudioLevelFromSliderValue((int)callData);
  if (theScsiPlayerApp->player()) {
    theScsiPlayerApp->player()->_audioBuffer->setGain(newGain);
    theScsiPlayerApp->updateGainSetting();
  }
}

MAtimevalue DeviceFramesPerSecond()
{
  if (theScsiPlayerApp->scsiDevice() == CdDev)
    return CDDA_FRAMES_PER_SECOND;
  else
    return DAT_FRAMES_PER_SECOND;
}

MAtimevalue MsfToFrame(int min, int sec, int fr)
{
  if (theScsiPlayerApp->scsiDevice() == CdDev)
    return CDmsftoframe(min, sec, fr);
  else
    return (2000*min + 33*sec + sec/3 + fr);
}

void PlayerPanel::timelineLocate(VkCallbackObject* obj, void* /* clientData */, 
				 void* callData )
{
  unsigned long  newLoc;
  AvTimeline* tl = (AvTimeline*)obj;

  ScsiPlayer* transport = theScsiPlayerApp->transport();

  if  (!theScsiPlayerApp->player()
       ||  !theScsiPlayerApp->player()->validTrackNumber(transport->track))
    return;

  struct __avTimeline_Time* s = (__avTimeline_Time*)callData;

  DBG(fprintf(stderr,"PlayerPanel::timelineLocate\n"));
  if (transport->trackInfo[transport->track])
    newLoc = mvConvertTime(s->time, s->timescale, DeviceFramesPerSecond())
      + transport->trackInfo[transport->track]->start_block;
  else
    newLoc = 0;

  tl->setTime(s->time, s->timescale);
  _programTimeDisplay->display(s->time,s->timescale);
  
  if (s->final) {
    theScsiPlayerApp->player()->seekFrame(newLoc);
  }

}

void PlayerPanel::timelineSelection(VkCallbackObject* obj, void* /* clientData */,
				    void* callData )
{
  AvTimeline* tl = (AvTimeline*)obj;
  struct MAselection* s = (MAselection*)callData;
  Player* player = theScsiPlayerApp->player();
  
  ScsiPlayer* transport = theScsiPlayerApp->transport();

  if  (!theScsiPlayerApp->player()
       ||  !theScsiPlayerApp->player()->validTrackNumber(transport->track))
    return;

  if (s->duration == 0) {		// Check to see if this was a no-op
    _programTimeDisplay->avTime()->invalidateSelection();
    player->_selectionEnd = 0;
    _mainWindow->_saveSelectionAsItem->deactivate();
    _mainWindow->_copySelectionItem->deactivate();
    return;
  } 

  player->_selectionStart = mvConvertTime(s->start, s->timescale, DeviceFramesPerSecond())
    + transport->trackInfo[transport->track]->start_block;

  player->_selectionEnd = mvConvertTime(s->duration+s->start, s->timescale,
					DeviceFramesPerSecond())
    + transport->trackInfo[transport->track]->start_block;

  _programTimeDisplay->avTime()->setSelection(s->start, s->duration, s->timescale);
  tl->setSelection(s->start, s->duration, s->timescale);
  
  _mainWindow->_saveSelectionAsItem->activate();
  _mainWindow->_copySelectionItem->activate();
  
  DBG(fprintf(stderr,"Selection Start = %d, Selection End = %d\n",
	      (int)player->_selectionStart, (int)player->_selectionEnd));

  
}

void PlayerPanel::selectAll()
{
  Player* player = theScsiPlayerApp->player();
  ScsiPlayer* transport = theScsiPlayerApp->transport();

  player->_selectionStart = transport->trackInfo[transport->track]->start_block;

  player->_selectionEnd = transport->trackInfo[transport->track]->total_blocks
                          + transport->trackInfo[transport->track]->start_block;

  _programTimeDisplay->avTime()->setSelection(0,
					      transport->trackInfo[transport->track]->total_blocks,
					      DeviceFramesPerSecond());

  _timeline->setSelection(0,
			  transport->trackInfo[transport->track]->total_blocks,
			  DeviceFramesPerSecond());

}

void PlayerPanel::selectNone()
{
  Player* player = theScsiPlayerApp->player();
  ScsiPlayer* transport = theScsiPlayerApp->transport();

  player->_selectionStart = 0;
  player->_selectionEnd = 0;

  _programTimeDisplay->avTime()->setSelection(0, 0, DeviceFramesPerSecond());
  _programTimeDisplay->avTime()->invalidateSelection();
  _timeline->setSelection(0, 0, DeviceFramesPerSecond());
}

// a little under .1 seconds (in blocks)

#define NUDGE_INCREMENT 8

void PlayerPanel::extendSelection(Boolean extendRight)
{
  Player* player = theScsiPlayerApp->player();
  ScsiPlayer* transport = theScsiPlayerApp->transport();
  
  long long trackStart = transport->trackInfo[transport->track]->start_block;
  long long trackEnd = trackStart + transport->trackInfo[transport->track]->total_blocks;
  long long midpoint;

  // If we don't have a selection, set it to the current location.o
  if (player->_selectionEnd == 0) 
    player->_selectionStart = player->_selectionEnd = midpoint = transport->curBlock;
  else
    midpoint = ((player->_selectionEnd - player->_selectionStart)/2)
                + player->_selectionStart;

 if (extendRight) {
   if ((transport->curBlock >= midpoint)
       || (player->_selectionStart > player->_selectionEnd)) {
     if (player->_selectionEnd < trackEnd) {
       player->_selectionEnd += NUDGE_INCREMENT;
       transport->curBlock = player->_selectionEnd;
     }
   }
   else {
     player->_selectionStart += NUDGE_INCREMENT;
     transport->curBlock = player->_selectionStart;
   }
 }
  else {
    if ((transport->curBlock <= midpoint)
	&& (player->_selectionStart > trackStart)) {
      player->_selectionStart -= NUDGE_INCREMENT;
      transport->curBlock = player->_selectionStart;
    }
    else if (player->_selectionEnd > trackStart) {
      player->_selectionEnd -= NUDGE_INCREMENT;
      transport->curBlock = player->_selectionEnd;
    }
  }


  long long relSelectionStart = player->_selectionStart - trackStart;
  long long relSelectionDur = player->_selectionEnd - player->_selectionStart;

  // update the GUI

  _programTimeDisplay->avTime()->setSelection(relSelectionStart, relSelectionDur,
					      DeviceFramesPerSecond());
  _timeline->setSelection(relSelectionStart, relSelectionDur,
			  DeviceFramesPerSecond());
  _timeline->setTime ((long long)(transport->curBlock - trackStart),
		      DeviceFramesPerSecond());
}


TrackCBData* trackCB = (TrackCBData*)malloc(sizeof(TrackCBData));


void PlayerPanel::cleanupTrackButtons(int currentTrackNum)
{
  if (theScsiPlayerApp->player()) {
    thePlayerPanel->_depressedButtonVector[currentTrackNum]++;
    for (int i = 0; i <= theScsiPlayerApp->_highestDisplayTrack; i++)
      if (thePlayerPanel->_depressedButtonVector[i]) {
	Widget oldButton = thePlayerPanel->_trackButtons[i];
	if (oldButton) 
	  if (i == currentTrackNum)
	    XtVaSetValues(oldButton, XmNset, True, NULL);
	  else {
	    DBG(fprintf(stderr,"clearing button %d\n", i));
	    XtUnmanageChild(oldButton);
	    XtVaSetValues(oldButton, XmNset, False, NULL);
	    XtManageChild(oldButton);
	    thePlayerPanel->_depressedButtonVector[i] = 0;
	  }
      }
  }
}

void PlayerPanel::CueTrackValueChangedCallback(Widget w , XtPointer clientData,
					       XtPointer /* callData */ ) 
{ 
  TrackCBData* trackCB = (TrackCBData*)clientData;
  DBG(fprintf(stderr,"CueTrack callback %d\n", trackCB->trackNum));
  theScsiPlayerApp->player()->seekTrack(trackCB->trackNum, DM_FALSE);
  thePlayerPanel->cleanupTrackButtons(trackCB->trackNum);
}


void PlayerPanel::displaySetup(ScsiPlayer* transport)
{
  // a cd came in.

  theScsiPlayerApp->player()->databaseSetup();

  displayGrid(transport);

  XtVaSetValues(_titleTextF,
		XmNvalue, theScsiPlayerApp->database()->getTitle(),
		NULL);
  XtVaSetValues(_artistTextF,
		XmNvalue,theScsiPlayerApp->database()->getArtistName(),
		NULL);

  _mainWindow->show();
  if (!theScsiPlayerApp->_displayTrackList) {
    _mainWindow->playerOnlyView();
  }
}


Widget FindShell(Widget w)
{
  while (!XtIsVendorShell(w))
    w = XtParent(w);
  return w;
}

void PlayerPanel::adjustShell(Widget shell, Dimension beforeHt, Dimension afterHt) 
{
  if ((beforeHt != afterHt) && _mainWindow->visible()) {
    Dimension ht;
    int spacing = (beforeHt < afterHt) ? GRID_BUTTON_SPACING : -(GRID_BUTTON_SPACING);
    shell = FindShell(shell);
    XtVaGetValues(shell, XmNheight, &ht, NULL);
    XtVaSetValues(shell,
		  XmNheight,
		  (Dimension)((int)ht + ((int)afterHt + spacing - (int)beforeHt)),
		  NULL);
  }
}

// Returns the amount that the grid needs to grow vertically
//
int PlayerPanel::addGridButton(int trackNum)
{
  Dimension beforeHt, afterHt;
    int growWindow = 0;

  XtVaGetValues(_trackGridRC, XmNheight, &beforeHt, NULL);

  if  (!theScsiPlayerApp->player()
       ||  !theScsiPlayerApp->player()->validTrackNumber(trackNum))
    return 0;
  
  char tmpString[MAXNAME]; //  MAXNAME is from Database.h
  for (int i = 1; i <= trackNum; i++) {
    if (!_trackButtons[i]) {

      if (((i-1) % TRACKS_PER_ROW) == 0) 
	growWindow =+ GRID_BUTTON_SPACING + GRID_BUTTON_HEIGHT;

      sprintf(tmpString, "%d", i);
      _trackButtons[i] =
	XtVaCreateManagedWidget("trackGridButton",
				xmToggleButtonWidgetClass,
				_trackGridRC, 
				XmNindicatorOn, False,
				XmNwidth, GRID_BUTTON_WIDTH,
				XmNheight, GRID_BUTTON_HEIGHT ,
				XmNrecomputeSize, False,
				XmNx, ((i-1) % TRACKS_PER_ROW)*GRID_BUTTON_WIDTH,
				XmNy, ((i-1)/TRACKS_PER_ROW)*GRID_BUTTON_HEIGHT,
				XmNlabelType, XmSTRING, 
				XmNlabelString,
				XmStringCreateSimple(tmpString),
				XmNtraversalOn, False,
				(XtPointer) NULL ); 

      TrackCBData* trackCB = (TrackCBData*)malloc(sizeof(TrackCBData));
      trackCB->trackNum = i;
      trackCB->otherPointer = (void*)this;
      XtAddCallback(_trackButtons[i], XmNvalueChangedCallback,
		    &PlayerPanel::CueTrackValueChangedCallback, 
		    (XtPointer)trackCB); 
      //if (theScsiPlayerApp->scsiDevice() == DatDev)
//       if (i > _highestDisplayTrack)
// 	addTrackListElement(i);
    }
    else {
      DBG(fprintf(stderr,"Not building trackButton %d\n", i));
    }
  }
  if (theScsiPlayerApp->_highestDisplayTrack < trackNum) {
    theScsiPlayerApp->_highestDisplayTrack = trackNum;
  }

  if (growWindow) {
    growWindow += GRID_BUTTON_SPACING;
    XtVaGetValues(_trackGridRC, XmNheight, &afterHt, NULL);
    adjustShell(baseWidget(), beforeHt, afterHt);
  }
  return growWindow;
}


void PlayerPanel::displayGrid(ScsiPlayer* transport)
{
  int numTracks = transport->lastTrack;

  if (numTracks) {
    int numRows = numTracks / TRACKS_PER_ROW;
    if (numTracks % TRACKS_PER_ROW) numRows++;
    
    addGridButton(numTracks);
    populateTrackBin();

    if (theScsiPlayerApp->scsiDevice() == CdDev) {
       theScsiPlayerApp->player()->reopenDevice();
    }
  }
}

void PlayerPanel::clearDisplay()
{
  int n;
  Dimension beforeHt, afterHt;

  _trackNumberDisplay->display(0);
  _indexDisplay->display(0);
  _programTimeDisplay->display(0,DeviceFramesPerSecond());
  _absoluteTimeDisplay->display(0,DeviceFramesPerSecond());
  _remainingTimeDisplay->display(0,DeviceFramesPerSecond());

  XtDestroyWidget(_trackBin->baseWidget());
  makeTrackBin();

  XtVaSetValues(_titleTextF, XmNvalue, "", NULL);
  XtVaSetValues(_artistTextF, XmNvalue, "", NULL);
  
  XtVaGetValues(_trackGridRC, XmNnumChildren, &n, NULL);
  XtVaGetValues(_trackGridRC, XmNheight, &beforeHt, NULL);
  
  for (int i = 1; i <= n; i++) {
    if (_trackButtons[i]) {
      XtUnmanageChild(_trackButtons[i]);
      _trackButtons[i] = NULL;
      _depressedButtonVector[i] = 0;
    }
  }

  XtVaGetValues(_trackGridRC, XmNheight, &afterHt, NULL);
  adjustShell(baseWidget(), beforeHt, afterHt);

  theScsiPlayerApp->_highestDisplayTrack = 0;
  theScsiPlayerApp->_trackCache = 0;
}

void PlayerPanel::updateDisplay(ScsiPlayer* transport)
{
  if (!transport || !theScsiPlayerApp->_audioMediaLoaded) {
    return;
  }

  int curTrackIndex;
  int prevTrackIndex = theScsiPlayerApp->_trackCache;

  if (theScsiPlayerApp->player()->validTrackNumber(transport->track))
    curTrackIndex = transport->track;
  else 
    curTrackIndex = 0;

  //DBG(fprintf(stderr,"updateDisplay...\n"));

  // Check to see if the DAT track count has grown
  if (theScsiPlayerApp->_highestDisplayTrack < transport->lastTrack ) {
    addGridButton(transport->lastTrack);
    _trackBin->deleteFields();
    populateTrackBin();
  }

  if (_mainWindow->iconic()) {
    DBG(fprintf(stderr,"App is iconic, skipping display.\n"));
    return;
  }


  if (theScsiPlayerApp->player()->_playerTimeline == NULL)
    theScsiPlayerApp->player()->_playerTimeline = _timeline;

  //  things to do when the track changes

  if (curTrackIndex != prevTrackIndex
      && theScsiPlayerApp->player()->validTrackNumber(transport->track)) {
    DBG(fprintf(stderr,"Track change in display from %d to %d\n", prevTrackIndex,
		curTrackIndex));

    // display the program number
    _trackNumberDisplay->display(transport->track);
    _trackBin->invalidateSelection();
    int trackBinNum = transport->track - 1;
    _trackBin->setSelection (&trackBinNum, 1);

    if (prevTrackIndex) {
      // the management stuff is due to a motif bug.
      XtUnmanageChild(_trackButtons[prevTrackIndex]);
      XtVaSetValues(_trackButtons[prevTrackIndex],
		    XmNset, False,
		    NULL);
      XtManageChild(_trackButtons[prevTrackIndex]);
      _depressedButtonVector[prevTrackIndex] = 0;
    }
    // This is redundant if the user hit the button, but not if the program
    //  changed tracks.
    if (curTrackIndex) {
      XtUnmanageChild(_trackButtons[curTrackIndex]);
      XtVaSetValues(_trackButtons[curTrackIndex],
		    XmNset, True, NULL);
      XtManageChild(_trackButtons[curTrackIndex]);
      _depressedButtonVector[curTrackIndex]++;
    }

    
    if (transport->trackInfo[transport->track])
      _timeline->setDuration((long long)transport->trackInfo[curTrackIndex]->total_blocks,
			     DeviceFramesPerSecond());
    _timeline->setSelection((MAtimevalue)0, (MAtimevalue)0, DeviceFramesPerSecond());
    _timeline->setTime((MAtimevalue)0, DeviceFramesPerSecond());
    theScsiPlayerApp->player()->_selectionEnd = 0;
    _mainWindow->_saveSelectionAsItem->deactivate();
    _mainWindow->_copySelectionItem->deactivate();
    _programTimeDisplay->avTime()->invalidateSelection();
    theScsiPlayerApp->_trackCache = curTrackIndex;
  }

  // make sure the stop/play buttons are in the right state.

  switch (theScsiPlayerApp->player()->_playState) {
  case Stopped:
  case Paused:
    _avTransport->setStopState(True);
    break;
  case Playing:
    _avTransport->setPlayState(True);
    break;
  case FastFwding:
  case Rewinding:
  case Seeking:
    //     theScsiPlayerApp->player()->updateTransport();
    //     DBG(fprintf(stderr,"PlayerPanel updating transport\n"));
  default:
    break;
  }

  _indexDisplay->display(transport->index);

  TRACKINFO* info = transport->trackInfo[transport->track];

  if (theScsiPlayerApp->player()->validTrackNumber(transport->track)) {
    unsigned long p, a, r;

    // If we are playing, don't display millisecs.

    MAtimevalue framesPerSec = DeviceFramesPerSecond();

    a = MsfToFrame(transport->absMin, transport->absSec, transport->absFrame);
    r = info->total_blocks - MsfToFrame(transport->progMin, transport->progSec,
					transport->progFrame);

    if (theScsiPlayerApp->player()->_playState == Playing) {
      if (theScsiPlayerApp->scsiDevice() == CdDev) 
	p = MsfToFrame(transport->progMin,transport->progSec, 0);
      else {
	// DATs have drop frames.
	p = MsfToFrame(transport->progMin,transport->progSec, transport->progFrame);
	p = (p / framesPerSec) * framesPerSec;
      }
    }
    else
      p = MsfToFrame(transport->progMin,transport->progSec, transport->progFrame);

    _programTimeDisplay->display(p, framesPerSec);
    _absoluteTimeDisplay->display(a, framesPerSec);
    _remainingTimeDisplay->display(r, framesPerSec);

    // fprintf(stderr,"curBlock = %d; start_block = %d; total_blocks = %d\n",
    // 	    (int)transport->curBlock,
    // 	    (int)transport->trackInfo[transport->track]->start_block,
    // 	    (int)transport->trackInfo[transport->track]->total_blocks);

    if (!_timeline->isDragging()
	&& (theScsiPlayerApp->player()->_playState != Seeking))
      _timeline->setTime	  // sorry for the ugly line break
	((long long) (transport->curBlock -
		      transport->trackInfo[transport->track]->start_block), 
	 DeviceFramesPerSecond());
  }
} 


// typically called from the MainWindow stateChange.
//
void PlayerPanel::stateChange(ScsiPlayer* transport)
{
  if (theScsiPlayerApp->_flagDisplayForNewMedia) {
    // setup once-per-cd display info
    displaySetup(transport);
    theScsiPlayerApp->_flagDisplayForNewMedia = False;
  }

  updateDisplay(transport);
}

/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 


void PlayerPanel::RtzCallback(VkCallbackObject* /* cbObj */ , XtPointer /* clientData */,
				     XtPointer /* callData */ ) 
{ 
  if (theScsiPlayerApp->player()) {
    theScsiPlayerApp->player()->seekTrack(1, DM_FALSE);
    cleanupTrackButtons(1);
  }
}

void PlayerPanel::PreviousTrackCallback(VkCallbackObject* /* cbObj */, 
					XtPointer /* clientData */,
					XtPointer /* callData */ ) 
{ 
  Player* player = theScsiPlayerApp->player();
  int curTrack;
  if (player){
  if (theScsiPlayerApp->transport()->track == 1)
    curTrack = 1;
  else
    curTrack = (theScsiPlayerApp->transport()->track - 1);
  player->seekTrack(curTrack, DM_FALSE);
  cleanupTrackButtons(curTrack);
  DBG(fprintf(stderr,"Prev to %d\n",curTrack));
  }
}

void PlayerPanel::NextTrackCallback (VkCallbackObject* /* cbObj */ , 
				     XtPointer /* clientData */,
				     XtPointer /* callData */ ) 
{ 
  if (theScsiPlayerApp->player())
    if ( (theScsiPlayerApp->transport()->track != (theScsiPlayerApp->transport()->lastTrack))
	 || (theScsiPlayerApp->scsiDevice() == DatDev) ) {
      // we let dat seek past the end
      int nextTrack = theScsiPlayerApp->transport()->track + 1;
      theScsiPlayerApp->player()->seekTrack(nextTrack, DM_FALSE);
      cleanupTrackButtons(nextTrack);
    }
}


void PlayerPanel::StopCallback (VkCallbackObject* /* cbObj */, XtPointer /* clientData */, 
				XtPointer /* callData */) 
{ 
  if (theScsiPlayerApp->player())
    theScsiPlayerApp->player()->stop();
  theScsiPlayerApp->_recordingEnabled = DM_FALSE;
}

// This is the only way to trigger RepeatOnce.
//
void PlayerPanel::PlayCallback(VkCallbackObject* /* cbObj */,
			       XtPointer /* clientData */, 
			       XtPointer /* callData */ ) 
{ 
  if (theScsiPlayerApp->player())
    theScsiPlayerApp->player()->play();
  else
    _avTransport->setStopState(True);
}


void PlayerPanel::ChangeCDTitleCallback(Widget w,
					XtPointer /* clientData */,
					XtPointer /* callData */ ) 
{ 
    char* title;
    XtVaGetValues(w, XmNvalue, &title, NULL);
    theScsiPlayerApp->database()->setTitle(title);
}

void PlayerPanel::ChangeCDArtistCallback(Widget w,
					XtPointer /* clientData */,
					XtPointer /* callData */) 
{ 
    char* artist;
    XtVaGetValues(w, XmNvalue, &artist, NULL);
    theScsiPlayerApp->database()->setArtistName(artist);
}

void PlayerPanel::ChangeTrackTitleCallback(Widget w,
					   XtPointer clientData,
					   XtPointer) 
{ 
    TrackCBData* obj = (TrackCBData*)clientData;
    char* title;
    XtVaGetValues(w, XmNvalue, &title, NULL);
    theScsiPlayerApp->database()->setTrackTitle(obj->trackNum, title);
}

void PlayerPanel::absoluteTimeChanged(VkCallbackObject *, void *clientData, 
				      void *callData)
{
  struct AvTimeChangedCallbackStruct *cbStruct;
  int hours, mins, secs, frames;
  NumberDisplay* self = (NumberDisplay*) clientData;
  cbStruct = (struct AvTimeChangedCallbackStruct *) callData;

  if (theScsiPlayerApp->player()){
    self->avTime()->getHoursMinutesSecondsFrames(cbStruct->currentTime, 
						 cbStruct->currentTimeTimeScale, &hours, &mins, &secs, &frames);
    DBG(fprintf(stderr, "ATimeChanged, new current Time is-> %d:%d:%d:%d\n",
		hours, mins, secs, frames));
    theScsiPlayerApp->player()->seekFrame(MsfToFrame(mins,secs, frames));
  }
}

void PlayerPanel::programTimeChanged(VkCallbackObject *, void *clientData, 
				      void *callData)
{
  struct AvTimeChangedCallbackStruct *cbStruct;
  int hours, mins, secs, frames;
  NumberDisplay* self = (NumberDisplay*) clientData;
  cbStruct = (struct AvTimeChangedCallbackStruct *) callData;

  if (theScsiPlayerApp->player()){

    self->avTime()->getHoursMinutesSecondsFrames(cbStruct->currentTime, 
						 cbStruct->currentTimeTimeScale, &hours, &mins, &secs, &frames);
    DBG(fprintf(stderr, "PTimeChanged, new current Time is-> %d:%d:%d:%d\n",
		hours, mins, secs, frames)); 
    ScsiPlayer* transport = theScsiPlayerApp->transport();
    theScsiPlayerApp->player()->seekFrame(MsfToFrame(mins,secs, frames)
					  + transport->trackInfo[transport->track]->start_block);
  }
}

void PlayerPanel::programSelectionChanged(VkCallbackObject *, void *clientData, 
				      void *callData)
{
  struct AvSelectionChangedCallbackStruct *cbStruct;
  int hours, mins, secs, frames;

  if (theScsiPlayerApp->player()) {

    NumberDisplay* self = (NumberDisplay*) clientData;
    cbStruct = (struct AvSelectionChangedCallbackStruct *) callData;
    SCSIPLAYER* transport = theScsiPlayerApp->transport();

    theScsiPlayerApp->player()->_selectionStart =
      mvConvertTime(cbStruct->selectionStartTime,
		    cbStruct->selectionTimeScale,
		    DeviceFramesPerSecond())
      + transport->trackInfo[transport->track]->start_block;

    theScsiPlayerApp->player()->_selectionEnd =
      mvConvertTime(cbStruct->selectionStartTime
		    + cbStruct->selectionDuration,
		    cbStruct->selectionTimeScale,
		    DeviceFramesPerSecond())
      + transport->trackInfo[transport->track]->start_block;

    thePlayerPanel->_timeline->setSelection(cbStruct->selectionStartTime,
					    cbStruct->selectionDuration,
					    cbStruct->selectionTimeScale);

    self->avTime()->getHoursMinutesSecondsFrames(cbStruct->selectionDuration, 
						 cbStruct->selectionTimeScale,
						 &hours, &mins, &secs, &frames);
    DBG(fprintf(stderr, "ProgramSelectionChanged, new duration is-> %d:%d:%d:%d\n",
		hours, mins, secs, frames)); 
  }
}

void PlayerPanel::trackChanged(VkCallbackObject *, void * /*clientData*/, void *callData)
{
  int newInt = (int)callData;
  DBG(fprintf(stderr, "Track Changed to: %d.\n", newInt));
  if (theScsiPlayerApp->player()) {
    theScsiPlayerApp->player()->seekTrack(newInt);
    cleanupTrackButtons(newInt);
  }
}







