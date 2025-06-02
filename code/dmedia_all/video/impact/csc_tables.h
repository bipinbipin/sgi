#ifndef PACKING_TABLE_H
#define PACKING_TABLE_H	/* prevent loading this twice */

typedef struct VMPackingInfoTable {
	int  packing;
	char *packingString;
} VMPackingInfoTable;

typedef struct VMFormatInfoTable {
	int  format;
	char *formatString;
} VMFormatInfoTable;

/*
 * This table is used to derrive extensions for filenames 
 * so we can read them back into 'memtovid' later.
 */
VMPackingInfoTable packingTable[] = {
        { VL_PACKING_RGBA_8,		"rgba" },
	{ VL_PACKING_YVYU_422_8,	"yuv422" },
	{ VL_PACKING_YUVA_4444_8,	"yuva4444" },
	{ VL_PACKING_YUVA_4444_10,	"yuva4444_10" },
	{ VL_PACKING_ABGR_8,		"abgr" },
	{ VL_PACKING_AUYV_4444_8,	"auyv4444" },
	{ VL_PACKING_AYU_AYV_10,	"ayu_ayv_10" },
        { VL_PACKING_ABGR_10,		"abgr_10" },
        { VL_PACKING_A_2_BGR_10,	"a2bgr_10" },
        { VL_PACKING_A_2_UYV_10,	"a2uyv_10" },
	{ VL_PACKING_YVYU_422_10,	"yvyu422_10" },
	{ VL_PACKING_AUYV_4444_10,	"auyv4444_10" },
	{ VL_PACKING_RGBA_10,		"rgba_10" },
	{ VL_PACKING_BGR_8_P,		"bgr_8_p" },
	{ VL_PACKING_UYV_8_P,		"uyv_8_p" }
};

#define VMMAX	(sizeof(packingTable)/sizeof(packingTable[0]))

VMFormatInfoTable formatTable[] = {
	{ VL_FORMAT_RGB, "fullrange" },
	{ VL_FORMAT_SMPTE_YUV, "yuvfullrange" },
	{ VL_FORMAT_DIGITAL_COMPONENT_SERIAL, "ccir" },
	{ VL_FORMAT_DIGITAL_COMPONENT_RGB_SERIAL, "rp175" },
};

#define FTMAX (sizeof(formatTable)/sizeof(formatTable[0]))

#endif /* PACKING_TABLE_H */
