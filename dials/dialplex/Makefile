# Makefile for Dialplex - SGI Open Inventor Dial Control System
# Compatible with IRIX 6.5 make

# Compiler and flags
CC = CC
CFLAGS = -g -n32 -I/usr/include/Inventor -I/usr/include/X11
LDFLAGS = -L/usr/lib32
LIBS = -lInventorInput -lInventorXt -lInventor -lX11 -lGL -lGLU -lm

# Object files
TARGET = dialplex
OBJECTS = dialplex.o overlay.o dialset0.o dialset1.o dialset2.o dialset3.o dialset4.o dialset5.o dialset6.o dialset7.o dialset8.o dialset9.o

# Default target
all: dialplex

# Link the final executable
dialplex: dialplex.o overlay.o dialset0.o dialset1.o dialset2.o dialset3.o dialset4.o dialset5.o dialset6.o dialset7.o dialset8.o dialset9.o
	CC dialplex.o overlay.o dialset0.o dialset1.o dialset2.o dialset3.o dialset4.o dialset5.o dialset6.o dialset7.o dialset8.o dialset9.o $(LDFLAGS) $(LIBS) -o dialplex

# Compile dialplex.cc to dialplex.o
dialplex.o: dialplex.cc
	CC $(CFLAGS) -c dialplex.cc

# Compile overlay.cc to overlay.o
overlay.o: overlay.cc
	CC $(CFLAGS) -c overlay.cc

dialset0.o: dialset0.cc
	CC $(CFLAGS) -c dialset0.cc

dialset1.o: dialset1.cc
	CC $(CFLAGS) -c dialset1.cc

dialset2.o: dialset2.cc
	CC $(CFLAGS) -c dialset2.cc

dialset3.o: dialset3.cc
	CC $(CFLAGS) -c dialset3.cc

dialset4.o: dialset4.cc
	CC $(CFLAGS) -c dialset4.cc

dialset5.o: dialset5.cc
	CC $(CFLAGS) -c dialset5.cc

dialset6.o: dialset6.cc
	CC $(CFLAGS) -c dialset6.cc

dialset7.o: dialset7.cc
	CC $(CFLAGS) -c dialset7.cc

dialset8.o: dialset8.cc
	CC $(CFLAGS) -c dialset8.cc

dialset9.o: dialset9.cc
	CC $(CFLAGS) -c dialset9.cc

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f *.o dialplex
	@echo "Clean complete."