#
#ident	"@(#)Makefile	1.2	99/06/18 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

ETCFILES= ioctl.syscon

include ../../Makefile.cmd

ROOTETCFILES= $(ETCFILES:%=$(ROOTETC)/%)

FILEMODE= 0644
OWNER= root
GROUP= sys

.KEEP_STATE:

all: $(ETCFILES)

install: all $(ROOTETCFILES)

clean:

lint:

clobber:

clean:

lint:

clobber:

FRC:
