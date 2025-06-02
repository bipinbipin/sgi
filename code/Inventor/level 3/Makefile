#!smake

PROGRAMS = spaceball spballViewer

# /usr/include/make/commondefs and /usr/include/make/commonrules
# define some useful Makefile rules.  For example, they
# defines a 'clean' target, so you can type 'make clean'
# to remove all .o files (or other files generated during
# compilation). See the file /usr/include/make/commonrules for
# documentation on what targets are supported.

include /usr/include/make/commondefs

TARGETS = $(PROGRAMS)

LLDLIBS =-lInventorXt -lInventor
C++FILES = $(PROGRAMS:=.c++)

# Use compiler flag IV_STRICT to catch things in the code that are not
# recommended for use with Inventor 2.1
LC++DEFS = -DIV_STRICT

default: $(PROGRAMS)

include $(COMMONRULES)

$(PROGRAMS): $$@.c++
	$(C++F) -o $@ $@.c++ $(LDFLAGS)

