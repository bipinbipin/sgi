/***********************************************************************
**
** composite.c
**
** This example program demonstrates the use of DMbuffers to
** do simple compositing.  It does "picture in picture", using two
** live video inputs.
**
** Two video streams are captured.  The first one is captured in
** "pbuffer" format so that it can be attached to a GLXDMbuffer and
** used as a drawable with GLX.  The second stream is captured (as a
** texture) and drawn as a "picture in picture" on top of an image
** from the first stream.
**
** The resulting composite image is sent to video out.
**
**
** Copyright 1996, Silicon Graphics, Inc.
** ALL RIGHTS RESERVED
**
** UNPUBLISHED -- Rights reserved under the copyright laws of the United
** States.   Use of a copyright notice is precautionary only and does not
** imply publication or disclosure.
**
** U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
** in Technical Data and Computer Software clause at DFARS 252.227-7013 and*or
** in similar or successor clauses in the FAR, or the DOD or NASA FAR
** Supplement.  Contractor*manufacturer is Silicon Graphics, Inc.,
** 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
**
** THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
** INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
** DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
** PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
** GRAPHICS, INC.
**
***********************************************************************/

#include <dmedia/dm_buffer.h>
#include <dmedia/dm_image.h>
#include <dmedia/vl.h>
#include <dmedia/vl_mvp.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>

#include <math.h>
#include <unistd.h>

#define CAMERA_Z	1000

#define MY_WIDTH	640
#define MY_HEIGHT	480

#define MY_TEX_WIDTH	512
#define MY_TEX_HEIGHT	256

#define MY_VL_PACKING	VL_PACKING_ARGB_1555
#define MY_VL_FORMAT	VL_FORMAT_RGB
#define MY_VL_RATE	30
#define MY_VL_TIMING	VL_TIMING_525_SQ_PIX

/**********************************************************************
*
* This is just a handy little routine for debugging.
*
**********************************************************************/

void PrintParams( const DMparams* p, int indent )
{
    int len = dmParamsGetNumElems( p );
    int i;
    int j;
    
    for ( i = 0;  i < len;  i++ ) {
	const char* name = dmParamsGetElem    ( p, i );
	DMparamtype type = dmParamsGetElemType( p, i );
	
	for ( j = 0;  j < indent;  j++ ) {
	    printf( " " );
	}

	printf( "%8s: ", name );
	switch( type ) 
	    {
	    case DM_TYPE_ENUM:
		printf( "%d", dmParamsGetEnum( p, name ) );
		break;
	    case DM_TYPE_INT:
		printf( "%d", dmParamsGetInt( p, name ) );
		break;
	    case DM_TYPE_STRING:
		printf( "%s", dmParamsGetString( p, name ) );
		break;
	    case DM_TYPE_FLOAT:
		printf( "%f", dmParamsGetFloat( p, name ) );
		break;
	    case DM_TYPE_FRACTION:
		{
		    DMfraction f;
		    f = dmParamsGetFract( p, name );
		    printf( "%d/%d", f.numerator, f.denominator );
		}
		break;
	    case DM_TYPE_PARAMS:
		PrintParams( dmParamsGetParams( p, name ), indent + 4 );
		break;
	    case DM_TYPE_ENUM_ARRAY:
		{
		    int i;
		    const DMenumarray* array = dmParamsGetEnumArray( p, name );
		    for ( i = 0;  i < array->elemCount;  i++ ) {
			printf( "%d ", array->elems[i] );
		    }
		}
		break;
	    case DM_TYPE_INT_ARRAY:
		{
		    int i;
		    const DMintarray* array = dmParamsGetIntArray( p, name );
		    for ( i = 0;  i < array->elemCount;  i++ ) {
			printf( "%d ", array->elems[i] );
		    }
		}
		break;
	    case DM_TYPE_STRING_ARRAY:
		{
		    int i;
		    const DMstringarray* array = 
			dmParamsGetStringArray( p, name );
		    for ( i = 0;  i < array->elemCount;  i++ ) {
			printf( "%s ", array->elems[i] );
		    }
		}
		break;
	    case DM_TYPE_FLOAT_ARRAY:
		{
		    int i;
		    const DMfloatarray* array = 
			dmParamsGetFloatArray( p, name );
		    for ( i = 0;  i < array->elemCount;  i++ ) {
			printf( "%f ", array->elems[i] );
		    }
		}
		break;
	    case DM_TYPE_FRACTION_ARRAY:
		{
		    int i;
		    const DMfractionarray* array = 
			dmParamsGetFractArray( p, name );
		    for ( i = 0;  i < array->elemCount;  i++ ) {
			printf( "%d/%d ", 
			        array->elems[i].numerator,
			        array->elems[i].denominator );
		    }
		}
		break;
	    case DM_TYPE_INT_RANGE:
		{
		    const DMintrange* range = dmParamsGetIntRange( p, name );
		    printf( "%d ... %d", range->low, range->high );
		}
		break;
	    case DM_TYPE_FLOAT_RANGE:
		{
		    const DMfloatrange* range = 
			dmParamsGetFloatRange( p, name );
		    printf( "%f ... %f", range->low, range->high );
		}
		break;
	    case DM_TYPE_FRACTION_RANGE:
		{
		    const DMfractionrange* range = 
			dmParamsGetFractRange( p, name );
		    printf( "%d/%d ... %d/%d", 
			    range->low.numerator, 
			    range->low.denominator, 
			    range->high.numerator,
			    range->high.denominator );
		}
		break;
	    defualt:
		printf( "UNKNOWN TYPE" );
	    }
	printf( "\n" );
    }
}

/**********************************************************************
*
* Global Variables
*
**********************************************************************/

/********
*
* theDisplay
*
********/

Display* theDisplay;

/********
*
* theVideoServer
*
* This is that handle that is used to access the video library.
*
********/

VLServer theVideoServer = NULL;

/**********************************************************************
*
* Utility Functions
*
**********************************************************************/

