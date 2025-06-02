#!smake
# $Id: Makefile,v 1.10 1995/03/31 01:47:23 edc Exp $

TARGETS = svtest1 svtest3 svtest4 svtest5 \
	  svtest6 svtest7 svtest8 svtest9 svtest10

INCFLAGS = -I../ -I../.. -I../../vl -I$(ROOT)/usr/include

GFLAGS = -32 -g -fullwarn -wlint -woff 813,826,852
OFLAGS = 

CFLAGS = $(INCFLAGS) $(GFLAGS) $(OFLAGS)

LIBS = ../lib/libsv.a -L$(ROOT)/usr/lib -lvl -lgl -ldmedia -limage

# If you want to use libsv as a shared library, you can use the following
# link line, but you will have to remember to install the libsv.so somewhere
# where the dynamic linker can find it or do a 
#
#     setenv LD_LIBRARY_PATH "../lib"
#
# LIBS = -L../lib -lsv -lvl


default: $(TARGETS)

$(TARGETS): $@.o $(SVLIB)
	cc -32 -o $@ $@.o $(LIBS)

clean:
	rm -rf *.o *.a *~ core vidtomem $(TARGETS) test00* *.rgb *.yuv

clobber: clean
