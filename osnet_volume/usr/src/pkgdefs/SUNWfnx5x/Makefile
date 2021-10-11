#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#
#pragma ident "@(#)Makefile	1.1 97/11/11 SMI"
#

include ../Makefile.com

DATAFILES += depend

ACTION_all=grep fnsx depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWfnsx	Federated Naming System for 64-bit" >> depend; \
	chmod 444 depend )


.KEEP_STATE:

all: $(FILES) action

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_all)
	touch $@

install: all pkg

include ../Makefile.targ