/********
*
* CHECK
*
* This is a simple error-checking macro that just exits the program if
* anything goes wrong.
*
********/

#define CHECK(condition,message)					\
    {									\
	if ( ! ( condition ) )						\
	{								\
	    fprintf( stderr, "Error at line %d of %s:\n",		\
		     __LINE__, __FILE__ );				\
	    fprintf( stderr, "    %s\n", message );			\
	    exit( 1 );							\
	}								\
    }

/********
*
* CHECK_VL
*
* This is similar to CHECK, but also prints a VL error message.
*
********/

#define CHECK_VL(condition,message)					\
    {									\
	if ( ! ( condition ) )						\
	{								\
	    fprintf( stderr, "Error at line %d of %s:\n",		\
		     __LINE__, __FILE__ );				\
	    fprintf( stderr, "    %s\n", message );			\
	    fprintf( stderr, "    VL error message: %s\n",		\
		     vlStrError( vlGetErrno() ) );			\
	    exit( 1 );							\
	}								\
    }

/********
*
* CHECK_DM
*
* This is similar to CHECK, but also prints a DM error message.
*
********/

#define CHECK_DM(condition,message)					\
    {									\
	if ( ! ( condition ) )						\
	{								\
	    const char* msg;						\
	    int errornum;						\
	    char detail[DM_MAX_ERROR_DETAIL];				\
	    fprintf( stderr, "Error at line %d of %s:\n",		\
		     __LINE__, __FILE__ );				\
	    fprintf( stderr, "    %s\n", message );			\
	    msg = dmGetError( &errornum, detail );			\
	    fprintf( stderr, "    DM error message: %s\n", detail );	\
	    exit( 1 );							\
	}								\
    }

/********
*
* getControlName
*
********/

const char* getControlName(
    VLPath path,
    VLNode node,
    VLControlType type
    )
{
    static char name[VL_NAME_SIZE];
    VLControlInfo* info;
    
    info = vlGetControlInfo( theVideoServer, path, node, type );
    CHECK_VL( info != NULL, "vlGetControlInfo failed" );
    
    strncpy( name, info->name, VL_NAME_SIZE );
    
    vlFreeControlInfo( theVideoServer, info );
    
    return name;
}

/********
*
* setIntControl
*
********/

void setIntControl(
    VLPath path,
    VLNode node,
    VLControlType type,
    int newValue
    )
{
    VLControlValue val;
    int vlstatus;
    
    val.intVal = newValue;
    vlstatus = vlSetControl( theVideoServer, path, node, type, &val );

#ifdef DEBUG
    printf( "Setting value %s: %d\n",
	    getControlName( path, node, type ),
	    val.intVal );
#endif
    
    if ( vlstatus != 0 && vlGetErrno() == VLValueOutOfRange )
    {
	vlstatus = vlGetControl( theVideoServer, path, node, type, &val );
	CHECK_VL( vlstatus == 0, "Could not get control" );
	printf( "Actual value for %s: %d\n",
		getControlName( path, node, type ), val.intVal );
    }
    else 
    {
	CHECK_VL( vlstatus == 0, "Could not set control" );
    }
}

/********
*
* setFractControl
*
********/

void setFractControl(
    VLPath path,
    VLNode node,
    VLControlType type,
    int numerator,
    int denominator
    )
{
    VLControlValue val;
    int vlstatus;
    
    val.fractVal.numerator   = numerator;
    val.fractVal.denominator = denominator;

#ifdef DEBUG
    printf( "Setting value %s: %d/%d\n",
	    getControlName( path, node, type ),
	    val.fractVal.numerator,
	    val.fractVal.denominator );
#endif
    
    vlstatus = vlSetControl( theVideoServer, path, node, type, &val );

    if ( vlstatus != 0 && vlGetErrno() == VLValueOutOfRange )
    {
	vlstatus = vlGetControl( theVideoServer, path, node, type, &val );
	CHECK_VL( vlstatus == 0, "Could not get control" );
	printf( "Actual value for %s: %d/%d\n",
		getControlName( path, node, type ),
		val.fractVal.numerator,
		val.fractVal.denominator );
    }
    else 
    {
	CHECK_VL( vlstatus == 0, "Could not set control" );
    }
}

/********
*
* setXYControl
*
********/

void setXYControl(
    VLPath path,
    VLNode node,
    VLControlType type,
    int x,
    int y
    )
{
    VLControlValue val;
    int vlstatus;
    
    val.xyVal.x = x;
    val.xyVal.y = y;

#ifdef DEBUG
    printf( "Setting value %s: %d %d\n",
	    getControlName( path, node, type ),
	    val.xyVal.x,
	    val.xyVal.y );
#endif
    
    vlstatus = vlSetControl( theVideoServer, path, node, type, &val );

    if ( vlstatus != 0 && vlGetErrno() == VLValueOutOfRange )
    {
	vlstatus = vlGetControl( theVideoServer, path, node, type, &val );
	CHECK_VL( vlstatus == 0, "Could not get control" );
	printf( "Actual value for %s: %d %d\n",
		getControlName( path, node, type ),
		val.xyVal.x,
		val.xyVal.y );
    }
    else 
    {
	CHECK_VL( vlstatus == 0, "Could not set control" );
	/* Make sure the VL was not lying: */
	vlstatus = vlGetControl( theVideoServer, path, node, type, &val );
	if ( val.xyVal.x != x || val.xyVal.y != y )
	{
	    printf( "XXX Actual value for %s: %d %d\n",
		    getControlName( path, node, type ),
		    val.xyVal.x,
		    val.xyVal.y );
	}
    }
}

/********
*
* getXYControl
*
********/

void getXYControl(
    VLPath path,
    VLNode node,
    VLControlType type,
    int* returnX,
    int* returnY
    )
{
    VLControlValue val;
    int vlstatus;
    
    vlstatus = vlGetControl( theVideoServer, path, node, type, &val );
    CHECK_VL( vlstatus == 0, "Could not get control" );
    
    *returnX = val.xyVal.x;
    *returnY = val.xyVal.y;
}

