#!smake
#
# Copyright (c) Mark J. Kilgard, 1995, 1997.
#

TOP = ..
include $(TOP)/glutdefs
include $(ROOT)/usr/include/make/commondefs

LN = ln -s
MV = mv
RM = -rm -rf

TARGETS = sphere scene movelight glutplane \
	stroke olympic glutdino dinospin \
	mjkwarp highlight subwin origami bitfont lightlab \
	sphere2 dinoball stereo glpuzzle dials blender \
	oclip ohidden olight scube spots \
	stars stenciltst triselect abgr texenv fogtst dials2 \
	surfgrid molehill zoomdino fontdemo splatlogo oversphere \
	fontdemo evaltest sb2db screendoor simple cube reflectdino \
	rendereps dinoshade halomagic trippy mjksift circlefit \
	editgrid

LLDLIBS = $(GLUT) -lGLU -lGL -lXmu -lXi -lXext -lX11 -lm

LCOPTS = -fullwarn
LWOFF = ,813,852,827,826,819
LDIRT = *~ mjkimage.c *.bak *.pure render.eps

default : $(TARGETS)

mjksift : mjksift.o mjkimage.o
	$(RM) $@
	$(CCF) -o $@ mjksift.o mjkimage.o $(LDFLAGS)

mjkwarp : mjkwarp.o mjkimage.o
	$(RM) $@
	$(CCF) -o $@ mjkwarp.o mjkimage.o $(LDFLAGS)

mjkimage.o : mjkimage.c.Z
	$(RM) $@
	zcat mjkimage.c.Z > mjkimage.c
	$(CCF) -c mjkimage.c
	$(RM) mjkimage.c

splatlogo : splatlogo.o logo.o
	$(RM) $@
	$(CCF) -o $@ splatlogo.o logo.o $(LDFLAGS)

logo.o : logo.c.Z
	$(RM) $@
	zcat logo.c.Z > logo.c
	$(CCF) -c logo.c
	$(RM) logo.c

dinospin : dinospin.o trackball.o
	$(RM) $@
	$(CCF) -o $@ dinospin.o trackball.o $(LDFLAGS)

glpuzzle : glpuzzle.o trackball.o
	$(RM) $@
	$(CCF) -o $@ glpuzzle.o trackball.o $(LDFLAGS)

include $(COMMONRULES)
