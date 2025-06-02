#!smake

SHELL = /bin/sh
CFLAGS = -O2 
ALL= sound_function  

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
