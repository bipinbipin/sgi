#!smake
# $Id: Makefile.user,v 1.1 1995/03/31 02:02:25 edc Exp $

INCFLAGS = -I../ -I../../vl

GFLAGS = -32 -g -fullwarn -wlint -woff 813,826,852
OFLAGS = 

CFLAGS = $(INCFLAGS) $(GFLAGS) $(OFLAGS)

DSO_LD_FLAGS = \
	-32 \
	-shared \
	-all \
	-no_unresolved \
	-check_registry /usr/lib/so_locations \
	-soname libsv.so \
	-set_version sgi1.0

DSO_LIBS = -lvl -ldmedia -limage -lgl -lc

default: libsv.so

libsv.a: svUtil.o vlUtil.o svImage.o
	ar cruv libsv.a svUtil.o svImage.o vlUtil.o

libsv.so: libsv.a
	   ld $(DSO_LD_FLAGS) libsv.a -o libsv.so $(DSO_LIBS)

clean:
	rm -f *.o *.a *.so *~ core Makedepend

clobber: clean

vlUtil.c: ../../vl/vlUtil/vlUtil.c
	-rm -f $@
	-cp -p $? $@
	-chmod ugo-w $@
	
