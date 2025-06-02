#!make 
#

include	$(ROOT)/usr/include/make/commondefs

DEPTH=../..

LC++INCS = -I$(DEPTH)/libmidifile/include

LC++DEFS = -float
C++FILES = 	recordmidi.c++ playmidi.c++

OBJS	= $(C++FILES:.c++=.o) 

LLDOPTS = -L$(DEPTH)/libmidifile/src
LLDLIBS	= -laudio -laudiofile -lmidifile -lmd

OPTIMIZER = -O2

TARGETS =  playmidi recordmidi

install: default

default: $(TARGETS)

include	$(COMMONRULES)

playmidi: playmidi.o
	$(C++F) -o playmidi playmidi.o $(LDOPTS) $(LDFLAGS) 

recordmidi: recordmidi.o
	$(C++F) -o recordmidi recordmidi.o $(LDOPTS) $(LDFLAGS) 

