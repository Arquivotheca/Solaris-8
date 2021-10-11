# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

#ident	"@(#)Makefile	1.3	99/05/27 SMI"
#
# cmd/geniconvtbl/i386/Makefile

.KEEP_STATE:

include ../Makefile.com

catalog: $(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

all:	$(PROG) $(ITM)

install: all $(ROOTBIN) $(ROOTPROG) $(ROOTITM32) $(ROOTLINKS32)
