/* $Id: vlUtil.h,v 1.2 1994/07/25 18:31:55 dpb Exp $ */

/**********************************************************************
 *  
 * vlUtil.h - utility macros/functions to make vl control
 *            settings a little easier and safer.
 *
 **********************************************************************
 */

#ifndef _VL_UTIL_H_
#define _VL_UTIL_H_

#include <vl/vl.h>

#define vlSetMuxSwitch(svr, path, node, value) \
	vlSetIntControl(svr, path, node, VL_MUXSWITCH, value)

#define vlSetPacking(svr, path, node, value) \
	vlSetIntControl(svr, path, node, VL_PACKING, value)

#define vlSetCapType(svr, path, node, value) \
	vlSetIntControl(svr, path, node, VL_CAP_TYPE, value)

#define vlSetSize(svr, path, node, w, h) \
	vlSetIntPairControl(svr, path, node, VL_SIZE, w, h)

#define vlSetTiming(svr, devName, timing)	\
	vlSetIntDeviceControl(svr, devName, VL_TIMING, timing)


int
vlSetIntControl(VLServer, 
		VLPath,
		VLNode,
		VLControlType,
		int);

int
vlSetIntPairControl(VLServer,
		    VLPath,
		    VLNode,
		    VLControlType,
		    int,
		    int);


int
vlFindDevice(VLServer svr,
	     char *deviceName,
	     VLDev *dev);	  /* RETURN */

VLPath
vlCreateDevPath(VLServer svr,
		VLDev dev,
		VLNode *devNode); /* RETURN */

int
vlGetIntControl(VLServer, 
		VLPath,
		VLNode,
		VLControlType,
		int *);

int
vlSetIntControl(VLServer, 
		VLPath,
		VLNode,
		VLControlType,
		int);


#endif /* _VL_UTIL_H */
