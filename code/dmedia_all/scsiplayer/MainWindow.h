
//////////////////////////////////////////////////////////////
//
// Header file for MainWindow
//
//    This class is a ViewKit VkWindow subclass
//
// Normally, very little in this file should need to be changed.
// Create/add/modify menus using the builder.
//
// Try to restrict any changes to adding members below the
// "//---- End generated code section" markers
// Doing so will allow you to make chnages using the builder
// without losing any changes you may have made manually
//
//////////////////////////////////////////////////////////////


#ifndef MainWindow_H
#define MainWindow_H

#include <Vk/VkWindow.h>
#include <Vk/VkRadioSubMenu.h>
#include <Vk/VkRadioGroup.h>

#include "PlayerState.h"

class VkMenuItem;
class VkMenuToggle;
class VkMenuConfirmFirstAction;
class VkSubMenu;
class VkRadioSubMenu;

//---- End generated headers

#define DECK_FULL_WIDTH 866
#define DECK_PARTIAL_WIDTH 330
#define DECK_PARTIAL_HEIGHT 260
#define DECK_FULL_HEIGHT 300

//---- MainWindow class declaration

class MainWindow: public VkWindow { 

public:

  MainWindow( const char * name, 
	      ArgList args = NULL,
	      Cardinal argCount = 0 );
  ~MainWindow();
  const char *className();
  virtual Boolean okToQuit();

  virtual void stateChange(ScsiPlayer*); //  communication stuff
  void catalogUpdate();
  void clearDisplay();
  void playerOnlyView();
  
  VkMenuItem* _saveSelectionAsItem; // we alter this dynamically
  VkMenuItem* _copySelectionItem;
  VkMenuItem* _selectAllItem;
  VkMenuItem* _selectNoneItem;
  VkMenuItem* _extendSelectionRightItem;
  VkMenuItem* _extendSelectionLeftItem;

protected:

  static const int PLAYER_WIDTH;
  static const int PLAYER_HEIGHT;
  static const int TERSE_WIDTH;
  static const int TERSE_HEIGHT;
  static const int PROGRAM_WIDTH;
  static const int PROGRAM_HEIGHT;

  // Classes created by this class

  class PlayerPanel* _playerPanel;

  // Menu items created by this class
  VkSubMenu*  _filePane;
  VkMenuItem* _separator1;
  VkMenuItem* _ejectItem;
  VkMenuItem* _saveTrackAsItem;
  VkMenuItem* _exitItem;
  VkSubMenu*  _editPane;
  VkSubMenu* _viewPane;
  VkSubMenu* _utilitiesPane;
  VkMenuItem* _trackListView;
  VkSubMenu*  _optionsPane;
  VkRadioSubMenu* _deviceSubMenu;
  VkRadioSubMenu* _repeatModeSubMenu;

  VkMenuItem* _noRepeatItem;
  VkMenuItem* _repeatCdItem;
  VkMenuItem* _repeatTrackItem;

  VkMenuItem* _loopSelectionToggle;
  VkMenuItem* _playSelectionToggle;
  VkMenuItem* _scanDatItem;
  VkMenuItem* _rescanDatItem;
  VkMenuItem* _headphonesItem;

  Dimension _originalWidth;

private:

  // Callbacks to interface with Motif

  static void openFileCallback(Widget, XtPointer, XtPointer );
  static void ejectCallback(Widget, XtPointer, XtPointer );
  static void saveTrackAsCallback(Widget, XtPointer, XtPointer );
  static void saveSelectionAsCallback(Widget, XtPointer, XtPointer );
  static void quitCallback(Widget, XtPointer, XtPointer );
  static void copyCallback(Widget, XtPointer, XtPointer );
  static void selectAllCallback(Widget, XtPointer, XtPointer );
  static void selectNoneCallback(Widget, XtPointer, XtPointer );
  static void extendSelectionRightCallback(Widget, XtPointer, XtPointer );
  static void extendSelectionLeftCallback(Widget, XtPointer, XtPointer );

  static void trackListViewCallback(Widget, XtPointer, XtPointer);
  static void changeToNoRepeat(Widget, XtPointer, XtPointer);
  static void changeToRepeatCd(Widget, XtPointer, XtPointer);
  static void changeToRepeatTrack(Widget, XtPointer, XtPointer);
  static void toggleLoopSelection(Widget, XtPointer, XtPointer);
  static void togglePlaySelection(Widget, XtPointer, XtPointer);
  static void scanDatCallback(Widget, XtPointer, XtPointer);
  static void rescanDatCallback(Widget, XtPointer, XtPointer);
  static void headphonesCallback(Widget, XtPointer, XtPointer);


};
#endif







