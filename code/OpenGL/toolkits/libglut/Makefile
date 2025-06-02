#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

TARGETS = libglut.32 libglut.N32 libglut.64

GLDEBUG ?= no
#if $(GLDEBUG) != "no"
WOFF =
OPTIMIZER = -g
#endif

TOOLKITDIR ?= /usr/share/src/OpenGL/toolkits

LCINCS = -I$(ROOT)$(TOOLKITDIR)/include

DFILES = \
	MonoRoman.stroke \
	Roman.stroke \
	$(NULL)

HFILES = \
	glut.h \
	glutbitmap.h \
	glutint.h \
	glutstroke.h \
	layerutil.h \
	$(NULL)

CFILES = \
	glut_8x13.c \
	glut_9x15.c \
	glut_bitmap.c \
	glut_bwidth.c \
	glut_cindex.c \
	glut_cursor.c \
	glut_dials.c \
	glut_dstr.c \
	glut_event.c \
	glut_ext.c \
	glut_fullscrn.c \
	glut_get.c \
	glut_hel10.c \
	glut_hel12.c \
	glut_hel18.c \
	glut_init.c \
	glut_input.c \
	glut_menu.c \
	glut_mesa.c \
	glut_modifier.c \
	glut_mroman.c \
	glut_overlay.c \
	glut_roman.c \
	glut_shapes.c \
	glut_space.c \
	glut_stroke.c \
	glut_swidth.c \
	glut_tablet.c \
	glut_teapot.c \
	glut_tr10.c \
	glut_tr24.c \
	glut_util.c \
	glut_vidresize.c \
	glut_warp.c \
	glut_win.c \
	glut_winmisc.c \
	layerutil.c \
	$(NULL)

default all: $(_FORCE)
	$(MAKE) library;

include $(COMMONRULES)

library: $(_FORCE)
	@if [ ! -d libglut.$(OBJECT_STYLE) ]; then \
	    mkdir libglut.$(OBJECT_STYLE); \
	fi; \
	cd libglut.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile links;
	cd libglut.$(OBJECT_STYLE); OBJECT_STYLE=$(OBJECT_STYLE) $(MAKE) -f ../Makefile libglut.a;

links:
	@for i in $(DFILES) $(HFILES) $(CFILES); do \
	    if [ ! -r $$i ]; then \
		ln -s ../$$i $$i; \
	    fi; \
	done;

libglut.a: $(OBJECTS)
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
	$(INSTALL) -F $(TOOLKITDIR)/include/GL -idb "LIBGLUT" glut.h;

install: headers default
#if $(OBJECT_STYLE) == "32"
	$(INSTALL) -F $(TOOLKITDIR)/lib -idb "LIBGLUT" -src libglut.32/libglut.a -O libglut.a;
#endif
#if $(OBJECT_STYLE) == "N32"
	$(INSTALL) -F $(TOOLKITDIR)/lib32 -idb "LIBGLUT" -src libglut.N32/libglut.a -O libglut.a;
#endif
#if $(OBJECT_STYLE) == "64"
	$(INSTALL) -F $(TOOLKITDIR)/lib64 -idb "LIBGLUT" -src libglut.64/libglut.a -O libglut.a;
#endif

$(COMMONTARGS): $(COMMONPREF)$$@
