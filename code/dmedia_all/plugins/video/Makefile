#!smake

include /usr/include/make/commondefs

.SUFFIXES: .c++ .so

TARGETS = 			\
	beamMeUp.so		\
	CrossDissolve.so	\
	fade.so			\
	NoopConvolveFilter.so   \
	NoopDrawPixelsFilter.so \
	NoopTextureFilter.so

LLDLIBS = -lfxplugutils -lC

C++FLAGS += -fullwarn

LDFLAGS += -no_unresolved -shared -all


UNRESOLVED_FILTER =
#
# If you want to filter out the known unresolved symbols,
# uncomment this line
#
# UNRESOLVED_FILTER = | ./UnresolvedFilter
#

#
# default target
#

default: $(TARGETS)

#
# Video Transitions
#

beamMeUp.so: beamMeUp.c++
	$(C++) $(C++FLAGS) $*.c++ $(LDFLAGS) -o $@ 	$(UNRESOLVED_FILTER)

CrossDissolve.so: CrossDissolve.c++
	$(C++) $(C++FLAGS) $*.c++ $(LDFLAGS) -lGL -o $@ $(UNRESOLVED_FILTER)

#
# Video Filters
#

fade.so: fade.c++
	$(C++) $(C++FLAGS) $*.c++ $(LDFLAGS) -lvk -o $@ $(UNRESOLVED_FILTER)

NoopDrawPixelsFilter.so: NoopDrawPixelsFilter.c++
	$(C++) $(C++FLAGS) $*.c++ $(LDFLAGS) -lGL -o $@ $(UNRESOLVED_FILTER)

NoopConvolveFilter.so: NoopConvolveFilter.c++
	$(C++) $(C++FLAGS) $*.c++ $(LDFLAGS) -lGL -o $@ $(UNRESOLVED_FILTER)

NoopTextureFilter.so: NoopTextureFilter.c++
	$(C++) $(C++FLAGS) $*.c++ $(LDFLAGS) -lGL -o $@ $(UNRESOLVED_FILTER)

include $(COMMONRULES)


