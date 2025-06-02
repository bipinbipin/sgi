#!smake
#
# Makefile for libmidifile
#

include $(ROOT)/usr/include/make/commondefs

LC++OPTS= -g
LC++INCS= -I../include

C++FILES= 	MFfile.c++		\
		MFtrack.c++		\
		MFfileEvent.c++ 	\
		libmidifileutils.c++	\
		$(NULL)


TARGETS = libmidifile.a

OBJECTS = MFfile.o MFtrack.o MFfileEvent.o libmidifileutils.o

default all: $(TARGETS)

.c.o:
	cc $(CFLAGS) -c $< -o $@

.c++.o:
	CC $(C++FLAGS) -c $< -o $@


$(TARGETS): $(OBJECTS)
	$(AR) cruv $@ $?

include $(COMMONRULES)
