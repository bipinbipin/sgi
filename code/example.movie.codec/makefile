


#/*
#	File:		Makefile
#
#	Contains:	Example Codec Thing Makefile
#
#	Written by:	Mark Krueger
#
#	Copyright:	© 1991 by Apple Computer, Inc., all rights reserved.
#
#    This file is used in these builds: Warhol
#
#	Change History (most recent first):
#
#		 <7>	 12/1/91	MK		use dsg interfaces
#		 <6>	  9/4/91	MK		fix resource id
#		 <5>	 7/19/91	LM		don't use asm
#		 <4>	  5/1/91	MK		add codecinfo resource
#		 <3>	 3/25/91	MK		fix for asm
#		 <2>	 2/26/91	MK		move codecguts.h to interfaces
#		 <1>	 2/25/91	MK		first checked in
#		 <1>	 2/14/91	MK		first checked in
#		 <1>	 2/14/91	MK		first checked in
#		 <1>	 2/14/91	MK		first checked in
#		 <5>	  2/8/91	MK		fix copy to buildresults
#		 <4>	  2/8/91	MK		fix message
#		 <3>	  2/7/91	MK		put objects in separate folder
#		 <2>	  2/7/91	MK		fix build
#		 <1>	  2/7/91	MK		first checked in
#	To Do:
#*/


COptions	 = -warnings off -I {INTERFACES} -mc68020  -o "{warholObjects}"
AOptions	 = -wb -o "{warholObjects}"
Roptions	 = -append -i {interfaces}


interfaces = "{warholSources}Interface:(DSGInterfaces)"
BUILDRES = "{warholSources}BuildResults:"
imagecomp = "{warholSources}Interface:(DSGInterfaces):imagecompression.h"
codecguts = "{warholSources}Interface:(DSGInterfaces):ImageCodec.h"


ExampleCodec	ÄÄ	 makefile ExampleCodec.r {warholSources}Interface:"Image Codec.r" "{warholObjects}"registerthings
	Echo   "# `date -s -t` -- Rezzing ExampleCodec"
	Rez {ROptions} -o ExampleCodec -c 'appl' -t 'thng' ExampleCodec.r


	
ExampleCodec 	ÄÄ	"{warholObjects}"ExampleCodec.c.o ¶
#					"{warholObjects}"ExampleCodec.a.o ¶
					makefile
	Echo   "# `date -s -t` -- Linking ExampleCodec"
	Link -w -m EXAMPLECODEC -rt 'cdec'=128 -ra 16 ¶
		-sg Inst {SYMBOLS}¶
		 "{warholObjects}"ExampleCodec.c.o ¶
#		 "{warholObjects}"ExampleCodec.a.o ¶
		"{Libraries}"Runtime.o ¶
		"{Libraries}"Interface.o ¶
		-o ExampleCodec -c "mark" -t "thng" 


"{warholObjects}"ExampleCodec.c.o Ä 'Makefile' ExampleCodec.c {imagecomp} {codecguts}
	Echo   "# `date -s -t` -- Compiling ExampleCodec"
	 C -b2 {COptions} ExampleCodec.c

"{warholObjects}"ExampleCodec.a.o Ä 'Makefile' ExampleCodec.a {imagecomp} {codecguts}
	Echo   "# `date -s -t` -- Assembling ExampleCodec"
	 Asm {AOptions} ExampleCodec.a

