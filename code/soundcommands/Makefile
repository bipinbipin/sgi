
OPTIMIZER = -O2 -g3 -USF_DEBUG -UDEBUG -DNDEBUG
CFLAGS = $(OPTIMIZER) \
	 -I \
         -I$(ROOT)/usr/include \
         -float -woff 726,813,824,826,831,835 -fullwarn -wlint,-zaiv 

LDFLAGS = -L -L$(ROOT)/usr/lib

LINKTARGETS = playaifc \
              playaiff \
              recordaifc \
              recordaiff \
              aiff2aifc \
              aifc2aiff \
              aifccompress \
              aifcdecompress \
              $(NULL)

TARGETS = sfplay \
          sfrecord \
          aifcinfo \
          aifcconvert \
          $(LINKTARGETS)

SF_OBJECTS         = sf.o
SFPLAY_OBJECTS     = $(SF_OBJECTS) sfplay.o play.o al.o
SFRECORD_OBJECTS   = $(SF_OBJECTS) sfrecord.o al.o

SF_LLDLIBS = -laudiofile -laudioutil -ldmedia -lm -lmalloc 
SFPLAY_LLDLIBS     = $(SF_LLDLIBS) -laudio
SFRECORD_LLDLIBS   = $(SF_LLDLIBS) -laudio

default all: $(TARGETS)

.c.o:
	cc $(CFLAGS) -c $< -o $@

sfplay: $(SFPLAY_OBJECTS)
	cc $(LDFLAGS) -o $@ $(SFPLAY_OBJECTS) $(SFPLAY_LLDLIBS)

sfrecord: $(SFRECORD_OBJECTS)
	cc $(LDFLAGS) -o $@ $(SFRECORD_OBJECTS) $(SFRECORD_LLDLIBS)


playaifc: sfplay
	ln -s sfplay playaifc

playaiff: sfplay
	ln -s sfplay playaiff

recordaiff: sfrecord
	ln -s sfrecord recordaiff

recordaifc: sfrecord
	ln -s sfrecord recordaifc

aifcinfo: oldaifcinfo.o
	cc $(CFLAGS) -o $@ oldaifcinfo.o -laudiofile 

aifcconvert: oldaifcconvert.o
	cc $(CFLAGS) -o $@ oldaifcconvert.o -laudiofile 

aiff2aifc aifc2aiff aifccompress aifcdecompress: aifcconvert
	ln -s aifconvert $@

clean:
	-rm -f *.o 

clobber: clean
	-rm -f $(TARGETS)
