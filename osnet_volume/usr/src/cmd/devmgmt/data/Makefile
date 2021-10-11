#
#ident	"@(#)Makefile	1.7	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/devmgmt/data/Makefile
#

FILES=  device.tab dgroup.tab
ROOTETCFILES= $(FILES:%=$(ROOTETC)/%)

include ../../Makefile.cmd

FILEMODE=	444
OWNER=		root

# conditional assignments
$(ROOTETC)/device.tab :=	GROUP= root
$(ROOTETC)/dgroup.tab :=	GROUP= sys

.KEEP_STATE:

all: $(FILES) 

install: all $(ROOTETCFILES)

clean clobber lint:

# no PROG to lint or clobber, so don't include.
#include ../../Makefile.targ
