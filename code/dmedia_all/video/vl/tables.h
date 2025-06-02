#ifndef PACKING_TABLE_H
#define PACKING_TABLE_H	/* prevent loading this twice */

typedef struct VMPackingInfoTable {
	int  packing;
	char *packingString;
} VMPackingInfoTable;

/*
 * This table is used to derrive extensions for filenames 
 * so we can read them back into 'memtovid' later.
 */
VMPackingInfoTable packingTable[] = {
	{ VL_PACKING_RGB_332_P, 	"rgb332p" },
        { VL_PACKING_RGBA_8, 		"rgba" },
	{ VL_PACKING_RGB_8,  		"rgb" },
	{ VL_PACKING_RBG_323, 		"rgb323"},
	{ VL_PACKING_VUY_411_SV, 	"yuv411" },
	{ VL_PACKING_YVYU_422_8, 	"yuv422" },
	{ VL_PACKING_Y_8_P, 		"yuvluma" },
	{ VL_PACKING_RGB_332, 		"rgb332" },
	{ VL_PACKING_BGR_332,		"bgr332" },
	{ VL_PACKING_RGB_332_IP, 	"rgb332ip" },
	{ VL_PACKING_BGR_332_P, 	"bgr332p" },
	{ VL_PACKING_BGR_332_IP, 	"bgr332ip" },
	{ VL_PACKING_RGB_565, 		"rgb565" },
	{ VL_PACKING_RGB_565_P, 	"rgb565p" },
	{ VL_PACKING_RGB_565_IP, 	"rgb565ip" },
	{ VL_PACKING_RGB_8_P, 		"rgb8p" },
	{ VL_PACKING_RGB_10, 		"rgb10" },
	{ VL_PACKING_Y_8_IP, 		"yuvlumaip" },
	{ VL_PACKING_YUV_444_8, 	"yuv444" },
	{ VL_PACKING_YUVA_4444_8, 	"yuva4444" },
	{ VL_PACKING_YUV_444_10, 	"yuv444_10" },
	{ VL_PACKING_YUVA_4444_10, 	"yuva4444_10" },
	{ VL_PACKING_ABGR_8, 		"abgr" },
	{ VL_PACKING_AUYV_4444_8, 	"auyv4444" },
	{ VL_PACKING_A_2_BGR_10, 	"a2bgr_10" },
	{ VL_PACKING_A_2_UYV_10, 	"a2uyv_10" },
	{ VL_PACKING_AYU_AYV_10, 	"ayuayv_10" },
	{ VL_PACKING_YVYU_422_10, 	"yvyu422_10" },
	{ VL_PACKING_AUYV_4444_10, 	"auyv4444_10" },
	{ VL_PACKING_RGBA_10, 		"rgba_10" },
	{ VL_PACKING_ABGR_10, 		"abgr_10" },
	{ VL_PACKING_BGR_8_P,		"bgr_8_p" },
	{ VL_PACKING_UYV_8_P,		"uyv_8_p" }
};

#define VMMAX	(sizeof(packingTable)/sizeof(packingTable[0]))

#endif /* PACKING_TABLE_H */
