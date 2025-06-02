#!smake

include $(ROOT)/usr/include/make/commondefs

SUBDIRS=  \
	lib		\
	clock		\
	copypixels	\
	fieldblend	\
	image		\
	videoin		\
	$(NULL)

COMMONPREF = dmedia

default : all

$(COMMONTARGS) all : $(_FORCE)
	$(SUBDIRS_MAKERULE)

include $(COMMONRULES)
