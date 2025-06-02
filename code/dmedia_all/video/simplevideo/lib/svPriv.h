/* $Id: svPriv.h,v 1.2 1994/07/16 01:47:49 dpb Exp $ */

#ifndef _SV_PRIV_H
#define _SV_PRIV_H

/*
 * Connection state info;
 */

typedef struct {
    int           vlInitialized;
    VLServer      svr;
    VLDev         dev;
    int           input;
    int           output;
    VLUsageType   pathUsageMode;
    VLUsageType   controlUsageMode;
    int           frameCount;
    int 	  packing;
    int		  saveImagesCompressed;
    int		  transferMode;
    int           transferCountRemaining;
    int           freeFrameNextXfer;
    int		  recoverFromPreemption;
    int	 	  deviceWidthFactor;
    int	 	  deviceHeightFactor;

    int           saved_src;
    int           saved_drn;
    VLNode        saved_buffer_node;
    VLPath        saved_path;
    VLBuffer      saved_buffer;
    int		  transferSize;
    int 	  transferring;

    SVSetupPathCallback setupPathCallback;
} ConnectionState;

/*
 * prototypes;
 */

ConnectionState *
_findContext(void);


#endif /* _SV_PRIV_H */

