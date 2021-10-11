#
#ident	"@(#)Makefile	1.5	99/07/10 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#

include ../Makefile.com

DATAFILES += i.rOWconfig

MACHDATAFILES += i.sdconf
CLOBBERFILES += $(MACHDATAFILES)

.KEEP_STATE:

all: $(FILES) $(MACHDATAFILES)
install: all pkg

include ../Makefile.targ
