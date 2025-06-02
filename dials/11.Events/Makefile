#!smake

PROGRAM = eventTest

C++FILES = ButtonBoxEvent.c++ DialEvent.c++ \
	DialNButton.c++ eventTest.c++

HFILES = ButtonBoxEvent.h DialEvent.h DialNButton.h

# /usr/include/make/commondefs and /usr/include/make/commonrules
# define some useful Makefile rules.  For example, they
# defines a 'clean' target, so you can type 'make clean'
# to remove all .o files (or other files generated during
# compilation). See the file /usr/include/make/commonrules for
# documentation on what targets are supported.

include /usr/include/make/commondefs

TARGETS = $(PROGRAM)

# Libraries to link with:

LLDLIBS =  -lInventorXt -lInventor -lXi -lXt

# Use compiler flag IV_STRICT to catch things in the code that are not
# recommended for use with Inventor 2.1
LC++DEFS = -DIV_STRICT -LANG:std
LC++INCS = -I/usr/include/Inventor/Input -I/usr/include/CC -I/usr/include
C++FLAGS = $(LC++DEFS) $(LC++INCS) -g -n32 -mips3

default: $(TARGETS)

include $(COMMONRULES)

$(TARGETS): $(OBJECTS)
	$(C++F) -o $@ $(OBJECTS) $(LDFLAGS)

