#! smake
# 
# $Revision: 1.7 $

include /usr/include/make/commondefs

OBJECTS = mvinfo.o simplemovie.o aud-to-movie.o img-to-movie.o mvPlayTimeUtils.o mvtimeframe.o

TARGETS = mvinfo simplemovie mvtimeframe aud-to-movie img-to-movie

CFILES = \
	simplemovie.c \
	mvinfo.c \
	aud-to-movie.c \
	mvPlayTimeUtils.c \
	mvtimeframe.c

C++FILES = \
	img-to-movie.c++

OPTIMIZER =-g 
SHDLIBC =
LCINCS = -I../common

MVINFO_LIBS             = -lmoviefile -ldmedia
SIMPLEMOVIE_LIBS        = -lmovieplay -lmoviefile -ldmedia -lX11 -lgl
MVTIMEFRAME_LIBS        = -lmovieplay -lmoviefile -ldmedia
IMG-TO-MOVIE_LIBS       = -lmoviefile -ldmedia -lil2.5
AUD-TO-MOVIE_LIBS       = -lmoviefile -laudiofile -ldmedia

default all: $(TARGETS)

clean:
	rm -f $(OBJECTS)

clobber:
	rm -f $(TARGETS) $(OBJECTS) core Makedepend

targets: $(TARGETS)

mvinfo: mvinfo.o $(LIBRARY)
	rm -f $@
	$(CC) -o $@ mvinfo.o $(LDFLAGS) $(MVINFO_LIBS)

simplemovie: simplemovie.o ;cd ../common ;make ;cd ../misc
	rm -f $@
	$(CC) -o $@ ../common/glxhelper.o simplemovie.o \
		$(LDFLAGS) $(SIMPLEMOVIE_LIBS)

mvtimeframe: mvPlayTimeUtils.o mvtimeframe.o
	rm -f $@
	$(CC) -o $@ mvPlayTimeUtils.o mvtimeframe.o \
		$(LDFLAGS) $(MVTIMEFRAME_LIBS)

img-to-movie : img-to-movie.o
	rm -f $@
	$(C++F) -o $@ $@.o $(LDFLAGS) $(IMG-TO-MOVIE_LIBS) $(C++FILT)

aud-to-movie : aud-to-movie.o
	rm -f $@
	$(CC) -o $@ $@.o $(LDFLAGS) $(AUD-TO-MOVIE_LIBS)
