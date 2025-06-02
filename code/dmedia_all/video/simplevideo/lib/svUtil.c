/* $Id: svUtil.c,v 1.8 1997/06/10 00:03:02 grant Exp $ */

#include <string.h>
#include <bstring.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <vl/vl.h>
#include <vlUtil/vlUtil.h>
#include <sv/sv.h>
#include "svPriv.h"

#define numof(arr) ((int) (sizeof(arr) / sizeof(arr[0])))


/*
 * forward static function definitions;
 */

static void
_shutdownVL(void);


/*
 * undocumented entry points;
 */

VLInfoPtr dmRBLockWritable(VLBuffer, int space, int count,
			      int *realCount, int block);

VLInfoPtr dmRBLockReadable(VLBuffer, int frameCount,
			   int *actualCount, int block);

/*
 * Possible Galileo video connections;
 */

#define svINPUT_MEMORY	0
#define svOUTPUT_MEMORY	0

typedef struct {
    int id;
    char *device;
    char *name;
    int type;
    int kind;
    int number;
    int muxValue;
} SvConnection;

/* XXX - we're gonna have to whack this a bit because the
 *       control value associated with a particular muxswitch
 *       label is not constant! i.e, "Composite 2" is "4" for
 *       Galileo and "5" for Indy Video.
 */

static SvConnection inputConnections[] = {
    { svINPUT_MEMORY,      "",     "memory",       VL_SRC, VL_MEM,   0, -1 },
    { vnINPUT_INDYCAM,     "vino", "IndyCam",      VL_SRC, VL_VIDEO, 0,  0 }, 
    { vnINPUT_COMPOSITE,   "vino", "Composite",    VL_SRC, VL_VIDEO, 1,  0 },
    { vnINPUT_SVIDEO,      "vino", "SVideo",       VL_SRC, VL_VIDEO, 1,  1 },
    { gvINPUT_COMPOSITE_1, "ev1",  "Composite 1",  VL_SRC, VL_VIDEO, 0,  3 },
    { gvINPUT_SVIDEO,      "ev1",  "Y/C (svideo)", VL_SRC, VL_VIDEO, 0,  1 },
    { gvINPUT_COMPOSITE_2, "ev1",  "Composite 2",  VL_SRC, VL_VIDEO, 0,  5 },
    { gvINPUT_DIGITAL_1,   "ev1",  "Digital 1",    VL_SRC, VL_VIDEO, 1, -1 },
    { gvINPUT_DIGITAL_2,   "ev1",  "Digital 2",    VL_SRC, VL_VIDEO, 2, -1 },
};

static int numInputConnections = numof(inputConnections);

static SvConnection outputConnections[] = {
    { svOUTPUT_MEMORY,  "",    "memory",      VL_DRN, VL_MEM,   0, -1 },
    { gvOUTPUT_ANALOG,  "ev1", "Analog  Out", VL_DRN, VL_VIDEO, 0, -1 },
    { gvOUTPUT_DIGITAL, "ev1", "Digital Out", VL_DRN, VL_VIDEO, 0, -1 },
};

static int numOutputConnections = numof(outputConnections);

/**********************************************************************
 * Simple Video context management -
 **********************************************************************
 */

typedef struct {
    int pid;
    ConnectionState *sv;
} SvContext;

typedef struct {
    int pid;
    int contextNum;
} SvPidTable;

#define TABLE_SIZE 256
static SvContext _svContextTable[TABLE_SIZE];
static SvPidTable _svPidEntry[TABLE_SIZE];


static int
_allocContextEntry(void)
{
    int pid;
    int i;
    ConnectionState *sv;

    pid = (int) getpid();

    for (i=0; i<TABLE_SIZE; i++)
	if (_svContextTable[i].pid == 0)
	    break;

    /* xxx table overflow ? */

    sv = (ConnectionState *) malloc(sizeof(ConnectionState));
    _svContextTable[i].pid = pid;
    _svContextTable[i].sv = sv;

    sv->vlInitialized = FALSE;
    sv->svr = NULL;
    sv->dev = NULL;
    sv->input = -1;
    sv->output = -1;
    sv->pathUsageMode = VL_SHARE;
    sv->controlUsageMode = VL_SHARE;
    sv->frameCount = 1;
    sv->packing = VL_PACKING_RGB_8;
    sv->saveImagesCompressed = FALSE;
    sv->transferMode = svTRANSFER_DISCRETE;
    sv->transferCountRemaining = 0;
    sv->freeFrameNextXfer = FALSE;
    sv->recoverFromPreemption = FALSE;
    sv->deviceWidthFactor = 1;
    sv->deviceHeightFactor = 1;
    sv->saved_src = -1;
    sv->saved_drn = -1;
    sv->saved_buffer_node = NULL;
    sv->saved_path = -1;
    sv->saved_buffer = NULL;
    sv->transferSize = 0; 
    sv->transferring = FALSE;

    sv->setupPathCallback = NULL;

    return(i);
}

