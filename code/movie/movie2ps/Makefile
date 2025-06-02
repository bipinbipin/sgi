#!smake
#
# This is the makefile for the movie2ps command.
#
# This makefile uses the SGI common definitions and rules include files
# which provide a set of useful macros and rules.  Makefiles that use these
# includes have a common set of targets that simplify different types of
# builds.  For example:
#
# To make mvfilter with -g debugging enabled:
#
# make OPTIMIZER=-g
#
# To make with a debugging ifdef turned on:
#
# make LCDEFS=-DDEBUG
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

C++FILES = \
	movie2ps.c++

LLDLIBS = \
	-lmoviefile		\
	-ldmedia 		\
	$(NULL)

TARGETS = movie2ps

default all : $(TARGETS)

$(TARGETS) : $(OBJECTS)
	$(C++F) -o $@ $(OBJECTS) $(LDFLAGS)

include $(COMMONRULES)
