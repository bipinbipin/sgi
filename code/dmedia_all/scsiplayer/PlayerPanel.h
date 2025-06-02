//////////////////////////////////////////////////////////////
//
// Header file for PlayerPanel
//
//////////////////////////////////////////////////////////////

#ifndef PlayerPanel_H
#define PlayerPanel_H

#include <Vk/VkComponent.h>
#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>

#include <AvTransport.h>
#include <AvVolume.h>
#include <AvTimeline.h>

#include "TrackBin.h"
#include "PlayerState.h"
#include "NumberDisplay.h"
#include "MainWindow.h"

// Externally defined classes referenced by this class:

typedef struct {
  int     buttonNum;
  void*   next;
} ButtonList;


class PlayerPanel;

extern PlayerPanel* thePanel;	// global pointer to the main panel

//---- PlayerPanel class declaration


class PlayerPanel : public VkComponent
{

public:

  PlayerPanel(const char *, Widget);
  PlayerPanel(const char *);
  ~PlayerPanel();
  const char *  className();

  void stateChange(ScsiPlayer*); 
  void updateDisplay(ScsiPlayer*);
  void clearDisplay();

  void selectAll();
  void selectNone();
  void extendSelection(Boolean extendRight);

  Widget _controlPanel;
  AvTimeline* _timeline;

  Boolean _audioMediaLoaded;	// flags whether or not anything is in the drive.

protected:

  // constants in the right namespace

  static const int SPACING;
  static const int BORDER_SPACING;
  static const int BUTTON_SIZE;
  static const float PRIMARY_DISPLAY_SIZE;
  static const float SECONDARY_DISPLAY_SIZE;
  static const int SECONDARY_DISPLAY_HEIGHT;
  static const int TRACKS_PER_ROW;
  static const int TIMESCALE;

  // Classes created by this class

    
  // Widgets created by this class

  Widget _buttonSpringBox;
  Widget _logoLabel;
  Widget _trackGridFrame;
  Widget _trackGridRC;
  Widget _trackButtons[1000];

  TrackBin* _trackBin;

  NumberDisplay* _trackNumberDisplay;
  NumberDisplay* _indexDisplay;
  NumberDisplay* _programTimeDisplay;
  NumberDisplay* _absoluteTimeDisplay;
  NumberDisplay* _remainingTimeDisplay;
  NumberDisplay* _smpteTimeDisplay;

  AvTransport* _avTransport;
  AvVolume* _volume;

  VkOptionMenu* _panelTimeTypeOption;
  MainWindow* _mainWindow;

private:

  // these guys are used as cached for updateDisplay

  int _trackNumber;

  Widget _artistTextF;
  Widget _titleTextF;
  Widget _trackInfoForm;
  Widget _titleForm;
  Widget _trackHeadingForm;
  Widget _trackListRxC;
  Widget _trackListScroll;

  void displaySetup(ScsiPlayer* transport);
  void displayGrid(ScsiPlayer* transport);
  int addGridButton(int trackNum);
  void addTrackListElement(int trackNum);
  void adjustShell(Widget shell, Dimension beforeHt, Dimension afterHt); 

  // void databaseSetup(ScsiPlayer* transport);
  void setupTrackPanel();
  void makeTrackBin();
  void populateTrackBin();

  virtual void timelineLocate(VkCallbackObject *, void *, void * );
  virtual void timelineSelection(VkCallbackObject *, void *, void * );
  virtual void volumeChanged(VkCallbackObject *, void *, void *);

  void RtzCallback (VkCallbackObject*, XtPointer, XtPointer );
  void PreviousTrackCallback (VkCallbackObject*, XtPointer, XtPointer );
  void NextTrackCallback(VkCallbackObject*, XtPointer, XtPointer);
  void StopCallback (VkCallbackObject*, XtPointer, XtPointer );
  void PlayCallback (VkCallbackObject*, XtPointer, XtPointer );

  // items to make the track button grid work right.
  int _depressedButtonVector[1000];
  void cleanupTrackButtons(int);
  static void CueTrackValueChangedCallback(Widget, XtPointer, XtPointer );

  static void ChangeCDTitleCallback(Widget, XtPointer, XtPointer );
  static void ChangeCDArtistCallback(Widget, XtPointer, XtPointer );
  static void ChangeTrackTitleCallback(Widget, XtPointer, XtPointer );

  void programTimeChanged(VkCallbackObject *, void *clientData, void *callData);
  void programSelectionChanged(VkCallbackObject *, void *clientData, void *callData);
  void absoluteTimeChanged(VkCallbackObject *, void *clientData, void *callData);
  void trackChanged(VkCallbackObject *, void *clientData, void *callData);

};


#endif
