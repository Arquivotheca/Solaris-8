#
#ident	"@(#)Makefile	1.10	96/04/18 SMI"
#
# Copyright (c) 1989,1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/ufs/quotaon/Makefile

FSTYPE=		ufs
LIBPROG=	quotaon
QUOTAOFF=	quotaoff
ATTMK=		$(LIBPROG)

#
# Note that no links are made from the rc[S0-3].d directories for this rc
# script.  The normal way to ship this is completely disabled.  The process
# for enabling this script is described in the "Routine System Administration
# Guide" (as of this time).
#
INIT_SCRIPT=	ufs_quota
ROOTINIT_D=	$(ROOTETC)/init.d
ROOTINIT_SCRIPT=$(ROOTINIT_D)/$(INIT_SCRIPT)

OTHERINSTALL=	$(ROOTLIBFSTYPE)/$(QUOTAOFF) \
		$(ROOTUSRSBIN)/$(LIBPROG) $(ROOTUSRSBIN)/$(QUOTAOFF) \
		$(ROOTINIT_SCRIPT)
LINKVALUEON=	../lib/fs/$(FSTYPE)/$(LIBPROG)
LINKVALUEOFF=	../lib/fs/$(FSTYPE)/$(QUOTAOFF)

include		../../Makefile.fstype

$(ROOTINIT_SCRIPT) :=	OWNER = root
$(ROOTINIT_SCRIPT) :=	GROUP = sys
$(ROOTINIT_SCRIPT) :=	FILEMODE = 0744

CPPFLAGS +=	-D_LARGEFILE64_SOURCE
lint:		lint_PROG

$(ROOTLIBFSTYPE)/$(QUOTAOFF):	$(ROOTLIBFSTYPE)/$(LIBPROG) 
		-$(RM) $@; $(LN) $(ROOTLIBFSTYPE)/$(LIBPROG) $@

$(ROOTUSRSBIN)/$(LIBPROG):
		-$(RM) $@; $(SYMLINK) $(LINKVALUEON) $@

$(ROOTUSRSBIN)/$(QUOTAOFF):
		-$(RM) $@; $(SYMLINK) $(LINKVALUEOFF) $@

$(ROOTINIT_D)/% :	%
		$(INS.file)
