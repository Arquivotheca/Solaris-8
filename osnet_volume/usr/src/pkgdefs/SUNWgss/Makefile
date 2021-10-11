#
#pragma ident	"@(#)Makefile	1.11	99/01/26 SMI"
#
# Copyright (c) 1997 - 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

include	../Makefile.com

DATAFILES += depend

ACTION_SUNWfns=grep SUNWfns depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWfns	fns" >> depend; \
	chmod 444 depend );

ACTION_SUNWgssc=grep SUNWgssc depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWgssc	gssapi configuration" >> depend; \
	chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action

install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWfns)
	$(ACTION_SUNWgssc)
	touch $@

include ../Makefile.targ