/********
*
* theErrorFlag
*
********/

static DMboolean theErrorFlag = DM_FALSE;

/********
*
* theOldHandler
*
********/

static int (*theOldHandler) ( Display*, XErrorEvent* ) = NULL;

/********
*
* errorHandler
*
********/

static int errorHandler(
    Display* display,
    XErrorEvent* event
    )
{
    theErrorFlag = DM_TRUE;
    (*theOldHandler)( display, event );
    return 0; /* return value is ignored */
}
    
/********
*
* installXErrorHandler
*
********/

void installXErrorHandler(
    void
    )
{
    /*
     * This is probably overly paranoid.  It will cause any errors
     * that are in the pipe already to use the old handler.
     */

    XSync( theDisplay, DM_FALSE );
    
    /*
     * Clear the flag.  It will get set if an error occurs.
     */
    
    theErrorFlag = DM_FALSE;

    /*
     * Install our error handler and save the old one.
     */

    theOldHandler = XSetErrorHandler( errorHandler );
}

/********
*
* removeXErrorHandler
*
********/

DMstatus removeXErrorHandler(
    void
    )
{
    /*
     * Force any errors out of the pipe.
     */

    XSync( theDisplay, DM_FALSE );
    
    /*
     * Replace the old error handler.
     */

    XSetErrorHandler( theOldHandler );
    
    /*
     * Did an error happen?
     */

    if ( theErrorFlag )
    {
	return DM_FAILURE;
    }
    else
    {
	return DM_SUCCESS;
    }
}

/**********************************************************************
*
* Video Input Paths
*
**********************************************************************/

/********
*
* setupVideoInput
*
********/

void setupVideoInput(
    int sourceNodeNumber,
    int layout,
    int zoomWidth,
    int zoomHeight,
    int width,
    int height,
    VLPath* returnPath,
    VLNode* returnNode
    )
{
    VLNode 	   source;
    VLNode 	   drain;
    VLPath 	   path;
    VLControlValue value;
    int vlstatus;
    
    /*
     * Create the two nodes.
     */

    source = vlGetNode( theVideoServer, VL_SRC, VL_VIDEO, sourceNodeNumber );
    CHECK_VL( source != -1, "Could not create video source node." )
    drain = vlGetNode( theVideoServer, VL_DRN, VL_MEM, VL_ANY );
    CHECK_VL( drain != -1, "Could not create video drain node." );
    
    /*
     * Create the path.
     */

    path = vlCreatePath( theVideoServer, VL_ANY, source, drain );
    CHECK_VL( path != -1, "Could not create video path." );
    vlstatus = vlSetupPaths( theVideoServer, &path, 1, VL_SHARE, VL_SHARE );
    CHECK_VL( vlstatus == 0, "Could not set up video path." );
    
    /*
     * Set the controls on the path for the correct format.  The
     * important one for this example is the layout parameter.
     * VL_LAYOUT_GRAPHICS generates images in the right format so that
     * the buffer can be used as a render area.
     * VL_LAYOUT_MIPMAP generated mipmapped images for use as textures.
     */

    setIntControl  ( path, source, VL_TIMING,   MY_VL_TIMING );

    /*
     * Set the ZOOMSIZE and SIZE.  These must be set before setting
     * MIPMAP, but after setting the timing.
     */

    setIntControl  ( path, drain,  VL_PACKING,       MY_VL_PACKING );
    setIntControl  ( path, drain,  VL_CAP_TYPE,      VL_CAPTURE_INTERLEAVED );
    setIntControl  ( path, drain,  VL_FORMAT,        MY_VL_FORMAT );
    setXYControl   ( path, drain,  VL_MVP_ZOOMSIZE,  zoomWidth, zoomHeight );
    setXYControl   ( path, drain,  VL_SIZE,          width, height );
    setFractControl( path, drain,  VL_RATE,          MY_VL_RATE, 1 );
    setIntControl  ( path, drain,  VL_LAYOUT,        layout );
    
    *returnPath = path;
    *returnNode = drain;
}

/********
*
* getVideoPoolConstraints
*
********/

void getVideoPoolConstraints(
    DMparams* poolParams,
    VLPath path,
    VLNode node
    )
{
    int vlstatus;
    int prevBuffCount;
    int videoBuffCount;
    
    /*
     * Because this program has multiple paths in operation at the
     * same time, the buffer counts for the paths need to be summed.
     * By default, though, vlDMPoolGetParams assumes that multiple
     * paths will not be used simultaneously, so it just increases the
     * buffer count if it is not already enough for the one path.
     */

    prevBuffCount = dmParamsGetInt( poolParams, DM_BUFFER_COUNT );
    dmParamsSetInt( poolParams, DM_BUFFER_COUNT, 0 );
    
    vlstatus = vlDMPoolGetParams( theVideoServer, path, node, poolParams );
    CHECK_VL( vlstatus == 0, "Could not get video pool parameters" );

    videoBuffCount = dmParamsGetInt( poolParams, DM_BUFFER_COUNT );
    dmParamsSetInt( poolParams, DM_BUFFER_COUNT, prevBuffCount + videoBuffCount );


}

/********
*
* startVideoInput
*
********/

void startVideoInput(
    VLPath path,
    VLNode drain,
    DMbufferpool pool
    )
{
    int vlstatus;
    
    /*
     * Tell the video library where to get the memory to store the
     * incoming frames.
     */

    vlstatus = vlDMPoolRegister( theVideoServer, path, drain, pool );
    CHECK_VL( vlstatus == 0, "Could not register pool with input path" );
    
    /*
     * Start the transfer.
     */

    vlstatus = vlBeginTransfer( theVideoServer, path, 0, NULL );
    CHECK_VL( vlstatus == 0, "Could not start video input transfer" );
}

/********
*
* getOneInputFrame
*
********/

