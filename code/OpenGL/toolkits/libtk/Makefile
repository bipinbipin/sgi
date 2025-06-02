#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

TARGETS = libtk.32 libtk.N32 libtk.64

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
WOFF =
OPTIMIZER = -g
#endif

TOOLKITDIR ?= /usr/share/src/OpenGL/toolkits

DFILES = \
	README \
	$(NULL)

HFILES = \
	private.h \
	tk.h \
	$(NULL)

CFILES = \
	cursor.c \
	event.c \
	getset.c \
	shapes.c \
	window.c \
	$(NULL)

default all: $(_FORCE)
	$(MAKE) library;

include $(COMMONRULES)

library: $(_FORCE)
	@if [ ! -d libtk.$(OBJECT_STYLE) ]; then \
	    mkdir libtk.$(OBJECT_STYLE); \
	fi; \
	cd libtk.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile links;
	cd libtk.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile libtk.a;

links:
	@for i in $(HFILES) $(CFILES); do \
	    if [ ! -r $$i ]; then \
		ln -s ../$$i $$i; \
	    fi; \
	done;

libtk.a: $(OBJECTS)
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
	$(INSTALL) -F $(TOOLKITDIR)/include -idb "LIBTK" tk.h;

install: headers default
#if $(OBJECT_STYLE) == "32"
	$(INSTALL) -F $(TOOLKITDIR)/lib -idb "LIBTK" -src libtk.32/libtk.a -O libtk.a;
#endif
#if $(OBJECT_STYLE) == "N32"
	$(INSTALL) -F $(TOOLKITDIR)/lib32 -idb "LIBTK" -src libtk.N32/libtk.a -O libtk.a;
#endif
#if $(OBJECT_STYLE) == "64"
	$(INSTALL) -F $(TOOLKITDIR)/lib64 -idb "LIBTK" -src libtk.64/libtk.a -O libtk.a;
#endif

$(COMMONTARGS): $(COMMONPREF)$$@