static int
_allocPidEntry(int pid)
{
    int i;

    for (i=0; i<TABLE_SIZE; i++)
	if (_svPidEntry[i].pid == 0)
	    break;

    /* xxx handle table overflow? */

    _svPidEntry[i].pid = pid;
    _svPidEntry[i].contextNum = 0;

    return i;
}

static int
_findPidEntry(int alloc)
{
    int pid;
    int i, j;

    pid = (int) getpid();

    for (i=0; i<TABLE_SIZE; i++)
	if (_svPidEntry[i].pid == pid)
	    return(i);

    i = _allocPidEntry(pid);
    if (alloc) {
	j = _allocContextEntry();
	_svPidEntry[i].contextNum = j;
    }    

    return i;
}

ConnectionState *
_findContext(void)
{
    int pidEntry, contextEntry;

    pidEntry = _findPidEntry(TRUE);
    contextEntry = _svPidEntry[pidEntry].contextNum;

    return _svContextTable[contextEntry].sv;
}

svContext
svNewContext(void)
{
    int pidEntry;
    int newContextEntry;

    newContextEntry = _allocContextEntry();
    pidEntry = _findPidEntry(TRUE);
    _svPidEntry[pidEntry].contextNum = newContextEntry;

    return(newContextEntry);
}

void
svSetContext(svContext contextNum)
{
    int pidEntry;

    pidEntry = _findPidEntry(FALSE);
    _svPidEntry[pidEntry].contextNum = contextNum;
}

svContext
svCurrentContext(void)
{
    int pidEntry;

    pidEntry = _findPidEntry(TRUE);
    return _svPidEntry[pidEntry].contextNum;
}

void
svFreeContext(svContext contextNum)
{
    /* xxx write */
    assert(0);
}

void
svSetupPathCallback(SVSetupPathCallback callbackFunction)
{
    ConnectionState *sv;

    sv = _findContext();
    sv->setupPathCallback = callbackFunction;
}

/***********************************************************************
 * Internal support functions;
 ***********************************************************************
 */

/**********************************************************************
 * _initializeVL -
 *
 * establish our connection to the video daemon and find the device
 * number of the galileo device.
 **********************************************************************
 */

static VLServer svr;

static int
_initializeVL(ConnectionState *sv,
	      SvConnection *srcConn,
	      SvConnection *drnConn) {
    int i;

    char *devName;
    VLDevList devlist;

    /* Find a connection record with a device name in it */
    if (strcmp(srcConn->device, "") != 0)
	devName = srcConn->device;
    else
	devName = drnConn->device;

    if (!svr)
	svr = vlOpenVideo("");

    sv->svr = svr;
    if (!svr)
	return svInitFailed;


    /* Find the desired device */
    
    vlGetDeviceList(sv->svr, &devlist);
    for (i=0; i<devlist.numDevices; i++)
	if (strcmp(devlist.devices[i].name, devName) == 0)
	    break;

    /* it wasn't there */
    if (i == devlist.numDevices)
	return svDeviceNotFound;

    /* we found it, now save it for future use */
    sv->dev = devlist.devices[i].dev;

    /* galileo hack */
    if (strcmp(devName, "ev1") == 0)
	sv->deviceWidthFactor = 2;

    /* vino hack */
    if (strcmp(devName, "vino") == 0)
	sv->deviceHeightFactor = 2;

    atexit(_shutdownVL);
    return svSuccess;
}

/**********************************************************************
 * _shutdownVL -
 *
 * cleanup VL before exiting.
 **********************************************************************
 */

