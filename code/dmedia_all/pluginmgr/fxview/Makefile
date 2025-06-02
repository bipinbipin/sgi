#
# This is the makefile for the fxview command.
#
# This makefile uses the SGI common definitions and rules include files
# which provide a set of useful macros and rules.  Makefiles that use these
# includes have a common set of targets that simplify different types of
# builds.  For example:
#
# To make fxview with -g debugging enabled:
#
# make OPTIMIZER=-g
#
# To make with a debugging ifdef turned on:
#
# make VCDEFS=-DDEBUG
#
# To remove all compiled files and executables:
#
# make clobber
#
# To remove just the intermediate build files (.o), but leave the final
# command:
#
# make clean
#
# To leave the intermediate build files, but remove the final target:
#
# make rmtargets
#
# This makefile will automatically generate a file ``Makedepend'' which
# will be included here.  The makedepend file contains all of the header
# file dependencies that the built objects have.

include $(ROOT)/usr/include/make/commondefs

C++FILES = \
	main.c++		\
	MainForm.c++		\
	MainFormUI.c++		\
	VkwindowMainWindow.c++	\
	unimplemented.c++	\
	ImageSource.c++		\
	ImageUtils.c++		\
	PlugIns.c++		\
	View.c++		\
	$(NULL)

GLLIBS=-lGLw -lGL -lGLU

CUTPASTELIBS= -lXmu

VIEWKITLIBS = \
	-lvk 			\
	-lvkhelp		\
	-lSgm 			\
	-lXpm 			\
	-ldesktopFileicon	\
	$(NULL)

LLDLIBS = \
	-lfxplugmgr 		\
	-lfxplugutils 		\
	-lmoviefile 		\
	-ldmedia 		\
	-lifl 			\
	-lil 			\
	$(CUTPASTELIBS)		\
	$(VIEWKITLIBS) 		\
	$(GLLIBS) 		\
	-lXm 			\
	-lXt 			\
	-lX11 			\
	-lgen			\
	$(NULL)

TARGETS = fxview

default all: $(TARGETS)

fxview : $(OBJECTS)
	$(C++F) -o $@ $(OBJECTS) $(LDFLAGS)

include $(COMMONRULES)
