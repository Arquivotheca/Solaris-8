#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	98/07/29 SMI"

include ../Makefile.com

DATAFILES += depend

.KEEP_STATE:

all: $(FILES) action

action: depend
	chmod 0666 depend
	egrep '(^#|^P SUNWcsr)' depend >$@
	cp $@ depend
	chmod 0444 depend

install: all pkg

include ../Makefile.targ