static void
_shutdownVL(void)
{
    ConnectionState *sv;

    sv = _findContext();

    if (sv->transferring)
	vlEndTransfer(sv->svr, sv->saved_path);

    if (sv->saved_buffer) {
	vlDeregisterBuffer(sv->svr, sv->saved_path,
			   sv->saved_buffer_node, sv->saved_buffer);
	vlDestroyBuffer(sv->svr, sv->saved_buffer);
    }

    if (sv->saved_path)
	vlDestroyPath(sv->svr, sv->saved_path);

    vlCloseVideo(sv->svr);
}

/**********************************************************************
 * _configurePath -
 *
 * make sure we have a path from the input specified in
 * svSelectedInputConnection to the output specified in
 * svSelectedOutputConnection.
 **********************************************************************
 */

static int
_configurePath(int src, int drn, ConnectionState *sv)
{
    int i;
    int status;
    SvConnection *srcConn, *drnConn;
    VLNode srcNode, drnNode;
    VLPath path;


    if (src == sv->saved_src && drn == sv->saved_drn)
	return svSuccess;

    /* We're not trying to use the same path, so tear down
     * the old path if it exists and build the new one up
     * from scratch;
     */

    if (sv->saved_path != -1) {
	if (sv->saved_buffer) {
	    vlDeregisterBuffer(sv->svr, sv->saved_path,
			       sv->saved_buffer_node, sv->saved_buffer);
	    vlDestroyBuffer(sv->svr, sv->saved_buffer);
	}
	    
	vlDestroyPath(sv->svr, sv->saved_path);
	sv->saved_path = -1;
	sv->saved_src = -1;
	sv->saved_drn = -1;
	sv->saved_buffer_node = NULL;
	sv->saved_buffer = NULL;

	sv->transferCountRemaining = 0;
	sv->freeFrameNextXfer    = FALSE;
	sv->transferring         = FALSE;
    }

    /* Find the connection record for the selected source and
     * selected drain connection;
     */

    for (i=0; i<numInputConnections; i++)
	if (inputConnections[i].id == src)
	    break;

    if (i == numInputConnections)
	return svBadInputConnection;

    srcConn = &inputConnections[i];

    for (i=0; i<numOutputConnections; i++)
	if (outputConnections[i].id == drn)
	    break;

    if (i == numOutputConnections)
	return svBadOutputConnection;

    drnConn = &outputConnections[i];

    /* We've got our two node records, now call initializeVL to
     * open the VL connection and pick the device we'll be using;
     */
    
    if (!sv->vlInitialized++)
	if ((status = _initializeVL(sv, srcConn, drnConn)) != svSuccess)
	    return status;

    /* Now that we know what kind of nodes we should be using,
     * create them;
     */

    srcNode = vlGetNode(sv->svr, srcConn->type, srcConn->kind, srcConn->number);
    drnNode = vlGetNode(sv->svr, drnConn->type, drnConn->kind, drnConn->number);

    if (srcNode == -1)
	return svBadInputNode;

    if (drnNode == -1)
	return svBadOutputNode;

    path = vlCreatePath(sv->svr, sv->dev, srcNode, drnNode);
    if (path == -1)
	return svBadPath;

    /*
     * If we make it this far, find out if we can now setup the path;
     */
       
    if (vlSetupPaths(sv->svr, &path, 1, sv->pathUsageMode, sv->controlUsageMode) < 0)
	return svBadPathSetup;

    if (sv->setupPathCallback)
	(sv->setupPathCallback)(sv->svr, path, srcNode, drnNode);

    /*
     * Still going?  Then all we need to do is see if we
     * need to pound on the muxswitch control to access the
     * correct connection;
     */

    if (srcConn->muxValue >= 0)
	vlSetMuxSwitch(sv->svr, path, srcNode, srcConn->muxValue);

    if (drnConn->muxValue >= 0)
	vlSetMuxSwitch(sv->svr, path, drnNode, drnConn->muxValue);

    /*
     * Now, before we can *really* do anything, we have to be able
     * to create and register a buffer to use in the frame transfer;
     */

    {
	VLNode node = NULL;
	VLBuffer buffer;
	
	if (srcConn->kind == VL_MEM)
	    node = srcNode;
	if (drnConn->kind == VL_MEM)
	    node = drnNode;

	if (node) {
	    vlSetPacking(sv->svr, path, node, sv->packing);
	    vlSetCapType(sv->svr, path, node, VL_CAPTURE_INTERLEAVED);

	    buffer = vlCreateBuffer(sv->svr, path, node, sv->frameCount);
	    if (buffer == NULL)
		return svBufferAllocFailed;

	    if (vlRegisterBuffer(sv->svr, path, node, buffer) != VLSuccess) {
		vlDestroyBuffer(sv->svr, buffer);
		return svBufferRegisterFailed;
	    }

	    sv->saved_buffer_node = node;
	    sv->saved_buffer = buffer;
	}
    }

    /*
     * Everything worked, so record the src connection,
     * the drain connection, and the path we just created
     * for future reference;
     */

    sv->saved_src  = src;
    sv->saved_drn  = drn;
    sv->saved_path = path;
    sv->transferSize = vlGetTransferSize(sv->svr, sv->saved_path);

    return svSuccess;
}

