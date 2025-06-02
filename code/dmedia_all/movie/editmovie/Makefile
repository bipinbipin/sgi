#! smake
# 
# $Revision: 1.7 $

include /usr/include/make/commondefs

EDITMOVIE_OBJECTS = editmovieArgs.o editmovieCopy.o editmovieEdit.o editmovie.o

OBJECTS = $(EDITMOVIE_OBJECTS)

TARGETS = editmovie

CFILES = \
	editmovieArgs.c \
	editmovieCopy.c \
	editmovieEdit.c \
	editmovie.c

OPTIMIZER =-g 
SHDLIBC =
LLDLIBS = -lmoviefile -laudiofile -ldmedia -lil2.5 -lcil2.5 -lmalloc

default all: $(TARGETS)

clean:
	rm -f $(OBJECTS)

clobber:
	rm -f $(TARGETS) $(OBJECTS) core Makedepend

targets: $(TARGETS)

editmovie: $(EDITMOVIE_OBJECTS) $(LIBRARY)
	rm -f $@
	$(CC) -o $@ $(EDITMOVIE_OBJECTS) $(LDFLAGS)
