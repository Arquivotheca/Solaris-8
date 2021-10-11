#
#ident	"@(#)Makefile	1.1	97/08/12 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/files/sparc/Makefile
#

include ../Makefile.com

.KEEP_STATE:

# note conditional assignment below
install: all $(ROOTLIBS) $(ROOTPSLIB)

# to avoid make warnings, this conditional assignment must
# follow the include of Makefile.targ
$(ROOTPSLIB) := ROOTLIBDIR=$(ROOTETCLIB)
