#! smake -J 1

TARGETS = snapmovie

snapmovie_FILES = snapmovie.c XLayerUtil.c debug.c

OBJS = ${snapmovie_FILES:.c=.o}

PROFILE = -p
OPTIMIZER = -g
LCINCS = -I$(ROOT)/usr/include -I$(ROOT)/usr/include/il2.5 
LCOPTS = -Xcpluscomm -fullwarn -xansi

LLDLIBS = -L$(ROOT)/usr/lib -lSgm -lX11 -lXm -lXt -lXext -lmoviefile -ldmedia -lil2.5

include $(ROOT)/usr/include/make/commondefs
CC	= $(TOOLROOT)/usr/bin/CC

default all: $(TARGETS)

snapmovie: $(OBJS)
	$(CC) $(OBJS) $(LCFLAGS) $(LDFLAGS) -o $@

include $(COMMONRULES)
