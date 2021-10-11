#
#ident	"@(#)Makefile	1.5	97/08/12 SMI"        
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/iconv/sparcv9/Makefile

.KEEP_STATE:


include ../Makefile.com
include ../../Makefile.cmd.64

#catalog: $(MSGDOMAIN64) $(POFILE)
#	$(RM) $(MSGDOMAIN64)/$(POFILE)
#	$(CP) $(POFILE) $(MSGDOMAIN64)

catalog:

# iconv and kbdcomp are built but not installed into proto
# 'make install' installs only $(ROOTLIB)/iconv/$(MACH64) directory

all: $(PROG) $(KPROG)

install: all $(ROOTDIRS64)

iconv_tbl:	$(CODESETS)

