#!/usr/sbin/smake
# $Revision: 1.10 $

include /usr/include/make/commondefs

LLDLIBS = -lvl -lcl -laudio -ldmedia -lX11 -limage -lgl -lGL
# OPTIMIZER = -g
LD_QUICKSTART_INFO=

#
# Example Impact Programs.
#
#   simpleblend simplewipe contcapt fb impact_alpha
#
TARGETS= \
	contcapt \
	simpleblend \
	simplewipe \
	fb \
	dsktovid \
	dsktovid_aio \
	vidtodsk \
	vidtodsk_aio \
	impact_alpha \
	dsktoav \
	avtodsk \
	memtocsctovid \
	vidtocsctomem \
	vidtotex \
	scrntodsk

default: $(TARGETS)

include $(COMMONRULES)
