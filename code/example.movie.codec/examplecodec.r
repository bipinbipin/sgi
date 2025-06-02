/*
	File:		examplecodec.r

	Contains:	xxx put contents here xxx

	Written by:	xxx put writers here xxx

	Copyright:	© 1991 by Apple Computer, Inc., all rights reserved.

    This file is used in these builds: Warhol

	Change History (most recent first):

		 <3>	 12/1/91	MK		clean up
		 <2>	 9/23/91	MK		clean up
		 <1>	 8/26/91	SMC		Initial version.
		 <7>	 8/23/91	SMC		Took out "register thing" stuff.
		 <6>	  7/8/91	MK		fix resources
		 <5>	  5/6/91	MK		fix it
		 <4>	  5/1/91	MK		add codecinfo resource

	To Do:
*/






#include "Types.r"
#include "MPWTypes.r"
#include "ImageCodec.r"


#define	exampleCodecFormatName			"Example - YUV"


#define	exampleCodecFormatType	'exmp'


/*

	This structure defines the capabilities of the codec. There will 
	probably be a tool for creating this resource, which measures the performance
	and capabilities of your codec.
	
*/
resource 'cdci' (128, "Example CodecInfo",locked) {
	exampleCodecFormatName,							/* name of the codec TYPE ( data format ) */
	1,												/* version */							
	1,												/* revision */	
	'appl',											/* who made this codec */
	codecInfoDoes32 + codecInfoDoesSpool,				/* depth and etc. supported directly on decompress */	
	codecInfoDoes32 + codecInfoDoesSpool,				/* depth and etc supported directly on compress */
	codecInfoDepth16,								/* which data formats do we understand */
	100,											/* compress accuracy (0-255) (relative to format) */
	100,											/* decompress accuracy (0-255) (relative to format) */
	200,											/* millisecs to compress 320x240 image on base Mac */
	200,											/* millisecs to decompress 320x240 image on base Mac */
	100,											/* compression level (0-255) (relative to format) */
	0,								
	2,												/* minimum height */
	2,												/* minimum width */
	0,
	0,
	0
};



resource 'thng' (128,  "Example Compressor",locked) {
	compressorComponentType,
	exampleCodecFormatType,
	'appl',
	codecInfoDoes32,
	0,
	'cdec',
	128,
	'STR ',
	128,
	'STR ',
	129,
	'ICON',
	128
};

resource 'thng' (130,  "Example Decompressor",locked) {
	decompressorComponentType,
	exampleCodecFormatType,
	'appl',
	codecInfoDoes32,
	0,
	'cdec',
	128,
	'STR ',
	130,
	'STR ',
	131,
	'ICON',
	130
};


resource 'ICON' (128, purgeable) {
	$"0003 8000 0004 C000 0004 C000 0004 C000"
	$"0004 C000 0004 C000 0004 C000 0004 C000"
	$"0004 C000 0004 C000 0004 C000 0004 C000"
	$"0004 C000 0004 C000 0004 C000 0004 C000"
	$"0004 C000 0004 C000 0004 C000 0007 C000"
	$"0008 6000 0018 7000 007F FC00 00C0 1E00"
	$"0180 1F00 0100 0F00 0100 0F00 0300 0F80"
	$"0200 0F80 03FF FF80 0C00 0FE0 0FFF FFE0"
};

resource 'STR ' (128) {
	"My Example Compressor"
};

resource 'STR ' (129) {
	"Compresses (in an exemplar fasion) an image into a YUVish format."
};


resource 'ICON' (130, purgeable) {
	$"0200 0F80 03FF FF80 0C00 0FE0 0FFF FFE0"
	$"0180 1F00 0100 0F00 0100 0F00 0300 0F80"
	$"0008 6000 0018 7000 007F FC00 00C0 1E00"
	$"0004 C000 0004 C000 0004 C000 0007 C000"
	$"0004 C000 0004 C000 0004 C000 0004 C000"
	$"0004 C000 0004 C000 0004 C000 0004 C000"
	$"0004 C000 0004 C000 0004 C000 0004 C000"
	$"0003 8000 0004 C000 0004 C000 0004 C000"
};

resource 'STR ' (130) {
	"My Example Decompressor"
};

resource 'STR ' (131) {
	"Decompresses an image conmpressed in YUVish format."
};


