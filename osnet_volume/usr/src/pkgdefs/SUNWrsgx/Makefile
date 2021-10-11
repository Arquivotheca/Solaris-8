#
#pragma ident	"@(#)Makefile	1.4	97/11/16 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#

include	../Makefile.com

DATAFILES += depend

ACTION_SUNWgssx=grep SUNWgssx depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWgssx	gssx" >> depend; \
	chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action

install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWgssx)
	touch $@

include ../Makefile.targ
