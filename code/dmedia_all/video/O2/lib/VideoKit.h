////////////////////////////////////////////////////////////////////////
//
// VideoKit.h
//	
//	A simple C++ kit for creating demos that use video+graphics.
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

#ifndef _VIDEO_KIT_H
#define _VIDEO_KIT_H

#include "RefCount.h"
#include <dmedia/dm_buffer.h>
#include <dmedia/dm_image.h>
#include <dmedia/vl.h>
#include <GL/glx.h>

#define DM_CAPTURE_TYPE		"DM_CAPTURE_TYPE"

#define DM_CAPTURE_TYPE_FIELDS	VL_CAPTURE_NONINTERLEAVED
#define DM_CAPTURE_TYPE_FRAMES	VL_CAPTURE_INTERLEAVED

////////////////////////////////////////////////////////////////////////
//
// Global Information
//
////////////////////////////////////////////////////////////////////////

class Global
{
public:
    static void init();
    static void cleanup();
    
    static VLServer	videoServer;
    static Display*	xDisplay;
    
    static GLXFBConfigSGIX chooseFBConfig(
	class Params* imageFormat,
	int depthSize,
	int stencilSize,
	DMboolean doubleBuffered
	);
    
    static void printFBConfig( GLXFBConfigSGIX );

    static void installXErrorHandler();
    static void removeXErrorHandler( const char* message );
};

////////////////////////////////////////////////////////////////////////
//
// General Digital Media Things
//
////////////////////////////////////////////////////////////////////////

////////
//
// ErrorHandler
//
// This class is used internally in the library to handle errors.  It
// just prints a message and exits when an error occurs.
//
////////

class ErrorHandler
{
public:
    static void error( const char* message );
    static void dmError();
    static void vlError();
    
    //
    // Set a breakpoint at exit() to catch programs before they exit.
    //
    
    static void exit();
};

#define DC(call)							\
    {									\
	if ( call != DM_SUCCESS )					\
	{								\
	    ErrorHandler::dmError();					\
	}								\
    }

#define VC(call)							\
    {									\
	if ( call != 0 )						\
	{								\
	    ErrorHandler::vlError();					\
	}								\
    }

////////
//
// Params
//
// This is a wrapper for DMparams to simplify the creating of
// parameters for:
//   - image formats
//   - pool requirements
//
////////

class Params : public RefCount
{
public:
    static Ref<Params> create();
    static Ref<Params> copy( Params* );

    static Ref<Params> imageFormat(
	int width, int height,
	DMimagelayout,
	DMimageorientation,
	DMimagepacking );
    
    static Ref<Params> poolSpec(
	int bufferCount,
	DMboolean cacheable );

    virtual DMparams* dmparams() = 0;
    
    //
    // These methods convert the values in an image format to the
    // enums used by other libraries.
    //

    virtual int width()		= 0;
    virtual int height() 	= 0;
    virtual int vlFormat() 	= 0;
    virtual int vlPacking() 	= 0;
    virtual int vlLayout()  	= 0;
    
    virtual void print() = 0;
};

////////
//
// Buffer
//
// A wrapper for DMbuffers.
//
////////

class Buffer : public RefCount
{
public:
    static Ref<Buffer> wrap( 
	DMbuffer bufferToWrap, 
	Params* imageFormat
	);
    
    virtual void*     data() = 0;
    virtual Params*   format() = 0;
    virtual DMbuffer  dmbuffer() = 0;
    
    virtual USTMSCpair timestamp() = 0;
    
    virtual void	setFormat( Params* ) = 0;
};

////////
//
// PoolUser
//
// An base class to be inherited by objects that use DMbufferpools.
//
////////

class PoolUser : public RefCount
{
public:
    virtual void getPoolRequirements( Params* ) = 0;
    virtual void setPool( class Pool* ) = 0;
};

////////
//
// Pool
//
// A wrapper for DMbufferpools.  It simplifies the creation of pools.
// Normally, when you create a pool you have to query all of the users
// for their pool requirements, collecting a DMparams specification.
// Then, after the pool is created, you have to go back and tell each
// of the users which pool to use.  The allocatePool() method in this
// class handles both of these tasks.
//
////////

class Pool : public RefCount
{
public:
    //
    // The create() method doesn't actually create the pool.  It just
    // makes the object.  The allocatePool() method, below, creates
    // the DMbufferpool.
    //
    
    static Ref<Pool> create( 
	int bufferCount,	// number of buffers needed by the
				// application
	DMboolean cacheable	// should be DM_TRUE if the
				// application will read or write the
				// contents of buffers
	);
    
    //
    // setMinBufferSize()
    //
    
