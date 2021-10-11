#
#ident	"@(#)Makefile	1.8	92/12/15 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/osadmin/shell/Makefile
#

include ../../Makefile.cmd
include ../Makefile.include

#SHFILES = admerr agefile diskumount drivename labelfsname samedev
SHFILES = agefile
ROOTSHFILES = $(SHFILES:%=$(ROOTLIB)/%)
CLOBBERFILES = "Nothing to clobber"

.KEEP_STATE:

all: $(SHFILES) 

install: all $(ROOTSHFILES)

# Don't install a directory already installed by Targetdirs
#$(ROOTLIB):
#	$(INS.dir)

clean:

lint:

include ../../Makefile.targ
