#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

TARGETS = libpdb.32 libpdb.N32 libpdb.64

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
WOFF =
OPTIMIZER = -g
#endif

TOOLKITDIR ?= /usr/share/src/OpenGL/toolkits

LCINCS = -I$(ROOT)$(TOOLKITDIR)/include

HFILES = \
	pdb.h \
	$(NULL)

CFILES = \
	pdb.c \
	$(NULL)

default all: $(_FORCE)
	$(MAKE) library;

include $(COMMONRULES)

library: $(_FORCE)
	@if [ ! -d libpdb.$(OBJECT_STYLE) ]; then \
	    mkdir libpdb.$(OBJECT_STYLE); \
	fi; \
	cd libpdb.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile links;
	cd libpdb.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile libpdb.a;

links:
	@for i in $(HFILES) $(CFILES); do \
	    if [ ! -r $$i ]; then \
		ln -s ../$$i $$i; \
	    fi; \
	done;

libpdb.a: $(OBJECTS)
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
	$(INSTALL) -F $(TOOLKITDIR)/include -idb "LIBPDB" pdb.h;

install: headers default
#if $(OBJECT_STYLE) == "32"
	$(INSTALL) -F $(TOOLKITDIR)/lib -idb "LIBPDB" -src libpdb.32/libpdb.a -O libpdb.a;
#endif
#if $(OBJECT_STYLE) == "N32"
	$(INSTALL) -F $(TOOLKITDIR)/lib32 -idb "LIBPDB" -src libpdb.N32/libpdb.a -O libpdb.a;
#endif
#if $(OBJECT_STYLE) == "64"
	$(INSTALL) -F $(TOOLKITDIR)/lib64 -idb "LIBPDB" -src libpdb.64/libpdb.a -O libpdb.a;
#endif

$(COMMONTARGS): $(COMMONPREF)$$@
