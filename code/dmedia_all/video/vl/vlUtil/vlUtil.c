/* $Id: vlUtil.c,v 1.3 1994/07/25 19:28:00 dpb Exp $ */

#include <string.h>
#include <vlUtil/vlUtil.h>

VLPath
vlCreateDevPath(VLServer svr, VLDev dev, VLNode *devNode)
{
    VLPath path;
    VLNode node;

    path = vlCreatePath(svr, dev, VL_ANY, VL_ANY);
    node = vlGetNode(svr, VL_DEVICE, 0, 0);
    vlAddNode(svr, path, node);

    if (vlSetupPaths(svr, &path, 1, VL_SHARE, VL_READ_ONLY) != VLSuccess) {
	vlDestroyPath(svr, path);
	return NULL;
    }

    if (devNode)
	*devNode = node;

    return(path);
}

int
vlFindDevice(VLServer svr,
	     char *deviceName,
	     VLDev *dev)
{
    int i;
    VLDevice *pDevice;
    VLDevList devList;

    vlGetDeviceList(svr, &devList);
    
    *dev = -1;
    pDevice = devList.devices;

    for (i=0; i<devList.numDevices; i++) {
	if (strcmp(pDevice->name, deviceName) == 0)
	    break;
	pDevice++;
    }

    if (i < devList.numDevices) {
	*dev = pDevice->dev;
	return (VLSuccess);
    }

    return (VLBadDevice);
}

int
vlSetIntControl(VLServer svr,
		VLPath path,
		VLNode node,
		VLControlType control,
		int value)
{
    VLControlValue val;
    
    val.intVal = value;
    return vlSetControl(svr, path, node, control, &val);
}

int
vlSetIntPairControl(VLServer svr,
		    VLPath path,
		    VLNode node,
		    VLControlType control,
		    int v1,
		    int v2)
{
    VLControlValue val;
    
    val.xyVal.x = v1;
    val.xyVal.y = v2;

    return vlSetControl(svr, path, node, control, &val);
}

int
vlSetIntDeviceControl(VLServer svr,
		      char *deviceName,
		      VLControlType control,
		      int value)
{
    int ret;
    VLDev device;
    VLNode devNode;
    VLPath devPath;
    VLControlValue val;

    if (ret=vlFindDevice(svr, deviceName, &device))
	return ret;

    devPath = vlCreateDevPath(svr, device, &devNode);
 
    val.intVal = value;
    ret = vlSetControl(svr, devPath, devNode, control, &val);
    vlDestroyPath(svr, devPath);

    return ret;
}

int
vlGetIntControl(VLServer svr,
		VLPath path,
		VLNode node,
		VLControlType control,
		int *value)
{
    int status;
    VLControlValue val;
    
    status = vlGetControl(svr, path, node, control, &val);
    *value = val.intVal;

    return status;
}
