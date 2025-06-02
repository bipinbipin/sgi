#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

TARGETS = built.32 built.N32 built.64

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
WOFF =
OPTIMIZER = -g
#endif

LLDLIBS = -lGLU -lGL -lXext -lX11 -lm

DFILES = \
	README \
        $(NULL)

PROGFILES = \
	detail.c \
	sharpen.c \
	tex3d.c \
	texobj.c \
        $(NULL)

default all: $(_FORCE)
	$(MAKE) program;

include $(COMMONRULES)

program: $(_FORCE)
	@if [ ! -d built.$(OBJECT_STYLE) ]; then \
	    mkdir built.$(OBJECT_STYLE); \
	fi;
	cd built.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile links;
	cd built.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile built;

links:
	@for i in $(PROGFILES); do \
	    if [ ! -r $$i ]; then \
	        ln -sf ../$$i $$i; \
	    fi; \
	done;

built:
	@for i in $(PROGFILES:.c=); do \
	    OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile $$i; \
	done;

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
