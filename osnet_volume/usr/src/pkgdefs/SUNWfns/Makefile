#
#ident	"@(#)Makefile	1.2	97/04/23 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#

include	../Makefile.com

DATAFILES += depend

ACTION_SUNWlibC=grep SUNWlibC depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWlibC	SPARCompilers Bundled libC" >> depend; \
	chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action

install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWlibC)
	touch $@

include ../Makefile.targ
