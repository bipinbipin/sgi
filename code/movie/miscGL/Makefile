#! smake
# 
# $Revision: 1.5 $

include /usr/include/make/commondefs

OBJECTS = simplemovie.o

TARGETS = simplemovieGL

CFILES = \
	simplemovieGL.c

OPTIMIZER =-g 
SHDLIBC =
LCINCS = -I../common
LLDLIBS= -lmovieplay -lmoviefile -ldmedia -lX11 -lGL

default all: $(TARGETS)

clean:
	rm -f $(OBJECTS)

clobber:
	rm -f $(TARGETS) $(OBJECTS) core Makedepend

targets: $(TARGETS)

simplemovieGL: simplemovieGL.o
	rm -f $@
	$(CC) -o $@ simplemovieGL.o $(LDFLAGS)
