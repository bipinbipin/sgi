#
# This is the makefile for the dmplay command.
#
# This makefile uses the SGI common definitions and rules include files
# which provide a set of useful macros and rules.  Makefiles that use these
# includes have a common set of targets that simplify different types of
# builds.  For example:
#
# To make dmplay with -g debugging enabled:
#
# make OPTIMIZER=-g
#
# To make with a debugging ifdef turned on:
#
# make VCDEFS=-DDEBUG
#
# To remove all compiled files and executables:
#
# make clobber
#
# To remove just the intermediate build files (.o), but leave the final
# command:
#
# make clean
#
# To leave the intermediate build files, but remove the final target:
#
# make rmtargets
#
# This makefile will automatically generate a file ``Makedepend'' which
# will be included here.  The makedepend file contains all of the header
# file dependencies that the built objects have.

include $(ROOT)/usr/include/make/commondefs

CFILES = dmplay.c \
	vlInit.c \
	clInit.c \
	alInit.c \
	mvInit.c \
	Xinit.c \
	singleFrameDecompress.c \
	streamDecompressEv1.c \
	streamDecompressImpact.c \
	parseargs.c \
	$(NULL)

HFILES = dmplay.h
LLDLIBS=-lSgm -lXm -lXt -lmoviefile -lcl -ldmedia -lgl -laudio -lm -lvl -lX11 -lgen

TARGETS =  dmplay

default all: $(TARGETS)

dmplay: $(OBJECTS)
	$(CCF) -o $@ $(OBJECTS) $(LLDLIBS)

include $(COMMONRULES)