DMbuffer getOneInputFrame(
    const char* pathName,
    VLPath path,
    stamp_t* msc
    )
{
    VLEvent event;
    DMbuffer buffer;
    int vlstatus;
    
    /*
     * Get the next event.
     */
    
    vlstatus = vlEventRecv( theVideoServer, path, &event );
    while ( vlstatus == -1 && vlGetErrno() == VLAgain )
    {
	sginap( 1 );
	vlstatus = vlEventRecv( theVideoServer, path, &event );
    }
    
    CHECK( vlstatus == 0, 
	   "Could not get video input event" );
    CHECK( event.reason == VLTransferComplete,
	   "Unexpected video event received" );
    
    /*
     * Get the buffer associated with the event.
     */

    vlstatus = vlEventToDMBuffer( &event, &buffer );
    CHECK( vlstatus == 0, "Video event does not have a buffer" );
    
    /*
     * Verify the MSC.
     */
    
    {
	USTMSCpair pair;
	pair = dmBufferGetUSTMSCpair( buffer );
	if ( *msc != 0 && pair.msc != *msc + 1 )
	{
	    printf( "skipped a frame: %s\n", pathName );
	}
	*msc = pair.msc;
    }
	    
    return buffer;
}

/********
*
* frameTooEarly
*
********/

DMboolean frameTooEarly(
    DMbuffer frame,
    DMbuffer otherFrame
    )
{
    USTMSCpair pair1 = dmBufferGetUSTMSCpair( frame );
    USTMSCpair pair2 = dmBufferGetUSTMSCpair( otherFrame );
    
    const stamp_t slop = 20000000;   /* 20ms */

    return pair1.ust + slop < pair2.ust;
}

/**********************************************************************
*
* Video Output Path
*
**********************************************************************/

/********
*
* setupVideoOutput
*
********/

void setupVideoOutput(
    VLPath* returnPath,
    VLNode* returnNode
    )
{
    VLNode 	   source;
    VLNode 	   drain;
    VLPath 	   path;
    VLControlValue value;
    int vlstatus;
    
    /*
     * Create the two nodes.
     */

    source = vlGetNode( theVideoServer, VL_SRC, VL_MEM, VL_ANY );
    CHECK_VL( source != -1, "Could not create video source node." )
    drain = vlGetNode( theVideoServer, VL_DRN, VL_VIDEO, VL_ANY );
    CHECK_VL( drain != -1, "Could not create video drain node." );
    
    /*
     * Create the path.
     */

    path = vlCreatePath( theVideoServer, VL_ANY, source, drain );
    CHECK_VL( path != -1, "Could not create video path." );
    vlstatus = vlSetupPaths( theVideoServer, &path, 1, VL_SHARE, VL_SHARE );
    CHECK_VL( vlstatus == 0, "Could not set up video path." );
    
    /*
     * Set the controls on the path for the correct format.  The
     * important one for this example is VL_LAYOUT_GRAPHICS, which
     * specifies that the frame sent for video output will be coming
     * from OpenGL.
     */

    setIntControl  ( path, source, VL_PACKING,  MY_VL_PACKING );
    setIntControl  ( path, source, VL_CAP_TYPE, VL_CAPTURE_INTERLEAVED );
    setFractControl( path, source, VL_RATE,     MY_VL_RATE, 1 );
    setIntControl  ( path, source, VL_FORMAT,   MY_VL_FORMAT );
    setIntControl  ( path, source, VL_LAYOUT,   VL_LAYOUT_GRAPHICS );
    setIntControl  ( path, drain,  VL_TIMING,   MY_VL_TIMING );
    setXYControl   ( path, source, VL_SIZE,     MY_WIDTH, MY_HEIGHT );
    
    *returnPath = path;
    *returnNode = source;
}

/********
*
* startVideoOutput
*
********/

void startVideoOutput(
    VLPath path
    )
{
    int vlstatus;
    
    /*
     * Start the transfer.
     */

    vlstatus = vlBeginTransfer( theVideoServer, path, 0, NULL );
    CHECK_VL( vlstatus == 0, "Could not start video input transfer" );
}

/********
*
* putOneOutputFrame
*
********/

void putOneOutputFrame(
    VLPath path,
    DMbuffer frame
    )
{
    int vlstatus;
    
    vlstatus = vlDMBufferSend( theVideoServer, path, frame );
    CHECK( vlstatus == 0, "Could not send video frame" );
}

/**********************************************************************
*
* Graphics
*
**********************************************************************/

#define ARRAY_COUNT(a)    (sizeof(a)/sizeof(a[0]))

static void printFBConfig(
    GLXFBConfigSGIX config
    )
{
    static struct { int attrib; const char* name; } attribs [] = {
	{ GLX_BUFFER_SIZE, "GLX_BUFFER_SIZE" },
	{ GLX_LEVEL, "GLX_LEVEL" },
	{ GLX_DOUBLEBUFFER, "GLX_DOUBLEBUFFER" },
	{ GLX_AUX_BUFFERS, "GLX_AUX_BUFFERS" },
	{ GLX_RED_SIZE, "GLX_RED_SIZE" },
	{ GLX_GREEN_SIZE, "GLX_GREEN_SIZE" },
	{ GLX_BLUE_SIZE, "GLX_BLUE_SIZE" },
	{ GLX_ALPHA_SIZE, "GLX_ALPHA_SIZE" },
	{ GLX_DEPTH_SIZE, "GLX_DEPTH_SIZE" },
	{ GLX_DRAWABLE_TYPE_SGIX, "GLX_DRAWABLE_TYPE_SGIX" },
	{ GLX_RENDER_TYPE_SGIX, "GLX_RENDER_TYPE_SGIX" },
	{ GLX_X_RENDERABLE_SGIX, "GLX_X_RENDERABLE_SGIX" },
	{ GLX_MAX_PBUFFER_WIDTH_SGIX, "GLX_MAX_PBUFFER_WIDTH_SGIX" },
	{ GLX_MAX_PBUFFER_HEIGHT_SGIX, "GLX_MAX_PBUFFER_HEIGHT_SGIX"},
    };
    int i;
    
    for ( i = 0;  i < ARRAY_COUNT( attribs );  i++ )
    {
	int value;
	int errorCode = glXGetFBConfigAttribSGIX(
	    theDisplay, config, attribs[i].attrib, &value );
	CHECK( errorCode == 0, "glXGetFBConfigAttribSGIX failed" );
	printf( "    %24s = %d\n", attribs[i].name, value );
    }
}

