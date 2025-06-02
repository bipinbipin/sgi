#!smake

#
# Makefile for the MIDI player
#
# "$Revision: 1.2 $"
#
include $(ROOT)/usr/include/make/commondefs

C++FILES= 	MidiPlayer.c++		\
		MidiFile.c++		\
		MidiTrack.c++		\
		TempoMap.c++		\
		mfutils.c++		\
		preroll.c++		\

LC++INCS = -I -I../include

LIBRARY=	../libmidiplayer.a
TARGETS=	$(LIBRARY)

default all $(COMMONTAGS): $(TARGETS)

include $(COMMONRULES)

$(TARGETS): $(OBJECTS)
	$(AR) cruv $@ $?
