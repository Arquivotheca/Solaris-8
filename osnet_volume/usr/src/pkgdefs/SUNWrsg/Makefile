#
#pragma ident	"@(#)Makefile	1.3	97/11/11 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#

include	../Makefile.com

DATAFILES += depend

ACTION_SUNWgss=grep SUNWgss depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWgss	gss" >> depend; \
	chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action

install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWgss)
	touch $@

include ../Makefile.targ
