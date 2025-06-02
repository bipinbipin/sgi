#!smake
# $Id: Makefile,v 1.3 1994/06/28 21:45:58 bowen Exp $

SUBDIRS = sv lib examples doc 


default:
	@for dir in $(SUBDIRS); do	\
	    echo "making $$dir";	\
	    (cd $$dir; $(MAKE) $@); 	\
	done;

clean::
	rm -f *~

clean::
	@for dir in $(SUBDIRS); do		\
	    echo "making $@ in $$dir";		\
	    (cd $$dir; $(MAKE) $(MAKEARGS) $@);	\
	done;

