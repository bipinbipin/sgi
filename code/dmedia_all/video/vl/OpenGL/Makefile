#!/usr/sbin/smake
# $Revision: 1.6 $

include /usr/include/make/commondefs

OBJECT_STYLE=32
LLDLIBS = -lvl -limage
# OPTIMIZER = -g
LD_QUICKSTART_INFO=

TARGETS=contcapt vidtomem mtov showgl composite

default: $(TARGETS)

mtov: mtov.c
	$(CCF) -o $@ mtov.c -lvl -lmoviefile -ldmedia

showgl: showgl.c
	$(CCF) -o $@ showgl.c -lGLw -lGL -lXm -lXt -lX11 -limage

contcapt:
	$(CCF) -o $@ contcapt.c -lvl -lGLw -lGL -lXm -lXt -lX11 
	
composite:
	$(CCF) -o $@ composite.c  -lvl -ldmedia -lGL -lX11 -lm

include $(COMMONRULES)

