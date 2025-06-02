include $(ROOT)/usr/include/make/commondefs

CFILES   = \
	main.c \
	capture.c \
	parseargs.c \
	$(NULL)

COBJS = $(CFILES:.c=.o)

TARGETS = dmrecord

LLDLIBS = -lmoviefile -lcl -lvl  -ldmedia -laudio -lm -lgen

default all: $(TARGETS)

include $(COMMONRULES)


dmrecord: $(COBJS)
	$(CC) -o $@ $(COBJS) $(LDFLAGS) $(LDFLAGS)
