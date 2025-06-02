/*-------------------------------------------------------------
 *  This is an example from The Inventor Toolmaker,
 *  chapter 11.
 *
 *  This is the header file for the "Dial" event.
 *------------------------------------------------------------*/


#ifndef  _DIAL_EVENT_
#define  _DIAL_EVENT_

#include <Inventor/SbBasic.h>
#include <Inventor/events/SoEvent.h>
#include <Inventor/events/SoSubEvent.h>

// convenience macro for determining if an event matches
#define DIAL_EVENT(EVENT, WHICH) \
   (DialEvent::isDialEvent(EVENT, WHICH))


// The DialEvent class
class DialEvent : public SoEvent {

   SO_EVENT_HEADER();
   
  public:
   // constructor
   DialEvent();
   
   // which dial generated the event, 1-8
   void     setDial(int d)   { dial = d; }
   int	    getDial() const  { return dial; }
    
   // value of the dial turned
   void     setValue(int v)  { value = v; }
   int	    getValue() const { return value; }
   
   // convenience routines to see if an SoEvent is a turn of
   // the passed dial. Passing -1 matches any button.
   static SbBool	isDialEvent(const SoEvent *e, int which = -1);
   
  SoINTERNAL public:
   static void		initClass();
   
  private:
   int   		dial;		    // which dial
   int			value;		    // value of dial
};

#endif /* _DIAL_EVENT_ */
