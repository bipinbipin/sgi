
TARGETS = libTransportButton.a 

OBJECTS	= TransportButton.o

OPTIMIZER = -g
CVERSION  = -xansi
LC++INCS  = -DFUNCPROTO -I../include -I. 
LLDOPTS   = -L.
LIBRARY   = 
LLDLIBS   = -L. -lTransportButton -lvk -lXm -lXpm -lXt -lX11
ARFLAGS	= cruv

default all:	$(TARGETS)

$(TARGETS): $(OBJECTS) 
		$(AR) $(ARFLAGS) $@ $?

example:	example.o $(TARGETS)
		$(C++) -o $@ example.o $(LDFLAGS)

clean:
	rm -rf core *.BAK *.bak *.ckp *.und *~ *.o 

rmtargets:
	rm -rf core *.BAK *.bak *.ckp *.und *~ *.o $(TARGETS) 

clobber : clean rmtargets
	rm -rf $(MKDEPFILE)
