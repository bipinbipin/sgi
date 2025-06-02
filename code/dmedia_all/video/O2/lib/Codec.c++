////////////////////////////////////////////////////////////////////////
//
// Codec.c++
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

#include <dmedia/dm_imageconvert.h>

////////////////////////////////////////////////////////////////////////
//
// Implementation class
//
////////////////////////////////////////////////////////////////////////

class CodecImpl : public Codec
{
public:
    
    //
    // Constructor (see below).
    //

    CodecImpl(
	Params* srcFormat,
	Params* dstFormat
	);
    
    //
    // Destructor
    //

    ~CodecImpl()
    {
	dmICDestroy( myConverter );
    }
    
    //
    // putFrame
    //

    void putFrame( Buffer* );
    
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
	DC( dmICGetSrcPoolParams( myConverter, params ) );
	DC( dmICGetDstPoolParams( myConverter, params ) );
	bufferCount += 1;
	DC( dmParamsSetInt( params, DM_BUFFER_COUNT, bufferCount ) );
	int bufferSize = dmImageFrameSize( mySrcFormat->dmparams() );
	if ( dmParamsGetInt( params, DM_BUFFER_SIZE ) < bufferSize )
	{
	    DC( dmParamsSetInt( params, DM_BUFFER_SIZE, bufferSize ) );
	}
    }
    
    //
    // setPool
    //

    void setPool( Pool* pool )
    {
	DC( dmICSetDstPool( myConverter, pool->dmpool() ) );
    }
    
    
private:
    Ref<Params>		mySrcFormat;
    Ref<Params>		myDstFormat;
    DMimageconverter 	myConverter;
};

////////
//
// Constructor
//
////////

CodecImpl::CodecImpl(
    Params* srcFormat,
    Params* dstFormat
    )
{
    mySrcFormat = srcFormat;
    myDstFormat = dstFormat;
    
    //
    // Set up the conversion parameters.  (We don't have any.)
    //

    Ref<Params> convParams = Params::create();
    dmParamsSetEnum( 
	convParams->dmparams(),
	DM_IC_SPEED,
	DM_IC_SPEED_REALTIME
	);
    
    //
    // Find the right image converter.
    //

    int converterID = dmICChooseConverter(
	mySrcFormat->dmparams(),
	myDstFormat->dmparams(),
	convParams->dmparams()
	);
    if ( converterID == -1 )
    {
	ErrorHandler::error( "No matching image converter could be found." ); 
    }
    DC( dmICCreate( converterID, &myConverter ) );
    
#if 0
    {
	Ref<Params> desc = Params::create();
	DC( dmICGetDescription( converterID, desc->dmparams() ) );
	printf( "Chosen converter:\n" );
	desc->print();
    }
#endif
    
    //
    // Configure it.
    //

    DC( dmICSetSrcParams ( myConverter, mySrcFormat->dmparams() ) );
    DC( dmICSetDstParams ( myConverter, myDstFormat->dmparams() ) );
    DC( dmICSetConvParams( myConverter, convParams ->dmparams() ) );
}

////////
//
// getFrame
//
////////

Ref<Buffer> CodecImpl::getFrame(
    )
{
    //
    // Wait until the answer is ready.
    //
    
    {
	int fd = dmICGetDstQueueFD( myConverter );
	fd_set readfds;
	FD_ZERO( &readfds );
	FD_SET( fd, &readfds );
	int s = select( fd+1, &readfds, NULL, NULL, NULL );
	if ( s != 1 )
	{
	    ErrorHandler::error( 
		"select failed waiting for image converter"
		);
	}
    }
    
    //
    // Get the buffer.
    //

    DMbuffer buffer;
    DC( dmICReceive( myConverter, &buffer ) );
    return Buffer::wrap( buffer, myDstFormat );
}

////////
//
// putFrame
//
////////

void CodecImpl::putFrame(
    Buffer* frame
    )
{
    DC( dmICSend( myConverter, frame->dmbuffer(), 0, NULL ) );
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

Ref<Codec> Codec::create(
    Params*	srcFormat,
    Params*	dstFormat
    )
{
    return new CodecImpl( srcFormat, dstFormat );
}

#if 0
////////
//
// createCompressor
//
////////

Ref<Codec> Codec::createCompressor(
    Params*	imageFormat,
    const char*	compressionScheme
    )
{
    Ref<Params> compressedFormat = Params::copy( imageFormat );
    dmParamsSetString(
	compressedFormat->dmparams(),
	DM_IMAGE_COMPRESSION,
	compressionScheme
	);
    
    return new CodecImpl( imageFormat, compressedFormat );
}

////////
//
// createDecompressor
//
////////

Ref<Codec> Codec::createDecompressor(
    Params*	imageFormat,
    const char*	compressionScheme
    )
{
    Ref<Params> compressedFormat = Params::copy( imageFormat );
    dmParamsSetString(
	compressedFormat->dmparams(),
	DM_IMAGE_COMPRESSION,
	compressionScheme
	);
    
    return new CodecImpl( compressedFormat, imageFormat );
}
#endif
