#!smake

include /usr/include/make/commondefs

.SUFFIXES: .c .c++ .so

TARGETS =		\
	audio_fadein.so \
	audio_volume.so
    
LLDLIBS = -lvk -lfxplugutils -lm -lC -lc
CFLAGS += -fullwarn
C++FLAGS += -fullwarn

LDFLAGS += -no_unresolved -shared -all

UNRESOLVED_FILTER =
#
# If you want to filter out the known unresolved symbols,
# uncomment this line
#
# UNRESOLVED_FILTER = | ./UnresolvedFilter
#

default: $(TARGETS)

audio_volume.so: audio_volume.c++
	$(C++) $(C++FLAGS) $*.c++ $(LDFLAGS) -o $@ 

audio_fadein.so: audio_fadein.c
	$(CC) $(CFLAGS) $*.c $(LDFLAGS) -o $@

include $(COMMONRULES)


