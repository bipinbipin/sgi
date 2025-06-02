#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

TARGETS = drip.32 drip.N32 drip.64

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
WOFF =
OPTIMIZER = -g
#endif

LLDLIBS = -lGLw -lGLU -lGL -lXm -lXt -lXext -lX11 -lm

HFILES = \
	Drip.h \
	oglwindow.h \
	$(NULL)

C++FILES = \
	Drip.c++ \
	main.c++ \
	oglwindow.c++ \
	$(NULL)

default all: $(_FORCE)
	$(MAKE) program;

include $(COMMONRULES)

program: $(_FORCE)
	@if [ ! -d drip.$(OBJECT_STYLE) ]; then \
	    mkdir drip.$(OBJECT_STYLE); \
	fi;
	cd drip.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile links;
	cd drip.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile drip;

links:
	@for i in $(HFILES) $(C++FILES); do \
	    if [ ! -r $$i ]; then \
	        ln -sf ../$$i $$i; \
	    fi; \
	done;

drip: $(OBJECTS)
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
