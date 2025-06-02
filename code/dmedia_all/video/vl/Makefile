#!/usr/sbin/smake
# $Revision: 1.27 $

include /usr/include/make/commondefs

OBJECT_STYLE=32
LLDLIBS = -lvl -lgl -ldmedia
#LLDLIBS = -lvl -lX11 -limage -lgl -ldmedia
# OPTIMIZER = -g
LD_QUICKSTART_INFO=

#
# The following two programs are Galileo-specific and depend on the 
# header <dmedia/vl_ev1.h> which is only shipped with the galileo
# product.
# Add them to TARGETS if you have installed this product on your system.
#
#   simpleblend simplewipe simplev2v
#
TARGETS=contcapt eventex simpleccapt colorcapture simplegrab \
	simplem2v simplev2s simplev2v \
	videoin vidtomem mtov colorposterize

default: $(TARGETS)

mtov: mtov.o
	$(CCF) -o $@ mtov.o -lvl -lmoviefile -ldmedia

videoin: videoin.o
	$(CCF) -o $@ videoin.o -lvl -ldmedia -lGL -lm -lXext -lX11
vidtomem: vidtomem.o
	$(CCF) -o $@ vidtomem.o -lvl -limage  -ldmedia
simplev2s: simplev2s.o
	$(CCF) -o $@ simplev2s.o -lvl  -lX11 -ldmedia
simplev2v: simplev2v.o
	$(CCF) -o $@ simplev2v.o -lvl   -ldmedia
simplem2v: simplem2v.o
	$(CCF) -o $@ simplem2v.o -lvl   -ldmedia
simpleblend: simpleblend.o
	$(CCF) -o $@ simpleblend.o -lvl -lX11 -ldmedia
simplewipe: simplewipe.o
	$(CCF) -o $@ simplewipe.o -lvl -lX11 -ldmedia

include $(COMMONRULES)

