# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile 1.4     99/05/27 SMI"
#
# cmd/geniconvtbl/sparcv9/Makefile

.KEEP_STATE:



include ../Makefile.com
include ../../Makefile.cmd.64

#catalog: $(MSGDOMAIN64) $(POFILE)
#	$(RM) $(MSGDOMAIN64)/$(POFILE)
#	$(CP) $(POFILE) $(MSGDOMAIN64)

lint := SRCS = $(SRCI)

all: $(ITM)

install: all  $(ROOTITM64) $(ROOTLINKS64)

both: $(ITM) $(PROG)

catalog:
