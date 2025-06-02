////////////////////////////////////////////////////////////////////////
//
// PlugIns.c++
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
//
////////////////////////////////////////////////////////////////////////

#include "PlugIns.h"


////////
//
// PlugIns Constructor
//
// Creates a filter manager, and a transition manager.
//
////////

PlugIns::PlugIns(
    Widget w)
{
    myFilters = dmPMCreateManager(w);
    myTransitions = dmPMCreateManager(w);

    const char *dirName = getDefaultDirectoryName();

    if (myFilters != NULL)
    {
	dmPMInitDirectory(myFilters, dirName, DM_VIDEO_FILTER);
	dmPMSortByName(myFilters);
    }

    if (myTransitions != NULL)
    {
	dmPMInitDirectory(myTransitions, dirName, DM_VIDEO_TRANSITION);
	dmPMSortByName(myTransitions);
    }
}


////////
//
// PlugIns destructor
//
////////

PlugIns::~PlugIns()
{
    if (myFilters != NULL)
    {
	dmPMDestroyManager(myFilters);
	myFilters = NULL;
    }

    if (myTransitions != NULL)
    {
	dmPMDestroyManager(myTransitions);
	myTransitions = NULL;
    }
}

////////
//
// getFilterManager:
//
////////

DMplugmgr * PlugIns::getFilterManager()
{
    return myFilters;
}


////////
//
// getTransitionManager:
//
////////

DMplugmgr * PlugIns::getTransitionManager()
{
    return myTransitions;
}


////////
//
// getDefaultDirectoryName
//
// returns the location of the plug in directory
//
////////

const char * PlugIns::getDefaultDirectoryName(void)
{
    const char *defaultDir = NULL;

    if ((defaultDir = getenv("PREMIERE_PLUGIN_DIR")) != NULL)
    {
	return defaultDir;
    }

    //
    // Let the plug in manager decide which directories to pull in...
    //

    return NULL;
}