/********
*
* getFBConfig
*
********/

GLXFBConfigSGIX getFBConfig(
    void
    )
{
    int i;
    
    /*
     * Find a frame buffer configuration that suits our needs: it must
     * work with both the pbuffers and the window, and must have 5551 pixels,
     * because that is the only format that can be used to capture
     * mipmapped video.
     */

    GLXFBConfigSGIX config = NULL;
    int params[] = { 
	GLX_RENDER_TYPE_SGIX, 	GLX_RGBA_BIT_SGIX,
	GLX_DRAWABLE_TYPE_SGIX,	GLX_PBUFFER_BIT_SGIX,
	GLX_ALPHA_SIZE,		1,
	GLX_RED_SIZE, 		5,
	GLX_GREEN_SIZE, 	5,
	GLX_BLUE_SIZE, 		5,
	GLX_DEPTH_SIZE,		24,
	GLX_X_RENDERABLE_SGIX,	False,
	(int)None,
    };
    int configCount;
    GLXFBConfigSGIX* configs = 
	glXChooseFBConfigSGIX(
	    theDisplay,
	    DefaultScreen(theDisplay),
	    params,
	    &configCount
	    );
    CHECK( configs != NULL || configCount != 0,
	   "Could not get FBConfig" );
	
    for ( i = 0;  i < configCount;  i++ ) 
    {
	int value;
	
	glXGetFBConfigAttribSGIX(theDisplay, configs[i], GLX_ALPHA_SIZE, &value);
	if ( value != 1 )  continue;
	glXGetFBConfigAttribSGIX(theDisplay, configs[i], GLX_RED_SIZE, &value);
	if ( value != 5 )  continue;
	glXGetFBConfigAttribSGIX(theDisplay, configs[i], GLX_GREEN_SIZE, &value);
	if ( value != 5 )  continue;
	glXGetFBConfigAttribSGIX(theDisplay, configs[i], GLX_BLUE_SIZE, &value);
	if ( value != 5 )  continue;
	
	break;
    }

    if ( i != configCount )
    {
	config = configs[i];
    }
    
    CHECK( config != NULL, "No 5551 FBconfigs were found" );

    XFree( configs );
    
    printFBConfig( config );

    return config;
}
    
/********
*
* setupDrawable
*
********/

void setupDrawable(
    int width,
    int height,
    GLXPbufferSGIX* returnDrawable
    )
{
    GLXPbufferSGIX drawable;
    int attrib[] = {
	GLX_PRESERVED_CONTENTS_SGIX,    GL_TRUE,
        GLX_DIGITAL_MEDIA_PBUFFER_SGIX, GL_TRUE,
	None};
    
    /*
     * Find a frame buffer configuration that suits our needs.
     */

    GLXFBConfigSGIX config = getFBConfig();

    /*
     * Create the GLXPbufferSGIX.
     */

    printf( "Drawable size: %d x %d\n", width, height );

    installXErrorHandler();
    drawable = glXCreateGLXPbufferSGIX(
	theDisplay, config, width, height, attrib);
    CHECK( removeXErrorHandler() == DM_SUCCESS && drawable != None,
	   "Could not create pbuffer" );
    
    /*
     * All done.
     */

    *returnDrawable = drawable;
}

/********
*
* getGraphicsPoolConstraints
*
********/

void getGraphicsPoolConstraints(
    DMparams* imageFormat,
    DMparams* poolParams
    )
{
    DMstatus status;
    
    status = dmBufferGetGLPoolParams( imageFormat, poolParams );
    CHECK_DM( status == DM_SUCCESS, "Could not get graphics pool params" );

    status = dmParamsSetInt( poolParams, DM_BUFFER_COUNT,
			     dmParamsGetInt( poolParams, DM_BUFFER_COUNT ) + 2 ); 
    CHECK_DM( status == DM_SUCCESS, "dmParamsSetInt failed" );

}

/********
*
* setupTexture
*
********/

void setupTexture(
    void
    )
{
    /*
     * Before the mipmapped texture can be copied from video, there
     * must already be a texture there to copy into.
     */

    int w = MY_TEX_WIDTH;
    int h = MY_TEX_HEIGHT;
    int level = 0;
    void* black = calloc( w * h, 4 );
    
    level = 0;
    while ( 1 )
    {
	glTexImage2D(
	    GL_TEXTURE_2D,
	    level,
	    GL_RGB5_A1_EXT,	/* number of components */
	    w, h,
	    0,			/* border */
	    GL_RGBA,
	    GL_UNSIGNED_BYTE,
	    black
	    );
	printf( "%d: %d x %d\n", level, w, h );
	
	if ( w == 1 && h == 1 )
	{
	    break;
	}
	
	if ( w > 1 )   w /= 2;
	if ( h > 1 )   h /= 2;
	level++;
    }
    
    free( black );
}

/********
*
* startGraphics
*
********/

