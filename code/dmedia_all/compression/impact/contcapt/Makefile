#!/usr/sbin/smake
# $Revision: 1.3 $

include /usr/include/make/commondefs
LCINCS=-I$(ROOT)/usr/include/abi

LLDLIBS = -lvl -limage
# OPTIMIZER = -g
LD_QUICKSTART_INFO=

#
# The following two programs are Galileo-specific and depend on the 
# header <dmedia/vl_ev1.h> which is only shipped with the galileo
# product.
# Add them to TARGETS if you have installed this product on your system.
#
#   simpleblend simplewipe
#
TARGETS=contcapt 

default: $(TARGETS)

contcapt:
	$(CCF) -o $@ contcapt.c -lvl -lGLw -lGL -lXm -lXt -lX11 
	

include $(COMMONRULES)

