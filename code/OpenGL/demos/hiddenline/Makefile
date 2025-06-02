#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

TARGETS = hiddenline.32 hiddenline.N32 hiddenline.64

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
WOFF =
OPTIMIZER = -g
#endif

LLDLIBS = -lGLU -lGL -lXext -lX11 -lm

DFILES = \
	README \
        $(NULL)

CFILES = \
	hiddenline.c \
        $(NULL)

default all: $(_FORCE)
	$(MAKE) program;

include $(COMMONRULES)

program: $(_FORCE)
	@if [ ! -d hiddenline.$(OBJECT_STYLE) ]; then \
	    mkdir hiddenline.$(OBJECT_STYLE); \
	fi;
	cd hiddenline.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile links;
	cd hiddenline.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile hiddenline;

links:
	@for i in $(CFILES); do \
	    if [ ! -r $$i ]; then \
	        ln -sf ../$$i $$i; \
	    fi; \
	done;

hiddenline: $(OBJECTS)
	$(CCF) -o $@ $(OBJECTS) $(LDFLAGS);

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