void startGraphics(
    GLXPbufferSGIX drawable,
    DMparams*	   drawableFormat,
    GLXPbufferSGIX readable,
    DMparams*      readableFormat,
    DMbufferpool   pool1,
    DMbufferpool   pool2,
    GLXContext*    returnContext
    )
{
    GLXFBConfigSGIX config;
    GLXContext context;
    int ok;
    
    /*
     * Get the FB configuration to use.
     */

    config = getFBConfig();
    
    /*
     * Create the graphics context.
     */

    installXErrorHandler();
    context = glXCreateContextWithConfigSGIX(
	theDisplay,
	config,
	GLX_RGBA_TYPE_SGIX,
	NULL,		
	GL_TRUE		
	);
    CHECK( removeXErrorHandler() == DM_SUCCESS && context != NULL,
	   "Could not create graphics context" );
    
    /*
     * The two pbuffers must have associated DMbuffer before they can
     * be made current.  (The will be associated with real video
     * frames before any rendering happens.)
     */
    
    {
	DMbuffer buffer;
	CHECK( dmBufferAllocate( pool1, &buffer ) == DM_SUCCESS,
	       "could not create dummy buffer" );
  
	installXErrorHandler();
	ok = glXAssociateDMPbufferSGIX( theDisplay, drawable,
					drawableFormat, buffer);
	CHECK( removeXErrorHandler() == DM_SUCCESS && ok,
	       "Could not associate DMbuffer with pbuffer" );
    }
    
    {
	DMbuffer buffer;
	CHECK( dmBufferAllocate( pool2, &buffer ) == DM_SUCCESS,
	       "could not create dummy buffer" );
  
	installXErrorHandler();
	ok = glXAssociateDMPbufferSGIX( theDisplay, readable,
					readableFormat, buffer);
	CHECK( removeXErrorHandler() == DM_SUCCESS && ok,
	       "Could not associate DMbuffer with pbuffer" );
    }
    
    /*
     * Because of a bug in OpenGL, dmpbuffers must be made current as
     * a drawable before being used as a readable.
     */

    installXErrorHandler();
    ok = glXMakeCurrent( theDisplay, readable, context );
    CHECK( removeXErrorHandler() == DM_SUCCESS && ok,
	   "glXMakeCurrent failed" );
    
    
    /*
     * Make it the current context.
     * DM Pbuffers must have associated DMbuffers before
     * they can be made current.
     */

    installXErrorHandler();
    ok = glXMakeCurrentReadSGI( theDisplay, drawable, readable, context );
    CHECK( removeXErrorHandler() == DM_SUCCESS && ok,
	   "glXMakeCurrent failed" );
    
    /*
     * Set up the OpenGL transforms.
     */

    glViewport( 0, 0, MY_WIDTH, MY_HEIGHT );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glFrustum(
	-MY_WIDTH / 2.0, MY_WIDTH / 2.0,
	-MY_HEIGHT / 2.0, MY_HEIGHT / 2.0,
	CAMERA_Z - MY_WIDTH / 2.0,  CAMERA_Z + MY_WIDTH / 2.0 
	);
    glMatrixMode( GL_MODELVIEW );
    glTranslatef( 0.0, 0.0, -CAMERA_Z );

    glEnable( GL_TEXTURE_2D );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    
    glEnable( GL_DEPTH_TEST );
    
    /*
     * Load a black texture.  Video images will be loaded as
     * subtextures. 
     * XXX This is only until the mipmap/dmbuffer/OpenGL stuff works.
     */
    
    setupTexture();

    /*
     * Turn on the mipmap extension so that OpenGL will use the
     * mipmaps created by video.
     */

    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
    
    *returnContext = context;
}

/********
*
* drawFace
*
********/

void drawFace(
    Bool borders,
    float z
    )
{
    if ( borders )
    {
	glDisable( GL_TEXTURE_2D );
	glBegin( GL_QUADS );
	{
	    glTexCoord2f( 0.0, 0.0 ); glVertex3i( -160, -160, z );
	    glTexCoord2f( 0.0, 1.0 ); glVertex3i( -160, -120, z );
	    glTexCoord2f( 1.0, 1.0 ); glVertex3i(  160, -120, z );
	    glTexCoord2f( 1.0, 0.0 ); glVertex3i(  160, -160, z );

	    glTexCoord2f( 0.0, 0.0 ); glVertex3i( -160,  120, z );
	    glTexCoord2f( 0.0, 1.0 ); glVertex3i( -160,  160, z );
	    glTexCoord2f( 1.0, 1.0 ); glVertex3i(  160,  160, z );
	    glTexCoord2f( 1.0, 0.0 ); glVertex3i(  160,  120, z );
	}
	glEnd();
    }
    
    glEnable( GL_TEXTURE_2D );
    glBegin( GL_QUADS );
    {
	glTexCoord2f( 0.0, 0.0 ); glVertex3i( -160, -120, z );
	glTexCoord2f( 0.0, 1.0 ); glVertex3i( -160,  120, z );
	glTexCoord2f( 1.0, 1.0 ); glVertex3i(  160,  120, z );
	glTexCoord2f( 1.0, 0.0 ); glVertex3i(  160, -120, z );
    }
    glEnd();
    
}

/********
*
* drawCube
*
********/

void drawCube(
    void
    )
{
    glColor4f( 0.5, 0.5, 0.5, 1.0 );
    
    /* front */
    drawFace( False, 160 );
    
    /* right */
    glPushMatrix();
    glRotatef( 90.0, 0.0, 1.0, 0.0 );
    drawFace( False, 160 );
    glPopMatrix();
    
    /* left */
    glPushMatrix();
    glRotatef( -90.0, 0.0, 1.0, 0.0 );
    drawFace( False, 160 );
    glPopMatrix();
    
    /* back */
    glPushMatrix();
    glRotatef( 180.0, 0.0, 1.0, 0.0 );
    drawFace( False, 160 );
    glPopMatrix();
    
    /* top */
    glPushMatrix();
    glRotatef( -90.0, 1.0, 0.0, 0.0 );
    drawFace( True, 120 );
    glPopMatrix();
    
    /* bottom */
    glPushMatrix();
    glRotatef( 90.0, 1.0, 0.0, 0.0 );
    drawFace( True, 120 );
    glPopMatrix();
}

/********
*
* renderOneFrame
*
********/

