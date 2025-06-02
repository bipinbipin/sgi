#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

## Watch out, this may need to change. We do not support Motif 2.1 in O32
## ABI.
TARGETS = libGLw.32 libGLw.N32 libGLw.64

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
WOFF =
OPTIMIZER = -g
#endif

TOOLKITDIR ?= /usr/share/src/OpenGL/toolkits

LCINCS = -I$(ROOT)$(TOOLKITDIR)/include

DFILES = \
	README \
	$(NULL)

HFILES = \
	GLwDrawA.h \
	GLwDrawAP.h \
	GLwMDrawA.h \
	GLwMDrawAP.h \
	$(NULL)

CFILES = \
	GLwDrawA.c \
	GLwMDrawA.c \
	GLwM21DrawA.c \
	GLwDAUtil.c \
	GLwDA21Util.c \
	$(NULL)

default: $(_FORCE)
	$(MAKE) library;

include $(COMMONRULES)

library:
	@if [ ! -d libGLw.$(OBJECT_STYLE) ]; then \
	    mkdir libGLw.$(OBJECT_STYLE); \
	fi; \
	cd libGLw.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile links;
	cd libGLw.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile libGLw.a;

GLwMDrawA.o: GLwMDrawA.c
	$(CC) $(VCFLAGS) $(LCDEFS) -I$(ROOT)/usr/Motif-1.2/include $(LCINCS) $(LCOPTS) $(GCFLAGS) -c $<

GLwM21DrawA.o: GLwM21DrawA.c
	$(CC) $(VCFLAGS) $(LCDEFS) -I$(ROOT)/usr/Motif-2.1/include $(LCINCS) $(LCOPTS) $(GCFLAGS) -c $<

GLwDA21Util.o: GLwDA21Util.c
	$(CC) $(VCFLAGS) $(LCDEFS) -I$(ROOT)/usr/Motif-2.1/include $(LCINCS) $(LCOPTS) $(GCFLAGS) -c $<

GLwDAUtil.o: GLwDAUtil.c
	$(CC) $(VCFLAGS) $(LCDEFS) -I$(ROOT)/usr/Motif-1.2/include $(LCINCS) $(LCOPTS) $(GCFLAGS) -c $<

links:
	@for i in $(HFILES) $(CFILES); do \
	    if [ ! -r $$i ]; then \
		ln -s ../$$i $$i; \
	    fi; \
	done;

libGLw.a: $(OBJECTS)
	$(AR) crl $@ $(OBJECTS);

clean: $(COMMONPREF)$$@
	for i in $(TARGETS); do \
	    if [ -d $$i ]; then \
		( cd $$i; $(MAKE) -f ../Makefile $(COMMONPREF)clean ); \
	    fi; \
	done;

clobber: $(COMMONPREF)$$@
	for i in $(TARGETS); do \
	    if [ -d $$i ]; then \
		rm -rf $$i; \
	    fi; \
	done;

headers:
	$(INSTALL) -F $(TOOLKITDIR)/include/GL -idb "LIBWIDGET" $(HFILES);

install: headers default
#if $(OBJECT_STYLE) == "32"
	$(INSTALL) -F $(TOOLKITDIR)/lib -idb "LIBWIDGET" -src libGLw.32/libGLw.a -O libGLw.a;
#endif
#if $(OBJECT_STYLE) == "N32"
	$(INSTALL) -F $(TOOLKITDIR)/lib32 -idb "LIBWIDGET" -src libGLw.N32/libGLw.a -O libGLw.a;
#endif
#if $(OBJECT_STYLE) == "64"
	$(INSTALL) -F $(TOOLKITDIR)/lib64 -idb "LIBWIDGET" -src libGLw.64/libGLw.a -O libGLw.a;
#endif

$(COMMONTARGS): $(COMMONPREF)$$@
