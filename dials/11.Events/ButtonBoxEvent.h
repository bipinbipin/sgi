/*-------------------------------------------------------------
 *  This is an example from The Inventor Toolmaker,
 *  chapter 11.
 *
 *  This is the header file for the "ButtonBox" event.
 *------------------------------------------------------------*/


#ifndef  _BUTTON_BOX_EVENT_
#define  _BUTTON_BOX_EVENT_

#include <Inventor/SbBasic.h>
#include <Inventor/events/SoButtonEvent.h>
#include <Inventor/events/SoSubEvent.h>

// some convenience macros for determining if an event matches
#define BUTTON_BOX_PRESS_EVENT(EVENT,BUTTON) \
   (ButtonBoxEvent::isButtonPressEvent(EVENT,BUTTON))

#define BUTTON_BOX_RELEASE_EVENT(EVENT,BUTTON) \
   (ButtonBoxEvent::isButtonReleaseEvent(EVENT,BUTTON))

// The ButtonBoxEvent class
class ButtonBoxEvent : public SoButtonEvent {

   SO_EVENT_HEADER();
   
  public:
   // constructor
   ButtonBoxEvent();
   
   // which button generated the event, e.g. ButtonBoxEvent::BUTTON1
   void     setButton(int b) { button = b; }
   int	    getButton() const  { return button; }
   
   // convenience routines to see if an SoEvent is a press or release
   // of the passed button box button. Passing -1 matches any button.
   static SbBool         isButtonPressEvent(
                           const SoEvent *e,
                           int whichButton = -1);
			    
   static SbBool         isButtonReleaseEvent(
                           const SoEvent *e,
                           int whichButton = -1);
    
  SoINTERNAL public:
   static void		initClass();
   
  private:
   int   		button;		    // which button
};

#endif /* _BUTTON_BOX_EVENT_ */
