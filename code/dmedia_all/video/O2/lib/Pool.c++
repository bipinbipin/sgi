////////////////////////////////////////////////////////////////////////
//
// Pool.c++
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

#define MAX_USERS 10

// #define DEBUG_POOL_PARAMS

////////
//
// Implementation class
//
////////

class PoolImpl : public Pool
{
public:
    //
    // Constructor
    //

    PoolImpl(
	int bufferCount,
	DMboolean cacheable
	)
    {
	myParams	= Params::poolSpec( bufferCount, cacheable );
	myPool		= NULL;
	myUserCount	= 0;
	myMinBufferSize = 0;
    }
    
    //
    // Destructor
    //

    ~PoolImpl()
    {
	dmBufferDestroyPool( myPool );
    }
    
    //
    // addUser
    //

    void addUser( PoolUser* user )
    {
	if ( MAX_USERS <= myUserCount )
	{
	    ErrorHandler::error( "too many pool users" );
	}
	myUsers[myUserCount++] = user;
    }
    
    //
    // allocatePool
    //

    void allocatePool();
    
    //
    // allocateBuffer
    //

    Ref<Buffer> allocateBuffer(
	Params* format
	)
    {
	DMbuffer buffer;
	DMstatus s = dmBufferAllocate( myPool, &buffer );
	if ( s != DM_SUCCESS )
	{
	    printf( "waiting for buffer...\n" );
	    dmBufferSetPoolSelectSize(
		myPool,
		dmBufferGetAllocSize( myPool )
		);
	    int fd = dmBufferGetPoolFD( myPool );
	    fd_set writefds;
	    FD_ZERO( &writefds );
	    FD_SET( fd, &writefds );
	    int ready = select( fd+1, NULL, &writefds, NULL, NULL );
	    if ( ready != 1 )
	    {
		ErrorHandler::error( "select on pool failed\n" );
	    }
	    s = dmBufferAllocate( myPool, &buffer );
	    printf( "    select came back.\n" );
	}
	DC( s );
	printf( "got buffer\n" );
	return Buffer::wrap( buffer, format );
    }

    //
    // dmpool
    //

    DMbufferpool dmpool()
    {
	return myPool;
    }
    
        
    //
    // setMinBufferSize
    //

    void setMinBufferSize( int size )
    {
	myMinBufferSize = size;
    }
    
    
private:
    Ref<Params>		myParams;
    DMbufferpool	myPool;
    Ref<PoolUser>	myUsers[MAX_USERS];
    int			myUserCount;
    int			myMinBufferSize;
};

////////
//
// allocatePool
//
////////

void PoolImpl::allocatePool(
    )
{
    DMparams* params = myParams->dmparams();
    
#ifdef DEBUG_POOL_PARAMS
    printf( "\nStart:\n" );
    myParams->print();
#endif
    
    //
    // Get the pool requirements from all users, summing the number of
    // buffers required.
    //
    
    {
	int bufferCount = dmParamsGetInt( params, DM_BUFFER_COUNT );
	DC( dmParamsSetInt( params, DM_BUFFER_COUNT, 0 ) );
	for ( int i = 0;  i < myUserCount;  i++ )
	{
	    myUsers[i]->getPoolRequirements( myParams );
#ifdef DEBUG_POOL_PARAMS
	    printf( "\ni = %d:\n", i );
	    myParams->print();
#endif
	    bufferCount += dmParamsGetInt( params, DM_BUFFER_COUNT );
	    DC( dmParamsSetInt( params, DM_BUFFER_COUNT, 0 ) );
	}

	DC( dmParamsSetInt( params, DM_BUFFER_COUNT, bufferCount ) );

#ifdef DEBUG_POOL_PARAMS
	printf( "\nFinal:\n", i );
	myParams->print();
#endif
    }
    
    //
    // Make sure that the buffers are as large as requested.
    //

    if ( dmParamsGetInt( params, DM_BUFFER_SIZE ) < myMinBufferSize )
    {
	DC( dmParamsSetInt( params, DM_BUFFER_SIZE, myMinBufferSize ) ); 
    }
    
    //
    // Allocate a pool.
    //

    DC( dmBufferCreatePool( params, &myPool ) );
    
    //
    // Tell all users which pool to use.
    //
    
    {
	for ( int i = 0;  i < myUserCount;  i++ )
	{
	    myUsers[i]->setPool( this );
	}
    }
    
    //
    // We don't need to know who the users are any more.
    //
    
    {
	for ( int i = 0;  i < myUserCount;  i++ )
	{
	    myUsers[i] = NULL;
	}
    }
    
}


////////
//
// create
//
////////

Ref<Pool> Pool::create(
    int bufferCount,
    DMboolean cacheable
    )
{
    return new PoolImpl( bufferCount, cacheable );
}