/**********************************************************************
 * BeginTransfer - 
 *
 * initiate a transfer if necessary;
 **********************************************************************
 */

/* XXX add triggering??? */

static int
_beginTransfer(ConnectionState *sv)
{
    int ret;
    VLTransferDescriptor xferDesc;

    if (sv->freeFrameNextXfer) {
	vlPutFree(sv->svr, sv->saved_buffer);
	sv->freeFrameNextXfer = FALSE;
    }

    if (sv->transferCountRemaining == 0) {
	if (sv->transferring) {
	    if (vlEndTransfer(sv->svr, sv->saved_path) < 0)  {
		/*** If we were only preempted,
		 *** try to re-setup our path;
		 ***/
		if (sv->recoverFromPreemption &&  vlErrno == VLBadAccess) { 
		    ret = vlSetupPaths(sv->svr, &(sv->saved_path), 1,
				       sv->pathUsageMode, sv->controlUsageMode);
		    if (ret)
			return svEndTransferFixupFailed;

		    if (sv->setupPathCallback)
			(sv->setupPathCallback)(sv->svr, sv->saved_path,
						sv->saved_src, sv->saved_drn);
		}
		else
		    return svEndTransferFailed;
	    }

	    sv->transferring = FALSE;
	    if (vlBufferReset(sv->svr, sv->saved_buffer) < 0)
		return svBufferResetFailed;
	}

	if (sv->transferMode == svTRANSFER_DISCRETE) {
	    xferDesc.mode    = VL_TRANSFER_MODE_DISCRETE;
	    xferDesc.count   = (short) sv->frameCount;
	    xferDesc.delay   = 0;
	    xferDesc.trigger = VLTriggerImmediate;
	}
	
	if (sv->transferMode == svTRANSFER_CONTINUOUS) {
	    xferDesc.mode = VL_TRANSFER_MODE_CONTINUOUS;
	    xferDesc.trigger = VLTriggerImmediate;
	}

	if (vlBeginTransfer(sv->svr, sv->saved_path, 1, &xferDesc))
	    return svBeginTransferFailed;
	
	sv->transferring = TRUE;
	sv->transferCountRemaining = sv->frameCount;
    }
    
    return svSuccess;
}

/***********************************************************************
 * Public Utility routines for settting transfer parameters;
 ***********************************************************************
 */

/**********************************************************************
 *
 * svSelectInput -
 *
 * Select the input connection you wish to use.
 **********************************************************************
 */

void
svSelectInput(int input)
{
    ConnectionState *sv;

    sv = _findContext();
    sv->input = input;
}

/**********************************************************************
 *
 * svSelectOutput -
 *
 * Select the output connection you wish to use.
 **********************************************************************
 */

void
svSelectOutput(int output)
{
    ConnectionState *sv;

    sv = _findContext();
    sv->output = output;
}

/**********************************************************************
 *
 * svNodeAccessMode -
 *
 * Set the locking mode for nodes.
 **********************************************************************
 */

void
svNodeAccessMode(VLUsageType mode)
{
    ConnectionState *sv;

    sv = _findContext();
    sv->pathUsageMode = mode;
}

/**********************************************************************
 *
 * svControlAccessMode -
 *
 * Set the locking mode for controls.
 **********************************************************************
 */

void
svControlAccessMode(VLUsageType mode)
{
    ConnectionState *sv;

    sv = _findContext();
    sv->controlUsageMode = mode;
}

/**********************************************************************
 *
 * svSetFrameCount -
 *
 * Set the number of frames in the ring buffer.
 **********************************************************************
 */

void
svSetFrameCount(int count)
{
    ConnectionState *sv;

    sv = _findContext();
    sv->frameCount = count;
}

