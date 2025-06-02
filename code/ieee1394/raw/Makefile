#!smake
# Copyright 1998 Silicon Graphics, Inc.  All rights reserved.
#
# $Revision: 1.00

include commondefs

LCINCS=         -I../include/

LLDLIBS= -lc -lraw1394

OPTIMIZER= -g

TARGETS =       \
		busreset 	\
		wait_for_reset	\
		topomap  	\
		speedmap 	\
		readquadlet 	\
		writequadlet 	\
		lockquadlet  	\
		readblock	\
		writeblock	\
		$(NULL) 

#
# Rules
#

default:        $(TARGETS)

include commonrules
