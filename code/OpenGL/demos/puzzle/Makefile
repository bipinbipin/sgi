#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

TARGETS = puzzle.32 puzzle.N32 puzzle.64

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
WOFF =
OPTIMIZER = -g
#endif

TOOLKITDIR ?= /usr/share/src/OpenGL/toolkits

LCINCS = -I$(ROOT)$(TOOLKITDIR)/include
#if $(OBJECT_STYLE) == "64"
LLDLIBS = -L$(ROOT)$(TOOLKITDIR)/lib64
#elif $(OBJECT_STYLE) == "N32"
LLDLIBS = -L$(ROOT)$(TOOLKITDIR)/lib32
#else
LLDLIBS = -L$(ROOT)$(TOOLKITDIR)/lib
#endif
LLDLIBS += -lglut -lGLU -lGL -lXmu -lXext -lX11 -lm

HFILES = \
	trackball.h \
        $(NULL)

CFILES = \
	puzzle.c \
	trackball.c \
        $(NULL)

default all: $(_FORCE)
	$(MAKE) program;

include $(COMMONRULES)

program: $(_FORCE)
	@if [ ! -d puzzle.$(OBJECT_STYLE) ]; then \
	    mkdir puzzle.$(OBJECT_STYLE); \
	fi;
	cd puzzle.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile links;
	cd puzzle.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile puzzle;

links:
	@for i in $(HFILES) $(CFILES); do \
	    if [ ! -r $$i ]; then \
	        ln -sf ../$$i $$i; \
	    fi; \
	done;

puzzle: $(OBJECTS)
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
