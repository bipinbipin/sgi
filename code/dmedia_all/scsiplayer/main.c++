//****************************************************************************
//
//  Copyright 1995, Silicon Graphics, Inc.
//  All Rights Reserved.
//
//  This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
//  the contents of this file may not be disclosed to third parties, copied or
//  duplicated in any form, in whole or in part, without the prior written
//  permission of Silicon Graphics, Inc.
//
//  RESTRICTED RIGHTS LEGEND:
//  Use, duplication or disclosure by the Government is subject to restrictions
//  as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
//  and Computer Software clause at DFARS 252.227-7013, and/or in similar or
//  successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
//  rights reserved under the Copyright Laws of the United States.
// 
//****************************************************************************

#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <locale.h>
#include "ScsiPlayerApp.h"		// derived from VkApp
#include "MainWindow.h"
#include "PlayerState.h"

// As per Shoji.
//
static String myLangProc(Display *, String xnl, XtPointer)
{
  if (! setlocale(LC_ALL, xnl))
    XtWarning("locale not supported by C library, locale unchanged");
  if (! XSupportsLocale()) {
    XtWarning("locale not supported by Xlib, locale set to C");
    setlocale(LC_ALL, "C");
  }
  if (! XSetLocaleModifiers(""))
    XtWarning("X locale modifiers not supported, using default");
  setlocale(LC_NUMERIC, "C");
  return setlocale(LC_ALL, NULL);
}

void main (int argc, char** argv )
{
  ScsiPlayerApp* app;

  setreuid(-1, getuid());	// drop privileges pronto

  // dsdebug = 1;   // if you need a scsi trace
  // Do some Xt initialization for internationalization
  XtSetLanguageProc(NULL, myLangProc, NULL);
  // XInitThreads();

  app = new ScsiPlayerApp(((strcmp(basename(argv[0]), "cdplayer") == 0)
			   || (strcmp(basename(argv[0]), "cdman") == 0))
			  ? "CdPlayer"
			  : "DatPlayer",
			  &argc, argv); 

  // Create the top level windows

  MainWindow* mainWindow  = new MainWindow("ScsiPlayer");
    
  app->initialize(app->scsiDevice()); 

  if (!app->_audioMediaLoaded) {
    app->show();
    if (!app->_displayTrackList)
      mainWindow->playerOnlyView();
  }

  app->run();

}
