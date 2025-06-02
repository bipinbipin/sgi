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
 *  This is the header file for the "ButtonBox" event.
 *------------------------------------------------------------*/

#include "ButtonBoxEvent.h"


SO_EVENT_SOURCE(ButtonBoxEvent);


////////////////////////////////////////////////////////////////////////
//
// Class initialization
//
// SoINTERNAL public
//
void
ButtonBoxEvent::initClass()
//
////////////////////////////////////////////////////////////////////////
{
   SO_EVENT_INIT_CLASS(ButtonBoxEvent, SoButtonEvent);
}

////////////////////////////////////////////////////////////////////////
//
// Constructor
//
ButtonBoxEvent::ButtonBoxEvent()
//
////////////////////////////////////////////////////////////////////////
{
   button = 0;
}

////////////////////////////////////////////////////////////////////////
//
// Convenience routine - this returns TRUE if the event is a button box
// press event matching the passed button.
//
// static public
//
SbBool
ButtonBoxEvent::isButtonPressEvent(const SoEvent *e, int whichButton)
//
////////////////////////////////////////////////////////////////////////
{
   SbBool isMatch = FALSE;
   
   // is it a button box event?
   if (e->isOfType(ButtonBoxEvent::getClassTypeId())) {
      const ButtonBoxEvent *be = (const ButtonBoxEvent *) e;
   
      // is it a press event?
      if (be->getState() == SoButtonEvent::DOWN) {
   
         // did the caller want any button press? or do they match?
         if ((whichButton == -1) ||
             (be->getButton() == whichButton))
            isMatch = TRUE;
      }
   }
   
   return isMatch;
}

////////////////////////////////////////////////////////////////////////
//
// Convenience routine - this returns TRUE if the event is a button box
// release event matching the passed button.
//
// static public
//
SbBool
ButtonBoxEvent::isButtonReleaseEvent(const SoEvent *e, int whichButton)
//
////////////////////////////////////////////////////////////////////////
{
   SbBool isMatch = FALSE;
   
   // is it a button box event?
   if (e->isOfType(ButtonBoxEvent::getClassTypeId())) {
      const ButtonBoxEvent *be = (const ButtonBoxEvent *) e;
   
      // is it a release event?
      if (be->getState() == SoButtonEvent::UP) {
      
         // did the caller want any button release? or do they match?
         if ((whichButton == -1) ||
             (be->getButton() == whichButton))
            isMatch = TRUE;
      }
   }
   
   return isMatch;
}

