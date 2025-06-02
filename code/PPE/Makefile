# Makefile for ppe
#

XFLAGS= -lSgm -lXm -lXt -lX11 -lPW
AUDIOFLAGS= -laudio -laudiofile
CFLAGS= -O2 -mips2 -lm

Ppe: Matrix.o Clip.o ppe.o
	cc Matrix.o Clip.o ppe.o -o ppe $(XFLAGS) $(AUDIOFLAGS) $(CFLAGS)

matrix: Matrix.c Clip.c
	cc -c $(CFLAGS) Matrix.c Clip.c

pppe: ppe.c
	cc -c $(CFLAGS) ppe.c
