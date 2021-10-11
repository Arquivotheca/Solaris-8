#
#ident	"@(#)Makefile	1.10	93/04/08 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# uts/common/fs/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=  fs_subr.h

PROCHDRS=   prdata.h

ROOTDIR=	$(ROOT)/usr/include/sys
ROOTDIRS=	$(ROOTDIR) $(ROOTDIR)/proc

ROOTHDRS=	$(HDRS:%=$(ROOTDIR)/%)
ROOTPROCHDRS=	$(PROCHDRS:%=$(ROOTDIR)/proc/%)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

$(ROOTDIR)/proc/%: proc/%
	$(INS.file)

# standards checking rules
proc/%.check:      proc/%.h
	$(DOT_H_CHECK)

CHECKHDRS= $(HDRS:%.h=%.check)  \
	$(PROCHDRS:%.h=proc/%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS) $(ROOTPROCHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