/**********************************************************************
 *
 * svSetImagePacking -
 *
 * Set the image packing for a particular context.
 **********************************************************************
 */

void
svSetImagePacking(int packing)
{
    ConnectionState *sv;

    sv = _findContext();
    sv->packing = packing;
}

/**********************************************************************
 *
 * svSetTransferMode -
 *
 * Set the transferMode.
 **********************************************************************
 */

void
svSetTransferMode(int mode)
{
    ConnectionState *sv;

    sv = _findContext();
    sv->transferMode = mode;
}

/**********************************************************************
 *
 * svCompressedImages -
 *
 * Set the transferMode.
 **********************************************************************
 */

void
svCompressedImages(int saveCompressed)
{
    ConnectionState *sv;

    sv = _findContext();
    sv->saveImagesCompressed = saveCompressed;
}

/**********************************************************************
 *
 * svRecoverFromPreemption -
 *
 * Attempt to recover from a preemption when our path was
 * setup in VL_SHARE mode;
 **********************************************************************
 */

void
svRecoverFromPreemption(void)
{
    ConnectionState *sv;

    sv = _findContext();
    sv->recoverFromPreemption = TRUE;
}

/***********************************************************************
 * Routines for grabbing, putting frames;
 ***********************************************************************
 */

/**********************************************************************
 * svGetFrame -
 *
 * Grab a frame from the selected input.
 **********************************************************************
 */

int
svGetFrame(svImage *frame)
{
    int status;


    VLInfoPtr info;
    DMImageInfo *imageInfo;
    ConnectionState *sv;

    sv = _findContext();

    if ((status = _configurePath(sv->input, svOUTPUT_MEMORY, sv)) != svSuccess)
	return status;

    if ((status = _beginTransfer(sv)) != svSuccess)
	return status;

    info = dmRBLockReadable(sv->saved_buffer, 1, NULL, 1 /* block */);
    if (sv->transferMode == svTRANSFER_DISCRETE)
	sv->transferCountRemaining--;
    sv->freeFrameNextXfer = TRUE;

    /* XXX note that in the ev1_dma.c , imageInfo->width is set
     *     to wds_per_line; should almost certainly convert this
     *     to the number of bytes wide the line is, check to see
     *     if the dm struct is documented that way or not;
     */

    /*
     * XXX we turned on interleave mode for vino; It seems to work,
     *     but the imageInfo->height doesn't seem to take this
     *     info account, so we need a special vino whack also.
     */

    /*
     * XXX the values coming back from the vino driver in imageInfo->packing
     *     don't match what we asked for;
     *     set to 2, we get back frames with packing = 0;
     *     set to 5, we get back frames with packing = 2;
     *
     *     So, blow off value passed back, just use the value we originally
     *     requested that is stored in sv->packing;
     */

    imageInfo = vlGetImageInfo(sv->svr, sv->saved_buffer, info);

    frame->width   = imageInfo->width  * sv->deviceWidthFactor;
    frame->height  = imageInfo->height * sv->deviceHeightFactor;
    /* XXX frame->packing = imageInfo->packing; */
    frame->packing = sv->packing;
    frame->data    = vlGetActiveRegion(sv->svr, sv->saved_buffer, info);
    frame->dataSize = sv->transferSize;

    return svSuccess;
}

/**********************************************************************
 * svPutFrame -
 *
 * Use an image as a frame of video output;
 **********************************************************************
 */

int
svPutFrame(svImage *frame)
{
    int status;
     VLInfoPtr info;
    char *dataPtr;
    ConnectionState *sv;

    sv = _findContext();

    if ((status = _configurePath(svINPUT_MEMORY, sv->output, sv)) != svSuccess)
	return status;

    if ((status = _beginTransfer(sv)) != svSuccess)
	return status;

    /* xxx we should check for size mismatches */

    info = dmRBLockWritable(sv->saved_buffer, sv->transferSize, 1,
			    NULL, 1 /* block */);
    dataPtr = vlGetActiveRegion(sv->svr, sv->saved_buffer, info);
    bcopy(frame->data, dataPtr, frame->dataSize);

    vlPutValid(sv->svr, sv->saved_buffer);
    if (sv->transferMode == svTRANSFER_DISCRETE)
	sv->transferCountRemaining--; 

    return svSuccess;
}

