#! smake
# 
# $Revision: 1.5 $

include /usr/include/make/commondefs

MANYMOVIE_OBJECTS = manymovieArgs.o manymovieEvents.o manymovieWin.o manymovie.o

OBJECTS = $(MANYMOVIE_OBJECTS)

TARGETS = manymovie

CFILES = \
	manymovieArgs.c \
	manymovieWin.c \
	manymovieEvents.c \
	manymovie.c

OPTIMIZER =-g 
SHDLIBC =
LCINCS = -I../common
LLDLIBS = -lmovieplay -lmoviefile -ldmedia -lX11 -lgl -lm


default all: $(TARGETS)

clean:
	rm -f $(OBJECTS)

clobber:
	rm -f $(TARGETS) $(OBJECTS) core Makedepend

targets: $(TARGETS)

manymovie: $(MANYMOVIE_OBJECTS) ;cd ../common ;make ;cd ../manymovie
	rm -f $@
	$(CC) -o $@ ../common/glxhelper.o $(MANYMOVIE_OBJECTS) $(LDFLAGS)
