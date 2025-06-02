#include "dmrecord.h"
#include "handy.h"


int getSpecifiedPort
    (
    void
    )
{
    VLServer server;
    VLPath path;
    VLDev device = UseThisDevice.dev;
    VLNode devNode;
    VLNode srcNode, dstNode;
    VLControlValue val;
    int videoPort;
    VLNodeInfo *node;
    int i;
    char *portName = options.videoPortName;

/* >>> YDL TEMPORARY FIX  */
/*
    server = vlOpenVideo("");
*/
    server = video.server;
/* <<< YDL TEMPORARY FIX */

    if (options.nodeKind == VL_SCREEN) {
	return 0;
    }

    if (portName == NULL) {
	/* Port not specified. Find default port and use it */
	/* Set up a dummy path so we can use it to find default port */
	VC( devNode=vlGetNode( server, VL_DEVICE, 0, VL_ANY ) );
	VC( path=vlCreatePath( server, device, devNode, devNode ) );
	VC( vlSetupPaths(server, &path, 1, VL_SHARE, VL_READ_ONLY) );
	VC( vlGetControl( server, path, devNode, VL_DEFAULT_SOURCE, &val) );
	options.videoPortNum = videoPort = val.intVal;
	vlDestroyPath(server, path);
    } else if ( (*(portName) >= '0') && (*(portName) <= '9')) {
	options.videoPortNum = videoPort = atoi(portName);
    } else {
	goto nameUsed;
    }

/* >>> YDL TEMPORARY FIX  */
/*
    vlCloseVideo(server);
*/
/* <<< YDL TEMPORARY FIX */

    /* Make sure that this is indeed an appropriate port number */
    for (i = 0; i < UseThisDevice.numNodes; i++) {
	node = &(UseThisNode[i]);
	if (node->number == videoPort) {
	    options.videoPortName = node->name;
	    return videoPort;
	}
    }
    return -1;

nameUsed:
/* >>> YDL TEMPORARY FIX */
/*
    vlCloseVideo(server);
*/
/* <<< YDL TEMPORARY FIX */

    /* A symbolic name is used for the port specification */
    /* Look for an appropriate port that matches */
    for (i = 0; i < UseThisDevice.numNodes; i++) {
	node = &(UseThisNode[i]);
	/* ignore case and allow shorthand */
	if (strncasecmp(node->name, portName, strlen(portName)) == 0) {
	    options.videoPortName = node->name;
	    return (options.videoPortNum = node->number);
	}
    }
    return -1;
} /* getSpecifiedPort */

