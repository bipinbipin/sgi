#! smake
# 
# $Revision: 1.2 $

include /usr/include/make/commondefs

MANYMOVIE_OBJECTS = manymovieArgs.o manymovie.o

MANYMOVIE_LDOBJECTS = ../manymovie/manymovieArgs.o ../manymovie/manymovie.o

MANYMOVIE_GL_OBJECTS = manymovieGLEvents.o manymovieGLWin.o

OBJECTS = $(MANYMOVIE_GL_OBJECTS)

TARGETS = manymovieGL

CFILES = \
	manymovieGLWin.c \
	manymovieGLEvents.c

OPTIMIZER =-g 
SHDLIBC =
LCINCS = -I../manymovie
LLDLIBS = -lmovieplay -lmoviefile -ldmedia -lX11 -lGL

default all: $(TARGETS)

clean:
	rm -f $(OBJECTS)

clobber:
	rm -f $(TARGETS) $(OBJECTS) core Makedepend

targets: $(TARGETS)

manymovieGL: $(MANYMOVIE_GL_OBJECTS) ;cd ../manymovie ;make $(MANYMOVIE_OBJECTS) ;cd ../manymovieGL
	rm -f $@
	$(CC) -o $@ $(MANYMOVIE_GL_OBJECTS) $(MANYMOVIE_LDOBJECTS) $(LDFLAGS)
