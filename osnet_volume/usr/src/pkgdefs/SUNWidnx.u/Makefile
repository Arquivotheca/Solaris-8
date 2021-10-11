#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/04/13 SMI"
#

include ../Makefile.com

DATAFILES += depend

ACTION_SUNWidn=grep SUNWidn depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWidn	Inter-Domain Network Modules for Sun Enterprise 10000" \
	>> depend; chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action
install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWidn)
	touch $@

include ../Makefile.targ
