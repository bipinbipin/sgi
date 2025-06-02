////////////////////////////////////////////////////////////////////////
//
// VideoOut.c++
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

#include <unistd.h>	// sginap()

////////////////////////////////////////////////////////////////////////
//
// Implementation class
//
////////////////////////////////////////////////////////////////////////

class VideoOutImpl : public VideoOut
{
public:
    
    //
    // Constructor (see below).
    //

    VideoOutImpl(
	int device,
	int node,
	Params* format
	);
    
    //
    // Destructor
    //

    ~VideoOutImpl()
    {
	vlDestroyPath( Global::videoServer, myPath );
    }
    
    //
    // start
    //

    void start()
    {
	VC( vlBeginTransfer( Global::videoServer, myPath, 0, NULL ) );
    }

    //
    // getFrame
    //

    void putFrame( Buffer* buffer )
    {
	VC( vlDMBufferSend( Global::videoServer, myPath, buffer->dmbuffer() ));
    }
    
    //
    // getPoolRequirements
    //

    void getPoolRequirements( Params* poolSpec )
    {
	DMparams* params = poolSpec->dmparams();
	int bufferCount = dmParamsGetInt( params, DM_BUFFER_COUNT );
	DC( dmParamsSetInt( params, DM_BUFFER_COUNT, 0 ) );
	VC( vlDMPoolGetParams( Global::videoServer, myPath, myDrain, params ));
	bufferCount += dmParamsGetInt( params, DM_BUFFER_COUNT );
	DC( dmParamsSetInt( params, DM_BUFFER_COUNT, bufferCount ) );
    }
    
    //
    // setPool
    //

    void setPool( Pool* )
    {
    }
    
    //
    // frontier
    //

    USTMSCpair frontier()
    {
	stamp_t msc = 
	    vlGetFrontierMSC( Global::videoServer, myPath, mySource );
	double ust_per_msc = 
	    vlGetUSTPerMSC( Global::videoServer, myPath, mySource );
	USTMSCpair pair;
	vlGetUSTMSCPair( Global::videoServer, myPath,
			 myDrain, VL_ANY,
			 mySource, &pair );
	stamp_t ust = pair.ust + ( msc - pair.msc ) * ust_per_msc;
	
	USTMSCpair front;
	front.msc = msc;
	front.ust = ust;
	return front;
    }
    
private:
    Ref<Params>	myFormat;
    VLNode	mySource;
    VLNode	myDrain;
    VLPath	myPath;
    
    void getControl(
	VLNode		node,
	VLControlType	control,
	VLControlValue*	value
	);
    
    void setControl(
	VLNode		node,
	VLControlType	control,
	VLControlValue*	value
	);
};

////////
//
// Constructor
//
////////

VideoOutImpl::VideoOutImpl(
    int device,
    int node,
    Params* format
    )
{
    myFormat = format;
    
    //
    // Create the nodes.
    //

    myDrain = vlGetNode( Global::videoServer, VL_DRN, VL_VIDEO, node );
    if ( myDrain == -1 )
    {
	ErrorHandler::vlError(); 
    }
    mySource = vlGetNode( Global::videoServer, VL_SRC, VL_MEM, VL_ANY );
    if ( mySource == -1 )
    {
	ErrorHandler::vlError(); 
    }

    //
    // Create the path.
    //

    myPath = vlCreatePath( Global::videoServer, device, mySource, myDrain );
    if ( myPath == -1 )
    {
	ErrorHandler::vlError(); 
    }
    VC( vlSetupPaths( Global::videoServer, &myPath, 1, VL_SHARE, VL_SHARE ) );
    
    //
    // Leave the default timing and rate (from vcp). 
    // Set the other controls:
    //
    
    VLControlValue value;
    value.intVal = VL_CAPTURE_INTERLEAVED;  // frames
    if ( dmParamsIsPresent( myFormat->dmparams(), DM_CAPTURE_TYPE ) )
    {
	switch( dmParamsGetEnum( myFormat->dmparams(), DM_CAPTURE_TYPE ) )
	{
	    case DM_CAPTURE_TYPE_FIELDS:
		value.intVal = VL_CAPTURE_NONINTERLEAVED;
		break;
		
	    case DM_CAPTURE_TYPE_FRAMES:
		value.intVal = VL_CAPTURE_INTERLEAVED;
		break;
		
	    default:
		ErrorHandler::error( "bad DM_CAPTURE_TYPE" );
	}
    }
    this->setControl( mySource, VL_CAP_TYPE, &value );
    
    value.intVal = myFormat->vlFormat();
    this->setControl( mySource, VL_FORMAT, &value );
    
    value.intVal = myFormat->vlPacking();
    this->setControl( mySource, VL_PACKING, &value );
    
    value.xyVal.x = myFormat->width();
    value.xyVal.y = myFormat->height();
    this->setControl( mySource, VL_SIZE, &value );
    
    value.xyVal.x = 0;
    value.xyVal.y = -2;
    this->setControl( mySource, VL_OFFSET, &value );
    
    value.intVal = myFormat->vlLayout();
    this->setControl( mySource, VL_LAYOUT, &value );

    //
    // Specify which events we want.
    //

    VLEventMask mask = VLTransferCompleteMask | VLTransferFailedMask;
    VC( vlSelectEvents( Global::videoServer, myPath, mask ) );
}