void renderOneFrame(
    stamp_t  msc,
    DMbuffer frame1,
    DMbuffer frame2
    )
{
    static int counter = 0;
   
    /*
     * Load the texture.  This call to "copy" the texture actually
     * just changes the texture object to point at this dmbuffer
     * because:
     *    1) the entire texture is copied.
     *    2) the format of the dmbuffer is the same as the texture.
     */

    glCopyTexSubImage2DEXT(
	GL_TEXTURE_2D, 
	0, 			/* level */
	0, 0,			/* x, y offset into target */
	0, 0, 			/* lower-left corner of copied rect */
	MY_TEX_WIDTH, MY_TEX_HEIGHT   /* size of copied rect */
	);
    
    /*
     * The depth buffer must be cleared.  It is not part of the
     * DMbuffer, so is retained from frame to frame.
     */

    glClear( GL_DEPTH_BUFFER_BIT );
    
    /*
     * Animate the cube.
     */
    
    {
	float count = ( (float) (counter++) );
	float angleX = count / 2;
	float angleY = count / M_PI;
	float scale  = ( cosf( count / 300.0 ) + 2.0 ) / 2.5;

	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glRotatef( angleX, 1.0, 0.0, 0.0 );
	glRotatef( angleY, 0.0, 1.0, 0.0 );
	glScalef( scale, scale, scale );
    }
    
    /*
     * Draw a cube using the texture.
     */

    drawCube();
    
    /*
     * Restore the transform
     */

    glPopMatrix();
}

/**********************************************************************
*
* Top-Level Control
*
**********************************************************************/

/********
*
* processOneFrame
*
********/

void processOneFrame(
    DMparams*		path1Format,
    VLPath		path1,
    VLNode		drain1,
    stamp_t*		path1MSC,
    DMparams*		path2Format,
    VLPath		path2,
    VLNode		drain2,
    stamp_t*		path2MSC,
    GLXPbufferSGIX 	drawable,
    GLXPbufferSGIX 	readable,
    VLPath		path3,
    VLNode		source3,
    int			bufferSize1,
    int			bufferSize2
    )
{
    DMbuffer frame1;
    DMbuffer frame2;
    Bool ok;
    
    /*
     * Get a frame from each of the input paths.
     */

    frame1 = getOneInputFrame( "path 1", path1, path1MSC );
    frame2 = getOneInputFrame( "path 2", path2, path2MSC );
    
    /*
     * Make sure that they are in sync.
     */

    while ( frameTooEarly( frame1, frame2 ) )
    {
	printf( "path1 frame was too early.\n" );
	dmBufferFree( frame1 );
	frame1 = getOneInputFrame( "path 1", path1, path1MSC );
    }
    while ( frameTooEarly( frame2, frame1 ) )
    {
	printf( "path2 frame was too early.\n" );
	dmBufferFree( frame2 );
	frame2 = getOneInputFrame( "path 2", path2, path2MSC );
    }
    
#ifdef DEBUG
    printf( "path1 MSC = %lld    path2 MSC = %lld\n", *path1MSC, *path2MSC );
#endif
    
    /*
     * Bind them to the drawable and readable so that we can get at
     * them from OpenGL.
     */

    ok = glXAssociateDMPbufferSGIX( theDisplay, drawable, path1Format, frame1);
    CHECK( ok, "Could not associate path1 DMbuffer with pbuffer" );
    
    ok = glXAssociateDMPbufferSGIX( theDisplay, readable, path2Format, frame2);
    CHECK( ok, "Could not associate path2 DMbuffer with pbuffer" );

    /*
     * Do the graphics.
     */

    renderOneFrame( *path1MSC, frame1, frame2 );
    
    /*
     * Make sure the graphics are complete before we pass the output
     * frame to video.
     */

    glFinish();

    /*
     * Send the processed frame out.
     */

    putOneOutputFrame( path3, frame1 );
    
    /*
     * We're done with both of the buffers now.  The video library
     * still has a handle to frame1 (from the output path), which will
     * be released when the frame goes out.  glX still has handles to
     * both (from the drawable and readable), which will be released
     * the next time this function is called.
     */

    dmBufferFree( frame1 );
    dmBufferFree( frame2 );
}

/********
*
* main
*
********/

