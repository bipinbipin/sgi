////////////////////////////////////////////////////////////////////////
//
// ErrorHandler.c++
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
//
////////////////////////////////////////////////////////////////////////

#include "VideoKit.h"

#include <stdio.h>
#include <stdlib.h>

////////
//
// dmError
//
////////

void ErrorHandler::dmError(
    )
{
    int errorNum;
    char detail[DM_MAX_ERROR_DETAIL];
    const char* message;
    message = dmGetError( &errorNum, detail );
    assert( message != NULL );
    fprintf( stderr, "ERROR: %s\n", message );
    fprintf( stderr, "    %s\n", detail );
    ErrorHandler::exit();
}

////////
//
// error
//
////////

void ErrorHandler::error(
    const char* message
    )
{
    fprintf( stderr, "ERROR: %s\n", message );
    ErrorHandler::exit();
}

////////
//
// exit
//
////////

void ErrorHandler::exit(
    )
{
    ::exit( -1 );
}
    
////////
//
// vlError
//
////////

void ErrorHandler::vlError(
    )
{
    vlPerror( "ERROR: " );
    ErrorHandler::exit();
}

