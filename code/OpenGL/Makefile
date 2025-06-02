#!smake

OBJECT_STYLE ?= N32

COMMONPREF = standard
include $(ROOT)/usr/include/make/commondefs
sinclude $(ROOT)/usr/include/make/$(PRODUCT)defs

# These toolkit directories must be built first.
# The order is important.
LIBDIRS = \
	toolkits/libwidget \
	toolkits/libfont \
	toolkits/librgb \
	toolkits/libglut \
	toolkits/libtk \
	toolkits/libaux \
	toolkits/libpdb \
	toolkits/libisfast \
	$(NULL)

# These directories can be built in any order.
# Some depend on the toolkits above.
APPLDIRS = \
	book/red_book_92 \
	demos/atlantis \
	demos/backtrace \
	demos/drip \
	demos/fadeflip \
	demos/hiddenline \
	demos/ideas \
	demos/lorenz \
	demos/puzzle \
	demos/stonehenge \
	teach/glc \
	teach/glut_based \
	teach/glut_based_1.1 \
	teach/glw_based \
	teach/glx_based \
	teach/isfast \
	teach/motif \
	teach/nurbs/sgiflag \
	teach/pixel \
	teach/stereo \
	teach/texture \
	teach/xlib \
	tools/glsplay \
	tools/xglinfo \
	$(NULL)

SUBDIRS = \
	$(LIBDIRS) \
	$(APPLDIRS) \
	$(NULL)

default:
	@for d in $(LIBDIRS); do \
	    ( cd $$d; $(MAKE) headers ); \
	done;
	@for d in $(LIBDIRS); do \
	    ( cd $$d; $(MAKE) install ); \
	done;
	@for d in $(APPLDIRS); do \
	    ( cd $$d; $(MAKE) ); \
	done;

include $(COMMONRULES)

$(COMMONTARGS): $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)
