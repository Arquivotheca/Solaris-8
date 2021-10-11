#
# ident "@(#)Makefile 1.3     94/12/06 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#

include ../Makefile.com

DATAFILES += depend

ACTION_all=grep fns depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWfns	Federated Naming System" >> depend; \
	chmod 444 depend )


.KEEP_STATE:

all: $(FILES) action

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_all)
	touch $@

install: all pkg

include ../Makefile.targ
