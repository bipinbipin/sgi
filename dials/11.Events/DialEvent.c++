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
 *  This is the source file for the "Dial" event.
 *------------------------------------------------------------*/

#include "DialEvent.h"

SO_EVENT_SOURCE(DialEvent);

////////////////////////////////////////////////////////////////////////
//
// Class initialization
//
// SoINTERNAL public
//
void
DialEvent::initClass()
//
////////////////////////////////////////////////////////////////////////
{
   SO_EVENT_INIT_CLASS(DialEvent, SoEvent);
}

////////////////////////////////////////////////////////////////////////
//
// Constructor
//
DialEvent::DialEvent()
//
////////////////////////////////////////////////////////////////////////
{
   dial = 0;
   value = 0;
}

////////////////////////////////////////////////////////////////////////
//
// Convenience routine - this returns TRUE if the event is a dial
// turn event matching the passed dial.
//
// static public
//
SbBool
DialEvent::isDialEvent(const SoEvent *e, int whichDial)
//
////////////////////////////////////////////////////////////////////////
{
   SbBool isMatch = FALSE;
   
   // is it a dial event?
   if (e->isOfType(DialEvent::getClassTypeId())) {
      const DialEvent *de = (const DialEvent *) e;
   
      // did the caller want any dial turn? or do they match?
      if ((whichDial == -1) ||
          (de->getDial() == whichDial))
         isMatch = TRUE;
   }
   
   return isMatch;
}


