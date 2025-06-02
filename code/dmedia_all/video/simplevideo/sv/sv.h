/* $Id: sv.h,v 1.8 1994/07/25 18:30:28 dpb Exp $ */

/**********************************************************************
 *  
 * sv.h - simple video interface defines and function prototypes;
 *
 **********************************************************************
 */

#ifndef _SV_H
#define _SV_H

#include <vlUtil/vlUtil.h>


/**********************************************************************
 * sv return codes - here's what can go wrong;
 **********************************************************************
 */

#define svSuccess	          0
#define svInitFailed	          1
#define svDeviceNotFound          2
#define svBadInputConnection      3
#define svBadOutputConnection     4
#define svBadInputNode            5
#define svBadOutputNode           6
#define svBadPath                 7
#define svBadPathSetup            8
#define svBufferAllocFailed       9
#define svBufferRegisterFailed   10
#define svBeginTransferFailed    11
#define svEndTransferFailed      12
#define svEndTransferFixupFailed 13
#define svBufferResetFailed      14
#define svBadFile                15
#define svBadImageType		 16
#define svCompressMissing	 17
#define svCompressFailed	 18
#define svFileWriteFailed	 19

/**********************************************************************
 * Context management -
 **********************************************************************
 */

typedef int svContext;

svContext
svNewContext(void);

void
svSetContext(svContext);

svContext
svCurrentContext(void);

void
svFreeContext(svContext);


/**********************************************************************
 * Input/Output selection - 
 *
 * Functions to select the input connection for svGetFrame(),
 * and the output connection for svPutFrame() -
 * choose from the list below;
 **********************************************************************
 */


/*
 * Indy connections -
 */

#define vnINPUT_INDYCAM		1
#define vnINPUT_COMPOSITE	2
#define vnINPUT_SVIDEO		3


/*
 * Galileo connections -
 */
 
#define gvINPUT_COMPOSITE_1	4
#define gvINPUT_SVIDEO		5
#define gvINPUT_COMPOSITE_2	6
#define gvINPUT_DIGITAL_1	7
#define gvINPUT_DIGITAL_2	8
#define gvOUTPUT_ANALOG		9
#define gvOUTPUT_DIGITAL       10


void svSelectInput(int);
void svSelectOutput(int);

/**********************************************************************
 * Frame & image file allocation/deallocation, input/output -
 **********************************************************************
 */

/* magic numbers */

#define RGB_MAGIC		0x1da
#define RGB_SV_IMAGE_MAGIC	0x1db

typedef struct {
    int width;
    int height;
    int packing;
    int dataMalloced;
    int dataSize;
    int compressed;
    char *data;
} svImage;


/* The returned image should be freed with svFreeImage() */
svImage *
svNewImage(void);

void
svFreeImage(svImage **);

int
svSaveImage(char *filename,
	    svImage *frame);

/* svLoadImage will allocate an svImage record and fill it in,
 * you must free it with svFreeImage();
 */
int
svLoadImage(char *filename,
	    svImage **frame /* return */);

int
svViewImage(svImage *frame, int x, int y);

void
svCompressedImages(int saveCompressed);


/**********************************************************************
 * Frame data packing;
 *
 * valid values are the #defines that start VL_PACKING_ in vl.h,
 * currently supported values are:
 *
 * VL_PACKING_RGBA_8
 * VL_PACKING_RGB_8
 * VL_PACKING_YVYU_422_8
 *
 * Others may work, these are the the only ones I've tried...
 *
 **********************************************************************
 */

void
svSetImagePacking(int packingType);


/**********************************************************************
 * Use these functions to read a frame of video into an image or
 * to write an image out to video.
 **********************************************************************
 */

int
svGetFrame(svImage *);

int
svPutFrame(svImage *);

/**********************************************************************
 * node/control access;
 *
 * Pass in VL_LOCK if desired, default is VL_SHARE;
 **********************************************************************
 */

void
svNodeAccessMode(VLUsageType);

void
svControlAccessMode(VLUsageType);

/*  if we use a usage mode of VL_SHARE, we may want
 *  to automatically try and recover if our transfer in
 *  progress is pre-empted;
 */
 
void
svRecoverFromPreemption(void);

/**********************************************************************
 * Modify transfer parameters;
 *
 * Default count = 1,
 * transfer mode = DISCRETE;
 *
 **********************************************************************
 */

void
svSetFrameCount(int);


#define svTRANSFER_DISCRETE	0
#define svTRANSFER_CONTINUOUS	1

void
svSetTransferMode(int);

/**********************************************************************
 * Setup path callback hook
 **********************************************************************
 */
typedef void (*SVSetupPathCallback)(VLServer, VLPath, VLNode, VLNode);

void
svSetupPathCallback(SVSetupPathCallback);

#endif /* SV_H */

