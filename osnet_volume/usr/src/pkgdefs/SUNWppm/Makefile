#
# ident	"@(#)Makefile	1.1	99/03/16 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include ../Makefile.com

DATAFILES += depend

ACTION= \
	( chmod 666 depend; \
	echo "P SUNWpcu		SunSoft Print - Client, (usr)" >> depend; \
	chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action

install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION)
	touch $@

include ../Makefile.targ