int main(
    )
{
    DMstatus status;
    DMbufferpool pool1;		/* For paths 1 and 3 */
    DMbufferpool pool2;		/* For path 2 */
    
    int bufferSize1;
    int bufferSize2;
    
    VLPath path1;		/* The first input path (passed through) */
    VLNode drain1;
    
    VLPath path2;		/* The other input path (used as texture) */
    VLNode drain2;
    
    GLXPbufferSGIX drawable;	/* This is where the rendering happens */
    GLXPbufferSGIX readable;	/* This is the source for loading textures */
    GLXContext     context;
    
    VLPath path3;		/* The output path */
    VLNode source3;
    
    DMparams* path1Format;	/* this is also the format for path3 */
    DMparams* path2Format;
    
    int vlstatus;
    
    /*
     * Set up the parameter lists that describes the image formats.
     */
    
    status = dmParamsCreate( &path1Format );
    CHECK_DM( status == DM_SUCCESS, "Could not create dmparams" );
    status = dmSetImageDefaults( path1Format, MY_WIDTH, MY_HEIGHT,
				 DM_IMAGE_PACKING_XRGB1555 );
    CHECK_DM( status == DM_SUCCESS, "Could not set image param" );
    status = dmParamsSetEnum( path1Format, DM_IMAGE_LAYOUT,
			      DM_IMAGE_LAYOUT_GRAPHICS );
    CHECK_DM( status == DM_SUCCESS, "Could not set image param" );

    status = dmParamsCreate( &path2Format );
    CHECK_DM( status == DM_SUCCESS, "Could not create dmparams" );
    status = dmSetImageDefaults( path2Format, MY_TEX_WIDTH, MY_TEX_HEIGHT,
				 DM_IMAGE_PACKING_XRGB1555 );
    CHECK_DM( status == DM_SUCCESS, "Could not set image param" );
    status = dmParamsSetEnum( path2Format, DM_IMAGE_LAYOUT,
			      DM_IMAGE_LAYOUT_MIPMAP );
    CHECK_DM( status == DM_SUCCESS, "Could not set image param" );

    printf( "\n\nPath 1 format:\n" ); PrintParams( path1Format, 4 );
    printf( "\n\nPath 2 format:\n" ); PrintParams( path2Format, 4 );
    

    /* XXX What should the orientation be? */
    
    /*
     * Open the connection to the X server.
     */

    theDisplay = XOpenDisplay( ":0" );
    CHECK( theDisplay != NULL, "Could not open connection to local X server" );

    /*
     * Open the video library.
     */

    theVideoServer = vlOpenVideo( "" );
    CHECK_VL( theVideoServer != NULL,
	      "Could not open connection to video server" );

    /*
     * Set up the input and output video paths.
     */

    setupVideoInput( 
	VL_MVP_VIDEO_SOURCE_CAMERA,
	VL_LAYOUT_GRAPHICS,
	MY_WIDTH, MY_HEIGHT,   		/* ZOOMSIZE */
	MY_WIDTH, MY_HEIGHT,		/* SIZE */
	&path1, &drain1
	);

    setupVideoInput(
	VL_MVP_VIDEO_SOURCE_COMPOSITE,
	VL_LAYOUT_MIPMAP,		
	MY_TEX_WIDTH, MY_TEX_HEIGHT,	/* ZOOMSIZE */
	MY_TEX_WIDTH, MY_TEX_HEIGHT,	/* SIZE */
	&path2, &drain2 );
    
    setupVideoOutput( &path3, &source3 );
    
    /*
     * Set up the drawable and readable.
     */

    setupDrawable( MY_WIDTH,     MY_HEIGHT,     &drawable );
    setupDrawable( MY_TEX_WIDTH, MY_TEX_HEIGHT, &readable );
    
    /*
     * Create the memory pools that will be used to hold the images.
     */
    
    {
	DMparams* poolParams;
	status = dmParamsCreate( &poolParams );
	CHECK_DM( status == DM_SUCCESS, "Could not create pool params." );

	status = dmBufferSetPoolDefaults( poolParams, 0, 0,
					  DM_FALSE, DM_FALSE );
	CHECK_DM( status == DM_SUCCESS, "Could not set pool defaults." );
	
	printf( "\n\nDefault pool params:\n" ); PrintParams( poolParams, 4 );

	getVideoPoolConstraints( poolParams, path1, drain1  );
	printf( "\n\nAfter path1:\n" ); PrintParams( poolParams, 4 );
	
	getVideoPoolConstraints( poolParams, path3, source3 );
	printf( "\n\nAfter path3:\n" ); PrintParams( poolParams, 4 );
	
	getGraphicsPoolConstraints( path1Format, poolParams );
	printf( "\n\nAfter path1 graphics:\n" ); PrintParams( poolParams, 4 );

	printf( "\n\nFinal:\n" ); PrintParams( poolParams, 4 );
	
	status = dmBufferCreatePool( poolParams, &pool1 );
	CHECK_DM( status == DM_SUCCESS, "Could not create buffer pool" );
	
	bufferSize1 = dmParamsGetInt( poolParams, DM_BUFFER_SIZE );
	
	dmParamsDestroy( poolParams );
    }

    {
	DMparams* poolParams;
	status = dmParamsCreate( &poolParams );
	CHECK_DM( status == DM_SUCCESS, "Could not create pool params." );

	status = dmBufferSetPoolDefaults( poolParams, 0, 0,
					  DM_FALSE, DM_FALSE );
	CHECK_DM( status == DM_SUCCESS, "Could not set pool defaults." );
	
	printf( "\n\nDefault pool params:\n" ); PrintParams( poolParams, 4 );

	getVideoPoolConstraints( poolParams, path2, drain2  );
	printf( "\n\nAfter path2:\n" ); PrintParams( poolParams, 4 );
	
	getGraphicsPoolConstraints( path2Format, poolParams );
	printf( "\n\nAfter path2 graphics:\n" ); PrintParams( poolParams, 4 );
	
	printf( "\n\nFinal:\n" ); PrintParams( poolParams, 4 );
	
	status = dmBufferCreatePool( poolParams, &pool2 );
	CHECK_DM( status == DM_SUCCESS, "Could not create buffer pool" );
	
	bufferSize2 = dmParamsGetInt( poolParams, DM_BUFFER_SIZE );
	
	dmParamsDestroy( poolParams );
    }

    /*
     * Do one-time initialization of graphics.
     */

    startGraphics(
	drawable, path1Format,
	readable, path2Format,
	pool1, pool2,
	&context
	);
    
    /*
     * Start all three video paths.
     */

    printf( "Starting path 1...\n" );
    startVideoInput( path1, drain1, pool1 );
    
    printf( "Starting path 2...\n" );
    startVideoInput( path2, drain2, pool2 );
    
    printf( "Starting path 3...\n" );
    startVideoOutput( path3 );
    
    /*
     * This is where the work gets done.
     */
    
    {
	int frameNum;
	stamp_t path1MSC = 0;
	stamp_t path2MSC = 0;
	for ( frameNum = 0;  frameNum < 10000;  frameNum++ )
	{
	    processOneFrame( path1Format, path1, drain1, &path1MSC,
			     path2Format, path2, drain2, &path2MSC,
			     drawable, readable,
			     path3, source3,
			     bufferSize1, bufferSize2 );
	}
    }
    
    /*
     * Clean up.
     */

    glXMakeCurrentReadSGI(theDisplay, None, None, NULL); 
    glXDestroyContext( theDisplay, context );
    glXDestroyGLXPbufferSGIX( theDisplay, drawable );
    glXDestroyGLXPbufferSGIX( theDisplay, readable );
    vlstatus = vlDestroyPath( theVideoServer, path1 );
    CHECK_VL( vlstatus == 0, "vlDestroyPath failed" );
    vlstatus = vlDestroyPath( theVideoServer, path2 );
    CHECK_VL( vlstatus == 0, "vlDestroyPath failed" );
    vlstatus = vlDestroyPath( theVideoServer, path3 );
    CHECK_VL( vlstatus == 0, "vlDestroyPath failed" );
    vlCloseVideo( theVideoServer );
    
    return 0;
}
