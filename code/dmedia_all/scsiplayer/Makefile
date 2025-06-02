#
# main Makefile for the scsiplayer apps
#

include $(ROOT)/usr/include/make/commondefs
include $(ROOT)/usr/include/make/dmediadefs


HELPLIB= -Wl,-all -lhelpmsg

# ViewKit header and libraries

LC++INCS =  -I$(ROOT)/usr/include/Vk \
	    -I$(ROOT)/usr/include/X11 \
            -I$(ROOT)/usr/include/dmedia \
	    $(NULL)

VIEWKITLIBS= $(NETLS) -lSgm  \
	     -lavtransport -lavtimeline -lavvolume -lavtime -lavint -lcdbin \
	     -L /usr/lib32/debug -lvk -lXpm -lmoviefile  -lGL -lXmu -ldesktopFileicon $(HELPLIB)

AUDIOLIBS=  -laudiofile -laudio

# Add Additional libraries to USERLIBS:

LLDLIBS =  -L$(ROOT)/usr/lib $(VIEWKITLIBS) $(AUDIOLIBS) \
            -lXm -lXt -lX11 -lgen -ldataudio2 -lds -lmediad -ldmedia -lm -lpthread\

# Local C++ options.

LCXXOPTS = -nostdinc -I$(ROOT)/usr/include $(SAFLAG) $(VIEWKITFLAGS) 

OPTIMIZER= -g

# SGI makefiles don't recognize all C++ sufixes, so set up
# the one being used here.

COMPONENTFILES = Database.o \
		 AudioBuffer.o \
		 OutputFile.o \
		 Semaphore.o \
	         $(NULL)

GUIFILES = TrackBin.c++ \
           PlayerPanel.c++ \
	   MainWindow.c++ \
	   NumberDisplay.c++ \
	   ScsiPlayerApp.c++ \
	   main.c++ \
	   $(NULL)


PLAYERFILES =  CdPlayer.c++ \
	       DatPlayer.c++ \
	       CdAudio.o 


C++FILES = $(COMPONENTFILES) $(GUIFILES) $(PLAYERFILES)  


#
# The program being built
#
TARGETS= cdplayer

default all: $(TARGETS)

$(TARGETS): $(OBJECTS)
	$(C++) $(OPTIMIZER) $(OBJECTS) $(LDFLAGS)  -o $@

#
# To install on the local machine, do 'make install'
#

install: all
	$(INSTALL) -F /usr/lib/X11/app-defaults CdPlayer
	$(INSTALL) -m 4755 -F /usr/sbin cdplayer
	$(INSTALL) -F /usr/lib/X11/app-defaults -lns CdPlayer DatPlayer
	$(INSTALL) -F /usr/sbin -lns cdplayer datplayer


include $(COMMONRULES)



