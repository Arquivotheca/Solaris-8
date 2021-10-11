#
#ident	"@(#)Makefile	1.4	99/08/27 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#

include ../Makefile.com

DATAFILES += depend

ACTION=grep SUNWpcr depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWpcr       SunSoft Print - Client, (root)" >> depend; \
	echo "I SUNWlpu       LP Print Service, (usr)" >> depend; \
	chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action
install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION)
	touch $@

include ../Makefile.targ