////////
//
// getControl
//
////////

void VideoOutImpl::getControl(
    VLNode		node,
    VLControlType	control,
    VLControlValue*	value
    )
{
    if ( vlGetControl( Global::videoServer, myPath, node, control, value ) != 0 )
    {
	/****
	VLControlInfo* info =
	    vlGetControlInfo( Global::videoServer, myPath, node, control ); 
	assert( info != NULL );
	fprintf( stderr, "error getting control %s\n", info->name );
	****/

	fprintf( stderr, "error getting control %d\n", control );
	ErrorHandler::vlError();
    }
}

////////
//
// setControl
//
////////

void VideoOutImpl::setControl(
    VLNode		node,
    VLControlType	control,
    VLControlValue*	value
    )
{
    if ( vlSetControl( Global::videoServer, myPath, node, control, value ) != 0 )
    {
	/****
	VLControlInfo* info =
	    vlGetControlInfo( Global::videoServer, myPath, node, control ); 
	assert( info != NULL );
	fprintf( stderr, "error setting control %s\n", info->name );
	****/

	fprintf( stderr, "error setting control %d\n", control );
	ErrorHandler::vlError();
    }
}

////////////////////////////////////////////////////////////////////////
//
// Static Members
//
////////////////////////////////////////////////////////////////////////

////////
//
// create
//
////////

Ref<VideoOut> VideoOut::create(
    int device,
    int node,
    Params* format
    )
{
    return new VideoOutImpl( device, node, format );
}

////////
//
// getDefaultSize
//
////////

void VideoOut::getDefaultSize(
    int device,
    int node,
    int* returnWidth,
    int* returnHeight
    )
{
    //
    // Create the nodes.
    //

    VLNode drain = vlGetNode( Global::videoServer, VL_DRN, VL_VIDEO, node );
    if ( drain == -1 )
    {
	ErrorHandler::vlError(); 
    }
    VLNode source = vlGetNode( Global::videoServer, VL_SRC, VL_MEM, VL_ANY );
    if ( source == -1 )
    {
	ErrorHandler::vlError(); 
    }

    //
    // Create the path.
    //

    VLPath path = vlCreatePath( Global::videoServer, device, source, drain );
    if ( path == -1 )
    {
	ErrorHandler::vlError(); 
    }
    VC( vlSetupPaths( Global::videoServer, &path, 1, VL_SHARE, VL_SHARE ) );
    
    //
    // Get the size.
    //
    
    VLControlValue value;
    VC( vlGetControl( Global::videoServer, path, source, VL_SIZE, &value ) );

    //
    // Clean up and return the size.
    //

    vlDestroyPath( Global::videoServer, path );
    *returnWidth  = value.xyVal.x;
    *returnHeight = value.xyVal.y;
}
