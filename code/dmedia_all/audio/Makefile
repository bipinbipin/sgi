#!smake

SHELL = /bin/sh
CFLAGS = -O2 
ALL= concert_a motifexample aesover1 connect connect2 \
	disconnect find_digital getconns getgain getrate \
	setrate sinput soutput recordexample printdevs \
	sync_example sine8 events sonic waop wideplay widerecord \
	audin audout iomenu

CC =cc

default: all

all: $(ALL)

clean:
	rm -f *.o

clobber: clean
	rm -f $(ALL)

sonic: sonic.c
	$(CC) $(CFLAGS) -o $* $<  -lGLU -lGLw -lGL -lXm -lXt -lX11 -laudio -laudiofile -lm

.c:
	$(CC) $(CFLAGS) -o $* $< -laudio -laudiofile -ldmedia -lm -lXt -lXm

iomenu: iomenu.c++
	$(C++) $(CFLAGS) -o $* $< -lvk -lXm -lXt -lX11 -laudio
