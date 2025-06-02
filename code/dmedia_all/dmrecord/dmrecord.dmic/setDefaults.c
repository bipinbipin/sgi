#include "dmrecord.h"

#define DMICJPEG 'jpeg'

VLDevice UseThisDevice;
VLNodeInfo UseThisNode[MAXNODES];
char *UseThisEngine; /* VICE, IMPACT or "cosmo" */

int usedmIC;
int UseThisdmIC; /* the er of the realtime jpeg compressor */
int dmICStatus;
int CosmoStatus;
int useMVP = 0;
int useIMPACT = 0;
int screenPortNum;
char *screenPortName;

/*
 * Check the video device, its attached nodes,
 * and the compressor anhe results in UseThisDevice,
 * UseThisNode, UseThisdStatus, CosmoStatus, usedmIC,
 * UseThisEngine, and useMVP.
 */

void setDefaults
    (
    void
    )
{
    VLServer	server;
    int		devNum;
    VLDevList	devlist;
    VLDevice	*cdevice;
    VLNodeInfo	*node;
    int		i;
    int numGoodNodes = 0;
    CLhandle CosmoHandle;
    int numIC;
    DMimageconverter ic;
    DMparams *p;

    options.height 		= 0;
    options.videoDevice	= NULL;
    options.videoPortName 	= NULL;
    options.critical 		= FALSE;
    options.verbose 		= FALSE;
    options.fileName 		= NULL;
    options.movieTitle 	= NULL;
    options.video 		= FALSE;
    options.audio 		= FALSE;
    options.audioChannels 	= 2;
    options.seconds 		= 0;
    options.batchMode		= FALSE;
    options.compressionScheme	= FALSE;
    options.compressionEngine	= FALSE;
    options.qualityFactor	= 75;
    options.halfx		= 0;
    options.halfy		= 0;
    options.avrFrameRate	= 0;
    options.nodeKind		= VL_VIDEO;
    options.frames		= 0;
    options.unit		= UNIT_FIELD;
    options.inbufs		= INBUFS;
    options.outbufs		= OUTBUFS;
    options.hiPriority		= 0;
    options.nowrite		= 0;
    options.timing		= 0;
    options.use30000		= 0;

    movie.format		= MV_FORMAT_QT;
    video.fdominance		= 1;

    UseThisDevice.numNodes = -1; /* mark as no input device found (yet) */

    server = vlOpenVideo("");

/* >>> YDL TEMPORARY FIX  */
    video.server = server;
/* <<< YDL TEMPORARY FIX */

    /* Get a list of all video devices */
    vlGetDeviceList(server, &devlist);

    for (devNum = 0; devNum < devlist.numDevices; devNum++) {
	cdevice = &(devlist.devices[devNum]);
	if (strcmp(cdevice->name, MVP) == 0) {
	    useMVP = 1;
	    goto devFound;
	}
	if (strcmp(cdevice->name, EV1) == 0) {
	    useMVP = 0;
	    goto devFound;
	}
        if (strcmp(cdevice->name, IMPACT) == 0) {
            useIMPACT = 1;
            goto devFound;
        }
    }

/* >>> YDL TEMPORARY FIX  */
/*
    vlCloseVideo(server);
*/
/* <<< YDL TEMPORARY FIX */

    return;

devFound:
    if (options.verbose > 1)
        printf("Available ports for video input device %s:\n", cdevice->name);

    UseThisDevice.dev = cdevice->dev;
    memcpy(UseThisDevice.name, cdevice->name, sizeof(UseThisDevice.name));
    UseThisDevice.numNodes = 0;
    UseThisDevice.nodes  = UseThisNode;

    numGoodNodes = 0;
    /* Find all available input nodes for the specified kind */
    for (i = 0; i < cdevice->numNodes; i++) {
	node = &(cdevice->nodes[i]);
	if ((useMVP == 0) && (node->number == 2)) {
	/* port #2 is not supported for this program if using ev1 */
		continue;
	}
	if ((node->type == VL_SRC) && (node->kind == options.nodeKind)) {
	    UseThisNode[numGoodNodes] = *node;
	    if (options.verbose > 1)
		printf("   port=%s' (%d)\n", UseThisNode[numGoodNodes].name, UseThisNode[numGoodNodes].number);
	    if (++numGoodNodes >= MAXNODES)
		break;
	}
	if ((node->type == VL_SRC) && (node->kind == VL_SCREEN)) {
	    screenPortName = (*node).name;
	    screenPortNum = (*node).number;
	}
    }
    UseThisDevice.numNodes = numGoodNodes;

/* >>> YDL TEMPORARY FIX  */
/*
    vlCloseVideo(server);
*/
/* <<< YDL TEMPORARY FIX */

    if (useMVP)
	UseThisEngine = strdup(VICE);
    else if (useIMPACT)
	UseThisEngine = strdup(IMPACT);
    else
	goto checkCosmo;

    usedmIC = 1;
    /* Find real-time jpeg compressor and try to open it */
    dmICStatus = DM_FAILURE;
    numIC = dmICGetNum();
    dmParamsCreate(&p);
    for (i = 0; i < numIC; i++) {
	/* Get description of next converter */
	if (dmICGetDescription(i, p) != DM_SUCCESS) {
	    continue;
	}

	/* Look for the right combination */
	if ((dmParamsGetInt(p, DM_IC_ID) == DMICJPEG) &&
	     (dmParamsGetEnum(p, DM_IC_CODE_DIRECTION) == DM_IC_CODE_DIRECTION_ENCODE) &&
	     (dmParamsGetEnum(p, DM_IC_SPEED) == DM_IC_SPEED_REALTIME)) {
	    if ((dmICStatus = dmICCreate(i, &ic)) == DM_SUCCESS) {
		dmICDestroy(ic);
		UseThisdmIC = i;
		break;
	    }
	}
    }
    dmParamsDestroy(p);
    return;

checkCosmo:
    usedmIC = 0;
    UseThisEngine = strdup("cosmo");
    CosmoStatus = clOpenCompressor( CL_JPEG_COSMO, &CosmoHandle);
    if (CosmoStatus == SUCCESS)
	clCloseCompressor(CosmoHandle);
    return;
 

} /* setDefaults */
