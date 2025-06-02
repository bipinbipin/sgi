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
LLDLIBS += -laux -lGLU -lGL -lXext -lX11 -lm

DFILES = \
	README \
	$(NULL)

HFILES = \
        jitter.h \
        $(NULL)

PROGFILES = \
        accanti.c \
        accnot.c \
        accpersp.c \
        accum.c \
        aim.c \
        alpha.c \
        alpha3D.c \
        anti.c \
        antiindex.c \
        antipindex.c \
        antipoint.c \
        antipoly.c \
        bezcurve.c \
        bezmesh.c \
        bezsurf.c \
        checker.c \
        checker2.c \
        chess.c \
        clip.c \
        colormat.c \
        cone.c \
        cube.c \
        curve.c \
        depthcue.c \
        disk.c \
        dof.c \
        dofnot.c \
        double.c \
        drawf.c \
        feedback.c \
        fog.c \
        fogindex.c \
        font.c \
        light.c \
        linelist.c \
        lines.c \
        list.c \
        list2.c \
        maplight.c \
        material.c \
        mipmap.c \
        model.c \
        movelight.c \
        nurbs.c \
        oss.c \
        pickdepth.c \
        pickline.c \
        picksquare.c \
        plane.c \
        planet.c \
        planetup.c \
        polys.c \
        robot.c \
        sccolorlight.c \
        scene.c \
        scenebamb.c \
        sceneflat.c \
        select.c \
        simple.c \
        smooth.c \
        sphere.c \
        stencil.c \
        stroke.c \
        surface.c \
        tea.c \
        teaambient.c \
        teapots.c \
        texgen.c \
        texturesurf.c \
        trim.c \
        xfont.c \
	xfont_japan.c \
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
	@for i in $(HFILES) $(PROGFILES); do \
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
