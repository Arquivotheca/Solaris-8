#
#ident	"@(#)Makefile	1.25	99/02/21 SMI"
#
# Copyright (c) 1989-1996, 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/common/nfs/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=	export.h	lm.h		nfs.h		nfssys.h \
	nfs_log.h \
	mount.h		nfs_acl.h	nfs_clnt.h	rnode.h \
	lm_nlm.h	lm_server.h	nfs_sec.h

ROOTDIRS= $(ROOT)/usr/include/nfs

ROOTHDRS= $(HDRS:%=$(ROOTDIRS)/%)

CHECKHDRS= $(HDRS:%.h=%.check)

$(ROOTDIRS)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
