#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

TARGETS = fadeflip.32 fadeflip.N32 fadeflip.64

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
WOFF =
OPTIMIZER = -g
#endif

LC++DEFS = -DDATADIR=\"..\/..\/data\/\" -DTEXTURE
LLDLIBS = -lGLw -lGLU -lGL -limage -lXext -lX11 -lm

HFILES = \
	myimage.h \
	texture.h \
	unitsquare.h \
	oglwindow.h \
	$(NULL)

C++FILES = \
	fadeflip.c++ \
	oglwindow.c++ \
	texture.c++ \
	unitsquare.c++ \
	$(NULL)

default all: $(_FORCE)
	$(MAKE) program;

include $(COMMONRULES)

program: $(_FORCE)
	@if [ ! -d fadeflip.$(OBJECT_STYLE) ]; then \
	    mkdir fadeflip.$(OBJECT_STYLE); \
	fi;
	cd fadeflip.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile links;
	cd fadeflip.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile fadeflip;

links:
	@for i in $(HFILES) $(C++FILES); do \
	    if [ ! -r $$i ]; then \
	        ln -sf ../$$i $$i; \
	    fi; \
	done;

fadeflip: $(OBJECTS)
	$(C++F) -o $@ $(OBJECTS) $(LDFLAGS);

clean: $(COMMONPREF)$$@
	for i in $(TARGETS); do \
	    if [ -d $$i ]; then \
		( cd $$i; $(MAKE) -f ../Makefile $(COMMONPREF)clean ); \
	    fi; \
	done;

clobber: $(COMMONPREF)$$@
	for i in $(TARGETS); do \
	    if [ -d $$i ]; then \
		rm -rf $$i; \
	    fi; \
	done;

headers exports install: default

$(COMMONTARGS): $(COMMONPREF)$$@
