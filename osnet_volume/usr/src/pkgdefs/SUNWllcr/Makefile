#
#ident  "@(#)Makefile	1.3    99/03/31 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include ../Makefile.com

DATAFILES += depend
CLOBBERFILES += $(DATAFILES)

.KEEP_STATE:

all: $(FILES) action
# action is a pseudotarget denoting completed work on the depend file
action: depend
	grep SUNWllc depend > /dev/null || \
	(chmod 666 depend; \
	echo "P SUNWllc LLC2 core functionality" >> depend; \
	chmod 444 depend )
	touch $@

install: all pkg

include ../Makefile.targ
