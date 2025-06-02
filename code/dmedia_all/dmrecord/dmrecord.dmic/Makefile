include $(ROOT)/usr/include/make/commondefs

CFILES   = \
	main.c \
	capture.c \
	parseargs.c \
	setDefaults.c \
	getSpecifiedPort.c \
	$(NULL)

COBJS = $(CFILES:.c=.o)

TARGETS = dmrecord

LLDLIBS = -lmoviefile \
	  -lvl \
          -ldmedia \
          -lcl \
          -laudio \
	  -lgen

default all: $(TARGETS)

include $(COMMONRULES)


dmrecord: $(COBJS)
	$(CC) -o $@ $(COBJS) $(LDFLAGS) $(LDFLAGS)
