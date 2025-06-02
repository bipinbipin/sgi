#!smake

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

TARGETS = toogl

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
OPTIMIZER = -g
#endif

NEWNOINLINE=-INLINE:none

#if $(OBJECT_STYLE) == "32"
CFRONTOPTS=-pte.c++
LIBDIRSUFF=
NOINLINE=+d
#elif $(OBJECT_STYLE) == "64"
CFRONTOPTS=
LIBDIRSUFF=64
NOINLINE=$(NEWNOINLINE)
#else
CFRONTOPTS=
LIBDIRSUFF=32
NOINLINE=$(NEWNOINLINE)
#endif

HFILES = \
	perlassoc.h \
	perlclass.h \
	regex.h \
	regexp.h \
	regmagic.h \
	search.h \
	$(NULL)

C++FILES = \
	perlclass.c++ \
	toogl.c++ \
	search.c++ \
	$(NULL)

CFILES = \
        irisgl_light.c \
	regex.c \
	$(NULL)

SRC_FILES = \
	README \
	Makefile \
	Makefile.lib \
	$(HFILES) \
	$(C++FILES) \
	$(CFILES) \
	$(NULL)
 
default: $(TARGETS)

include $(COMMONRULES)

toogl: $(OBJECTS)
	$(C++F) $(CFRONTOPTS) -L$(ROOT)/usr/lib$(LIBDIRSUFF) \
		-lGL -o toogl $(OBJECTS);

toogl.o: toogl.c++
	$(C++F) -c $(NOINLINE) toogl.c++;

clobber: $(COMMONPREF)$$@
	rm -rf ii_files;

headers exports: 

install ism_install: 
	$(INSTALL) -F /usr/share/src/OpenGL/tools/toogl -idb "TOOGL" $(SRC_FILES);

$(COMMONTARGS): $(COMMONPREF)$$@

