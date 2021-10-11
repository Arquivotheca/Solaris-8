#
# @(#)Makefile 99/06/28 SMI; SunOS BSM
#
# Copyright (c) 1989,1998,1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/allocate/Makefile
#

include		../Makefile.cmd

TEXT_DOMAIN=	SUNW_OST_OSCMD

ROOTSEC =	$(ROOTETC)/security
ROOTSECDEV =	$(ROOTSEC)/dev
ROOTSECLIB =	$(ROOTSEC)/lib
ROOTDIRS =	$(ROOTSECDEV) $(ROOTSECLIB)

PROG1 =		allocate
PROG2 =		mkdevmaps mkdevalloc
PROG =		$(PROG1) $(PROG2)
DEFILE =	deallocate
LISTFILE =	list_devices
RTLCKS =	audio fd0 sr0 st0 st1
SCRIPTS =	st_clean fd_clean sr_clean audio_clean

POFILE=		allocate_all.po
POFILES=	allocate.po allocate3.po audio_clean.po \
		mkdevmaps.po mkdevalloc.po dev_clean.po


ROOTPROG =	$(PROG1:%=$(ROOTUSRSBIN)/%) $(PROG2:%=$(ROOTUSRSBIN)/%)
ROOTLOCKS=	$(RTLCKS:%=$(ROOTSECDEV)/%)
ROOTSCRIPTS=	$(SCRIPTS:%=$(ROOTSECLIB)/%)

allocate :=	POBJS = allocate.o  allocate3.o
mkdevmaps :=	POBJS = mkdevmaps.o
mkdevalloc :=	POBJS = mkdevalloc.o
audio_clean :=	POBJS = audio_clean.o

OBJS =		allocate.o allocate3.o mkdevmaps.o mkdevalloc.o
SRCS =		$(OBJS:%.o=%.c)

FILEMODE=	0755
DIRMODE=	0755
OWNER=		root
GROUP=		sys
$(ROOTUSRSBIN)/% :=	FILEMODE = 04755
$(ROOTUSRSBIN)/% :=	OWNER = root
$(ROOTUSRSBIN)/% :=	GROUP = bin
$(ROOTSECDEV)/%  :=	FILEMODE = 0400
$(ROOTSECDEV)/%  :=	OWNER = root
$(ROOTSECDEV)/%  :=	GROUP = bin
$(ROOTSECLIB)/%  :=	FILEMODE = 0751

LDLIBS +=	-lnsl -lbsm -lsecdb

.KEEP_STATE:

all :		$(PROG) $(RTLCKS) $(SCRIPTS)

install :	all $(ROOTDIRS) $(ROOTPROG) $(ROOTLOCKS) $(ROOTSCRIPTS)
		$(RM) $(ROOTUSRSBIN)/$(DEFILE)
		$(LN) $(ROOTUSRSBIN)/$(PROG1) $(ROOTUSRSBIN)/$(DEFILE)
		$(RM) $(ROOTUSRSBIN)/$(LISTFILE)
		$(LN) $(ROOTUSRSBIN)/$(PROG1) $(ROOTUSRSBIN)/$(LISTFILE)

$(PROG) :	$$(POBJS)
		$(LINK.c) $(POBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

$(RTLCKS):
	touch $@

$(ROOTDIRS):
		$(INS.dir)

$(ROOTSECDEV)/%: %
	$(INS.file)

$(ROOTSECLIB)/%: %
	$(INS.file)

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat     $(POFILES)      > $@

clean :
		$(RM) $(OBJS) $(RTLCKS) $(ROOTCLEAN)
		$(RM) $(SCRIPTS) $(PROG) $(POFILE) $(POFILES)

lint :		lint_SRCS

strip :
		$(STRIP) $(PROG)

include		../Makefile.targ
