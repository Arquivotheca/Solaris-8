#
# ident	"@(#)Makefile	1.1	98/08/17 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

include ../Makefile.com

DATAFILES += depend

ACTION_SUNWdrr=grep SUNWdrr depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWdrr	Dynamic Reconfiguration Modules for Sun Enterprise 10000" \
	>> depend; chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action
install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWdrr)
	touch $@

include ../Makefile.targ
