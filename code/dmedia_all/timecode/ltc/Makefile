#!smake

include $(ROOT)/usr/include/make/commondefs

LCOPTS=-fullwarn
CFILES=readltc.c dmltc.c
TARGETS=readltc dmltc

default: $(TARGETS)

readltc: readltc.o
	$(CCF) -o $@ readltc.o -laudio -laudiofile -lm

dmltc: dmltc.o
	$(CCF) -o $@ dmltc.o -laudio -ldmedia

include $(COMMONRULES)
