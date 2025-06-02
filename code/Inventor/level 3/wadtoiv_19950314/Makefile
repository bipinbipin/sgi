include $(ROOT)/usr/include/make/commondefs

MOTIFLIBS = -lSgm -lXm -lXt -lX11
INVXTLIBS = -lInventorXt
INVLIBS = -lGL -lInventor -limage
LLDLIBS=  $(INVLIBS) -lm -lC
LC++FLAGS= -g

TARGETS=wadtoiv

OBJECTS= main.o wfuncs.o

default all: $(TARGETS)

targets: $(TARGETS)

wadtoiv: main.o wfuncs.o
	$(C++) -o $@ main.o wfuncs.o $(LLDLIBS)

include $(COMMONRULES)
# DO NOT DELETE THIS LINE -- make depend depends on it.
