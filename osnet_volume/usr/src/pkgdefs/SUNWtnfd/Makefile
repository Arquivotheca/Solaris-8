#
#ident	"@(#)Makefile	1.2	94/12/12 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#

include ../Makefile.com

DATAFILES += depend

ACTION_all=grep tnfc depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWtnfc	TNF Core Components" >> depend; \
	chmod 444 depend )


.KEEP_STATE:

all: $(FILES) action

# action is a pseudotarget denoting completed work on the depend file
action:	depend
	$(ACTION_all)
	touch $@

install: all pkg

include ../Makefile.targ
