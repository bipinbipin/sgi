#!smake
# $Revision: 1.4 $

# comment one of these two lines out. 6.4 use 6.5
# DMEDIA_VERSION = -D DMEDIA_6_3
DMEDIA_VERSION = -D DMEDIA_6_5

COMMONPREF=examples
include $(ROOT)/usr/include/make/commondefs

LLDLIBS = -ldmedia -ldmnet 

LCOPTS = -g3 -I$(ROOT)/usr/include/dmedia -fullwarn $(DMEDIA_VERSION)

TARGETS = nettovid vidtonet disktonet nettotex dmttcp

default: $(TARGETS)

vidtonet: vidtonet.o Makefile
	$(CC) -o vidtonet vidtonet.o $(LDOPTS) $(LLDLIBS) -lvl

disktonet: disktonet.o Makefile
	$(CC) -o disktonet disktonet.o $(LDOPTS) $(LLDLIBS) -lmoviefile

nettovid: nettovid.o Makefile
	$(CC) -o nettovid nettovid.o $(LDOPTS) $(LLDLIBS) -lvl

nettotex: nettotex.o xwindow.o Makefile
	$(CC) -o $@ nettotex.o xwindow.o $(LDOPTS) $(LLDLIBS) -lX11 -lGL

divo_vidtotex: divo_vidtotex.o xwindow.o Makefile
	$(CC) -o $@ divo_vidtotex.o xwindow.o $(LDOPTS) $(LLDLIBS) -lvl -lX11 -lGL

clobber: 
	rm -rf *.o Makedepend $(TARGETS)

include $(COMMONRULES)
