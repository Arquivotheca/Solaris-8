#
#ident	"@(#)Makefile	1.1	98/06/04 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All Rights Reserved.
#

TOPDIR = ../../../../../..
PLATFORM = sun4u

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/Makefile.psm

.KEEP_STATE:

# DDIRS	= $(ROOT_PSM_DIR)

PROG = boot.conf
FILEMODE = 0644
OWNER = root
GROUP = sys

all: $(PROG)

install: all $(DDIRS) $(ROOT_PSM_PROG)

clean:

clobber:

lint:

include $(TOPDIR)/Makefile.psm.targ
