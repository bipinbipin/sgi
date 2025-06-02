#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

TARGETS = libisfast.32 libisfast.N32 libisfast.64

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
WOFF =
OPTIMIZER = -g
#endif

TOOLKITDIR ?= /usr/share/src/OpenGL/toolkits

LCDEFS = -DX11
LCINCS = -I$(ROOT)$(TOOLKITDIR)/include

HFILES = \
	isfast.h \
	visinfo.h \
	$(NULL)

CFILES = \
	isfast.c \
	visinfo.c \
	$(NULL)

default all: $(_FORCE)
	$(MAKE) library;

include $(COMMONRULES)

library: $(_FORCE)
	@if [ ! -d libisfast.$(OBJECT_STYLE) ]; then \
	    mkdir libisfast.$(OBJECT_STYLE); \
	fi; \
	cd libisfast.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile links;
	cd libisfast.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile libisfast.a;

links:
	@for i in $(HFILES) $(CFILES); do \
	    if [ ! -r $$i ]; then \
		ln -s ../$$i $$i; \
	    fi; \
	done;

libisfast.a: $(OBJECTS)
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
	$(INSTALL) -F $(TOOLKITDIR)/include -idb "LIBISFAST" isfast.h;

install: headers default
#if $(OBJECT_STYLE) == "32"
	$(INSTALL) -F $(TOOLKITDIR)/lib -idb "LIBISFAST" -src libisfast.32/libisfast.a -O libisfast.a;
#endif
#if $(OBJECT_STYLE) == "N32"
	$(INSTALL) -F $(TOOLKITDIR)/lib32 -idb "LIBISFAST" -src libisfast.N32/libisfast.a -O libisfast.a;
#endif
#if $(OBJECT_STYLE) == "64"
	$(INSTALL) -F $(TOOLKITDIR)/lib64 -idb "LIBISFAST" -src libisfast.64/libisfast.a -O libisfast.a;
#endif

$(COMMONTARGS): $(COMMONPREF)$$@
