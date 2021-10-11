#
#pragma ident	"@(#)Makefile	1.3	99/07/20 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

include	../Makefile.com

DATAFILES += depend i.tiservices i.preserve

.KEEP_STATE:

all: $(FILES) action

install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	touch $@

include ../Makefile.targ
