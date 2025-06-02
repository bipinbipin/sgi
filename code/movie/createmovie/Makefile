#! smake
# 
# $Revision: 1.11 $

include /usr/include/make/commondefs

CREATEMOVIE_OBJECTS = createmovieArgs.o createmovieInit.o createmovieResize.o createmovieFiles.o createmovie.o

OBJECTS = $(CREATEMOVIE_OBJECTS)

TARGETS = createmovie

C++FILES = \
	createmovieArgs.c++ \
	createmovieInit.c++ \
	createmovieFiles.c++ \
	createmovieResize.c++ \
	createmovie.c++

OPTIMIZER =-g 
SHDLIBC =
LLDLIBS = -lmoviefile -laudiofile -ldmedia -lil2.5 -lmalloc

default all: $(TARGETS)

clean:
	rm -f $(OBJECTS)

clobber:
	rm -f $(TARGETS) $(OBJECTS) core Makedepend

targets: $(TARGETS)

createmovie: $(CREATEMOVIE_OBJECTS) $(LIBRARY)
	rm -f $@
	$(C++F) -o $@ $(CREATEMOVIE_OBJECTS) $(LDFLAGS) $(C++FILT)
