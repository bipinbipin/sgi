#!smake


TARGETS = addaudio

addaudio_FILES = addaudio.c
OBJS = ${addaudio_FILES:.c=.o}

OPTIMZER = -O
PROFILE = -p
LCOPTS = -Xcpluscomm -fullwarn -xansi
LCINCS = -I$(ROOT)/usr/include
LWOFF=,838,649

LLDLIBS = -lmoviefile -laudiofile -ldmedia

include $(ROOT)/usr/include/make/commondefs

default all: $(TARGETS)

addaudio: $(OBJS)
	$(CC) $(OBJS) $(LCFLAGS) $(LDFLAGS) -o $@

include $(COMMONRULES)
