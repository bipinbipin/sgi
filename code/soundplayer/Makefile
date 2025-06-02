
include $(ROOT)/usr/include/make/commondefs

OPTIMIZER = -O2 -g3 -USF_DEBUG -UDEBUG -DNDEBUG

GUILIBS = -lTransportButton -lvk -lXpm -lSgm -lXm -lXt -lX11 -lhelpmsg
LLDLIBS = $(GUILIBS) \
	  -laudio -laudiofile \
	  -lmidifile \
          -ldmedia \
	  -lm -lgen

LLDOPTS   = -L../libTransportButton  -L../libmidifile/src
LCINCS    = -I../libmidifile/include -I../libTransportButton
LC++INCS  = -I../libmidifile/include -I../libTransportButton

OBJECTS = soundplayer.o

TARGETS = soundplayer

default all: $(TARGETS)

soundplayer: $(OBJECTS)
	$(C++F) -o $@ $(OBJECTS) $(LDFLAGS)

include $(COMMONRULES)
