/*
** vl.c -- video library code for the blendicomp example program
*/

#include "blendicomp.h"

vlInit()
{
    int rc;
    VLControlValue val;

    video.vlSrv = vlOpenVideo( "" );
    if (video.vlSrv == NULL) {
	fprintf(stderr,"failed to open a connection to the video daemon\n");
	return -1;
    }

    /*
    ** get all the pesky little nodes that are gonna be on our little
    ** path.
    */
    video.node_codec = vlGetNode(video.vlSrv, VL_SRC, VL_CODEC, codec.vlcodec);
    video.node_screen = vlGetNode(video.vlSrv, VL_SRC, VL_SCREEN, VL_ANY);
    video.node_video = vlGetNode(video.vlSrv, VL_DRN, VL_VIDEO, 
					VL_MGV_NODE_NUMBER_VIDEO_1);
    video.node_blend = vlGetNode(video.vlSrv, VL_INTERNAL, VL_BLENDER, VL_ANY);

    if (video.node_codec == -1) {
	fprintf(stderr,"failed to get VL_CODEC source node\n");
	return -1;
    }

    if (video.node_screen == -1) {
	fprintf(stderr,"failed to get VL_SCREEN source node\n");
	return -1;
    }

    if (video.node_video == -1) {
	fprintf(stderr,"failed to get VL_VIDEO drain node\n");
	return -1;
    }

    if (video.node_blend == -1) {
	fprintf(stderr,"failed to get VL_BLENDER drain node\n");
	return -1;
    }

    /*
    ** create the path containing all those nodes
    */
    video.vlPath = vlCreatePath( video.vlSrv, VL_ANY, 
				    video.node_codec, video.node_video );
    if (video.vlPath == -1) {
	vlPerror("create path");
	return -1;
    }

    if (vlAddNode(video.vlSrv, video.vlPath, video.node_screen) != VLSuccess) {
	vlPerror("add screen node");
	return -1;
    }

    if (vlAddNode(video.vlSrv, video.vlPath, video.node_blend) != VLSuccess) {
	vlPerror("add blender node");
	return -1;
    }

    /*
    ** setup the path
    */
    rc = vlSetupPaths(video.vlSrv, (VLPathList)&video.vlPath, 1, 
							VL_SHARE, VL_SHARE );
    if (rc != 0) {
	vlPerror("setup path");
	return -1;
    }

    /*
    ** and now plumb the blender into the path
    */
    rc = vlSetConnection( video.vlSrv, video.vlPath, 
		    video.node_codec, VL_IMPACT_PORT_PIXEL_SRC, 
		    video.node_blend, VL_IMPACT_PORT_PIXEL_DRN_B, TRUE );
    if (rc < 0) {
	vlPerror("setconnection: codec to blend B");
	return -1;
    }

    rc = vlSetConnection( video.vlSrv, video.vlPath, 
		    video.node_screen, VL_IMPACT_PORT_PIXEL_SRC, 
		    video.node_blend, VL_IMPACT_PORT_PIXEL_DRN_A, TRUE );
    if (rc < 0) {
	vlPerror("setconnection: screen to blend A");
	return -1;
    }

    rc = vlSetConnection( video.vlSrv, video.vlPath, 
		    video.node_screen, VL_IMPACT_PORT_ALPHA_SRC, 
		    video.node_blend, VL_IMPACT_PORT_ALPHA_DRN_A, TRUE );
    if (rc < 0) {
	vlPerror("setconnection: screen to blend A");
	return -1;
    }

    rc = vlSetConnection( video.vlSrv, video.vlPath, 
		    video.node_blend, VL_IMPACT_PORT_PIXEL_SRC, 
		    video.node_video, VL_IMPACT_PORT_PIXEL_DRN, TRUE );
    if (rc < 0) {
	vlPerror("setconnection: blend to video");
	return -1;
    }

    /*
    ** don't forget to set the size of the video that will be
    ** sent to the VL_CODEC node
    */
    val.xyVal.x = image.source_width;
    val.xyVal.y = image.source_height;
    rc = vlSetControl(video.vlSrv, video.vlPath, 
				video.node_codec, VL_SIZE, &val);
    if (rc != 0) {
	vlPerror("set codec size");
	return -1;
    }

    /*
    ** and finally set controls on the blender for alpha keying
    */
    val.intVal = VL_MGV_KEYERMODE_NONE;
    rc = vlSetControl(video.vlSrv, video.vlPath, video.node_blend, 
					    VL_MGV_KEYER_MODE, &val);
    if (rc != 0) {
	vlPerror("keyermode alpha");
	return -1;
    }

    val.intVal = 2;		/* value 1.0 */
    rc = vlSetControl(video.vlSrv, video.vlPath, video.node_blend, 
					    VL_MGV_BLEND_B_ALPHA_SELECT, &val);
    if (rc != 0) {
	vlPerror("blend B alpha select value 1.0");
	return -1;
    }

    val.intVal = 0;		/* blender input */
    rc = vlSetControl(video.vlSrv, video.vlPath, video.node_blend, 
					    VL_MGV_BLEND_A_ALPHA_SELECT, &val);
    if (rc != 0) {
	vlPerror("blend A alpha select blender input");
	return -1;
    }

    val.intVal = VL_BLDFCN_MINUS_A_ALPHA;		/* 1 - alpha */
    rc = vlSetControl(video.vlSrv, video.vlPath, video.node_blend, 
					    VL_BLEND_A_FCN, &val);
    if (rc != 0) {
	vlPerror("blend A alpha select blender input");
	return -1;
    }

    val.intVal = VL_BLDFCN_ONE;
    rc = vlSetControl(video.vlSrv, video.vlPath, video.node_blend, 
					    VL_BLEND_B_FCN, &val);
    if (rc != 0) {
	vlPerror("blend A alpha select blender input");
	return -1;
    }

    return 0;
}

