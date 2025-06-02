////////////////////////////////////////////////////////////////////////
//
// VideoIn.c++
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

class VideoInImpl : public VideoIn
{
public:
    
    //
    // Constructor (see below).
    //

    VideoInImpl(
	int device,
	int node,
	Params* format
	);
    
    //
    // Destructor
    //

    ~VideoInImpl()
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

    Ref<Buffer> getFrame();
    
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

    void setPool( Pool* pool )
    {
	VC( vlDMPoolRegister( Global::videoServer, myPath, myDrain, 
			      pool->dmpool() ) );
    }
    
    
private:
    Ref<Params>	myFormat;
    VLNode	mySource;
    VLNode	myDrain;
    VLPath	myPath;
    USTMSCpair	myPrevTimestamp;
    
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

VideoInImpl::VideoInImpl(
    int device,
    int node,
    Params* format
    )
{
    myFormat = format;
    myPrevTimestamp.msc = 0;
    myPrevTimestamp.ust = 0;
    
    //
    // Create the nodes.
    //

    mySource = vlGetNode( Global::videoServer, VL_SRC, VL_VIDEO, node );
    if ( mySource == -1 )
    {
	ErrorHandler::vlError(); 
    }
    myDrain = vlGetNode( Global::videoServer, VL_DRN, VL_MEM, VL_ANY );
    if ( myDrain == -1 )
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
    this->setControl( myDrain, VL_CAP_TYPE, &value );
    
    value.intVal = myFormat->vlFormat();
    this->setControl( myDrain, VL_FORMAT, &value );
    
    value.intVal = myFormat->vlPacking();
    this->setControl( myDrain, VL_PACKING, &value );
    
    value.xyVal.x = myFormat->width();
    value.xyVal.y = myFormat->height();
    this->setControl( myDrain, VL_SIZE, &value );
    
    value.xyVal.x = 0;
    value.xyVal.y = -2;
    this->setControl( myDrain, VL_OFFSET, &value );
    
    value.intVal = myFormat->vlLayout();
    this->setControl( myDrain, VL_LAYOUT, &value );

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

void VideoInImpl::getControl(
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
// getFrame
//
////////

Ref<Buffer> VideoInImpl::getFrame(
    )
{
    VLEvent event;
    int vlstatus;
	
    //
    // Get the next event.
    //

    vlstatus = vlEventRecv( Global::videoServer, myPath, &event );
    while ( vlstatus == -1 && vlGetErrno() == VLAgain )
    {
	// XXX should use select
	sginap( 1 );
	vlstatus = vlEventRecv( Global::videoServer, myPath, &event );
    }
    if ( vlstatus != 0 )
    {
	ErrorHandler::vlError();
    }
    if ( event.reason != VLTransferComplete )
    {
	ErrorHandler::error( "Unexpected video event" );
    }
    
    //
    // Get the buffer from the event.
    //

    DMbuffer buffer;
    vlstatus = vlEventToDMBuffer( &event, &buffer );
    Ref<Buffer> frame = Buffer::wrap( buffer, myFormat );
    
    //
    // Was a frame dropped.
    //
    
    {
	USTMSCpair timestamp = frame->timestamp();
	if ( myPrevTimestamp.msc != 0 )
	{
	    if ( timestamp.msc != myPrevTimestamp.msc + 1 )
	    {
		printf( "VideoIn: %lld frames dropped\n", 
			timestamp.msc - ( myPrevTimestamp.msc + 1 ) );
	    }
	}
	myPrevTimestamp = timestamp;
    }
    
    //
    // That's about it.
    //

    return frame;
}

////////
//
// setControl
//
////////

void VideoInImpl::setControl(
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

Ref<VideoIn> VideoIn::create(
    int device,
    int node,
    Params* format
    )
{
    return new VideoInImpl( device, node, format );
}

////////
//
// getDefaultFieldSize
//
////////

void VideoIn::getDefaultFieldSize(
    int device,
    int node,
    int* returnWidth,
    int* returnHeight
    )
{
    //
    // Create the nodes.
    //

    VLNode source = vlGetNode( Global::videoServer, VL_SRC, VL_VIDEO, node );
    if ( source == -1 )
    {
	ErrorHandler::vlError(); 
    }
    VLNode drain = vlGetNode( Global::videoServer, VL_DRN, VL_MEM, VL_ANY );
    if ( drain == -1 )
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
    // Tell it we want fields.
    //
    
    VLControlValue value;
    value.intVal = VL_CAPTURE_NONINTERLEAVED;
    VC( vlSetControl( Global::videoServer, path, drain, VL_CAP_TYPE, &value ));

    //
    // Get the size.
    //
    
    VC( vlGetControl( Global::videoServer, path, drain, VL_SIZE, &value ) );

    //
    // Clean up and return the size.
    //

    vlDestroyPath( Global::videoServer, path );
    *returnWidth  = value.xyVal.x;
    *returnHeight = value.xyVal.y;
}

////////
//
// getDefaultFrameSize
//
////////

void VideoIn::getDefaultFrameSize(
    int device,
    int node,
    int* returnWidth,
    int* returnHeight
    )
{
    //
    // Create the nodes.
    //

    VLNode source = vlGetNode( Global::videoServer, VL_SRC, VL_VIDEO, node );
    if ( source == -1 )
    {
	ErrorHandler::vlError(); 
    }
    VLNode drain = vlGetNode( Global::videoServer, VL_DRN, VL_MEM, VL_ANY );
    if ( drain == -1 )
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
    VC( vlGetControl( Global::videoServer, path, drain, VL_SIZE, &value ) );

    //
    // Clean up and return the size.
    //

    vlDestroyPath( Global::videoServer, path );
    *returnWidth  = value.xyVal.x;
    *returnHeight = value.xyVal.y;
}

