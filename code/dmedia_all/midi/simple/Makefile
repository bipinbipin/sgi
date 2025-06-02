#!make 
#

include	$(ROOT)/usr/include/make/commondefs

LLDLIBS	= -lmd -ldmedia

OPTIMIZER = -O2

default: scale thru simple_scale panic

include	$(COMMONRULES)

thru: thru.o
	$(CCF) $@.o -o $@ $(LDFLAGS)

scale: scale.o
	$(CCF) $@.o -o $@ $(LDFLAGS)

simple_scale: simple_scale.o 
	$(CCF) $@.o -o $@ $(LDFLAGS)

panic: panic.o
	$(CCF) $@.o -o $@ $(LDFLAGS)
