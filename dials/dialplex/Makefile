# Makefile for Dialplex - SGI Open Inventor Dial Control System
# Compatible with IRIX 6.5 make

# Compiler and flags
CC = CC
CFLAGS = -g -n32
INCLUDES = -I/usr/include/Inventor -I/usr/include/X11
LIBS = -L/usr/lib32 -lInventorInput -lInventorXt -lInventor -lX11 -lGL -lGLU -lm

# Object files
OBJECTS = dialplex.o overlay.o
TARGET = dialplex

# Default target
all: $(TARGET)

# Link the final executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LIBS) -o $(TARGET)

# Compile dialplex.cc to dialplex.o
dialplex.o: dialplex.cc overlay.h
	$(CC) $(CFLAGS) $(INCLUDES) -c dialplex.cc -o dialplex.o

# Compile overlay.cc to overlay.o
overlay.o: overlay.cc overlay.h
	$(CC) $(CFLAGS) $(INCLUDES) -c overlay.cc -o overlay.o

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Rebuild everything
rebuild: clean all 