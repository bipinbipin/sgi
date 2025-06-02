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

TOOLKITDIR ?= /usr/share/src/OpenGL/toolkits

LCINCS = -I$(ROOT)$(TOOLKITDIR)/include
#if $(OBJECT_STYLE) == "64"
LLDLIBS = -L$(ROOT)$(TOOLKITDIR)/lib64
#elif $(OBJECT_STYLE) == "N32"
LLDLIBS = -L$(ROOT)$(TOOLKITDIR)/lib32
#else
LLDLIBS = -L$(ROOT)$(TOOLKITDIR)/lib
#endif
LLDLIBS += -lrgb -lfont -lglut -lGLU -lGL -lXmu -lXext -lX11 -lm

DFILES = \
	1.rgb \
	2.rgb \
	3.rgb \
	$(NULL)

PROGFILES = \
	abgr.c \
	accum.c \
	bitmap1.c \
	bitmap2.c \
	blendeq.c \
	blendxor.c \
	copy.c \
	copytex.c \
	depth.c \
	eval.c \
	fog.c \
	line.c \
	logo.c \
	nurb.c \
	packedpix.c \
	point.c \
	prim.c \
	quad.c \
	select.c \
	shell.c \
	sphere.c \
	star.c \
	stencil.c \
	stretch.c \
	surfgrid.c \
	teapot.c \
	texenv.c \
	texobj.c \
	texproxy.c \
	texsubimage.c \
	texture.c \
	tri.c \
	twotextures.c \
	varray.c \
	wave.c \
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
	@for i in $(PROGFILES) $(DFILES); do \
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