    virtual void setMinBufferSize( int size ) = 0;

    //
    // addUser() should be called once for each client of the pool
    // before allocatePool() is called.
    //

    virtual void addUser( PoolUser* ) = 0;
    
    //
    // allocatePool() -- create a pool that matches the requirments of
    // all of the users.  The number of buffers in the pool is the sum
    // of the requests from the users, plus the number specified to
    // Pool::create(). 
    //

    virtual void allocatePool() = 0;
    
    //
    // allocateBuffer() -- returns an unused buffer from the pool.
    //

    virtual Ref<Buffer> allocateBuffer( Params* format ) = 0;
    
    //
    // dmpool() -- returns the pool inside.
    //

    virtual DMbufferpool dmpool() = 0;
};

////////////////////////////////////////////////////////////////////////
//
// Video Things
//
////////////////////////////////////////////////////////////////////////

////////
//
// VideoIn
//
////////

class VideoIn : public PoolUser
{
public:
    static Ref<VideoIn> create(
	int videoDevice,
	int videoNode,
	Params* imageFormat 
	);
    
    //
    // getDefaultFrameSize() -- will return the size of a frame, based on
    // the settings in vcp (NTSC/PAL).
    //

    static void getDefaultFrameSize( 
	int videoDevice,
	int videoNode,
	int* returnWidth,
	int* returnHeight
	);
    
    static void getDefaultFieldSize( 
	int videoDevice,
	int videoNode,
	int* returnWidth,
	int* returnHeight
	);
    
    //
    // Nothing happens until start() is called.
    //

    virtual void start() = 0;
    
    //
    // getFrame() -- returns the next frame.
    //

    virtual Ref<Buffer> getFrame() = 0;
};

////////
//
// VideoOut
//
////////

class VideoOut : public PoolUser
{
public:
    static Ref<VideoOut> create(
	int videoDevice,
	int videoNode,
	Params* imageFormat 
	);
    
    //
    // getDefaultSize() -- will return the size of a frame, based on
    // the settings in vcp (NTSC/PAL).
    //

    static void getDefaultSize( 
	int videoDevice,
	int videoNode,
	int* returnWidth,
	int* returnHeight
	);
    
    //
    // Nothing happens until start() is called.
    //

    virtual void start() = 0;
    
    //
    // getFrame() -- returns the next frame.
    //

    virtual void putFrame( Buffer* ) = 0;
    
    //
    // frontier() -- returns the timestamp for the next frame to be
    // sent. 
    //

    virtual USTMSCpair frontier() = 0;
};

////////////////////////////////////////////////////////////////////////
//
// Graphics Things
//
////////////////////////////////////////////////////////////////////////

////////
//
// Context
//
////////

class Context : public RefCount
{
public:
    static Ref<Context> create(
	GLXFBConfigSGIX fbconfig,
	Params*		imageFormat
	);
    
    //
    // The first time either make-current method is called, an
    // orthographic coordinate system is set up, with (0,0) at the
    // lower-left, and (width,height) at the upper right.
    //

    virtual void makeCurrent( GLXDrawable draw ) = 0;
    virtual void makeCurrentRead( GLXDrawable draw, GLXDrawable read ) = 0;
};

////////
//
// GLWindow
//
////////

class GLWindow : public RefCount
{
public:
    static Ref<GLWindow> create( 
	const char* 	name,
	GLXFBConfigSGIX	fbconfig,
	Params*		imageFormat
	);

    virtual GLXDrawable drawable() = 0;
    
    virtual void swapBuffers() = 0;
    
    virtual Window xwindow() = 0;
};

////////
//
// Pbuffer
//
////////

class Pbuffer : public RefCount
{
public:
    static Ref<Pbuffer> create( 
	GLXFBConfigSGIX	fbconfig,
	Params*		imageFormat
	);

    virtual GLXDrawable drawable() = 0;
};

////////
//
// DMPbuffer
//
////////

class DMPbuffer : public PoolUser
{
public:
    static Ref<DMPbuffer> create( 
	GLXFBConfigSGIX	fbconfig,
	Params*		imageFormat
	);

    virtual GLXDrawable drawable() = 0;
    
    virtual void associate( Buffer* ) = 0;
};

////////////////////////////////////////////////////////////////////////
//
// Compression and Decompression
//
////////////////////////////////////////////////////////////////////////

class Codec : public PoolUser
{
public:
    static Ref<Codec> create(
	Params*		srcFormat,
	Params*		dstFormat
	);
    
    virtual void putFrame( Buffer* ) = 0;
    virtual Ref<Buffer> getFrame() = 0;
};

    
#endif // _VIDEO_KIT_H
