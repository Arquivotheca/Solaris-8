#
# ident	"@(#)Makefile	1.23	99/03/30 SMI"
#
# Copyright (c) 1995,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/bnu/Makefile
#


include		../Makefile.cmd

USERPROG1 =	uudecode uuencode
USERPROG =	$(USERPROG1) uucp uuglist uuname uustat uux ct cu
UUCPPROG1 =	bnuconvert uucheck uucleanup
UUCPPROG =	$(UUCPPROG1) remote.unknown uusched uucico uuxqt
SBINPROG =	in.uucpd

PROG =		$(UUCPPROG) $(USERPROG) $(SBINPROG)

LIBSHELLS =	Uutry uudemon.admin uudemon.cleanup uudemon.hour uudemon.poll
BINSHELLS =	uulog uupick uuto
SHELLS =	$(LIBSHELLS) $(BINSHELLS)
CRONTAB =	uudemon.crontab

# also define DATAKIT in parms.h
#LIBRARY =	libdk.a
#OBJECTS =	dkbreak.o dkdial.o dkerr.o dkminor.o dtnamer.o

#include	../../lib/Makefile.lib

FILES =		Config Devconfig Devices Dialcodes	\
		Dialers Grades Limits Permissions	\
		Poll Sysfiles Systems

CLEANFILES =	$(SRCS:%.c=%.o)

TXTFILES =	README $(FILES)	\
		dkbreak.c dkdial.c dkerr.c dkminor.c dtnamer.c	\
		Cvt Install SetUp Teardown dial.c dial.h getopt.c strpbrk.c

CAT =		cat
POFILE =	bnu.po
# step around name collisions
POFILES =	$(PROG:%=%.cat)

CPPFLAGS =	-I. $(SMALLFLAG) $(CPPFLAGS.master) # -DSMALL
LDLIBS +=	$(LIBRARY) -lgen # -lunet -lbtl

OWNER =		uucp
GROUP =		uucp
FILEMODE =	4111

# if you change these directories, change them in uucp.h as well

ROOTLIBUUCP =	$(ROOT)/usr/lib/uucp
ROOTETCUUCP =	$(ROOT)/etc/uucp
ROOTVAR =	$(ROOT)/var
ROOTVARUUCP =	$(ROOTVAR)/uucp
ROOTSPOOL =	$(ROOTVAR)/spool
ROOTSPLOCKS =	$(ROOTSPOOL)/locks
ROOTSPUUCP =	$(ROOTSPOOL)/uucp
ROOTSPPUB =	$(ROOTSPOOL)/uucppublic

DOTADMIN =	.Admin
DOTCORRUPT =	.Corrupt
DOTLOG =	.Log
DOTOLD =	.Old
DOTSEQ =	.Sequence
DOTSTATUS =	.Status
DOTWORKSPACE =	.Workspace
DOTXQT =	.Xqtdir
DOTDIRS =	$(DOTADMIN) $(DOTLOG) $(DOTOLD) $(DOTSEQ) $(DOTSTATUS)
DOTSPDIRS =	$(DOTCORRUPT) $(DOTWORKSPACE) $(DOTXQT)

DOTLOGCICO =	$(DOTLOG)/uucico
DOTLOGUUCP =	$(DOTLOG)/uucp
DOTLOGUUX =	$(DOTLOG)/uux
DOTLOGUUXQT =	$(DOTLOG)/uuxqt
DOTLOGDIRS =	$(DOTLOGCICO) $(DOTLOGUUCP) $(DOTLOGUUX) $(DOTLOGUUXQT)

ROOTDOTDIRS =	$(DOTDIRS:%=$(ROOTVARUUCP)/%)
ROOTDOTSPDIRS =	$(DOTSPDIRS:%=$(ROOTSPUUCP)/%)
ROOTDOTLOGDIRS = $(DOTLOGDIRS:%=$(ROOTVARUUCP)/%)

ROOTPROG =	$(USERPROG:%=$(ROOTBIN)/%)
ROOTSPROG =	$(SBINPROG:%=$(ROOTUSRSBIN)/%)
ROOTSHELLS =	$(BINSHELLS:%=$(ROOTBIN)/%)
ROOTUUCPPROG =	$(UUCPPROG:%=$(ROOTLIBUUCP)/%)
ROOTLIBSHELLS =	$(LIBSHELLS:%=$(ROOTLIBUUCP)/%)
ROOTCRONTAB =	$(CRONTAB:%=$(ROOTLIBUUCP)/%)
ROOTFILES =	$(FILES:%=$(ROOTETCUUCP)/%)

ROOTDIRS =	$(ROOTLIBUUCP) $(ROOTETCUUCP) $(ROOTSPLOCKS)	\
		$(ROOTDOTDIRS) $(ROOTDOTSPDIRS) $(ROOTDOTLOGDIRS)

ROOTSYMDIRS =	$(DOTDIRS:%=$(ROOTSPUUCP)/%)
SYMDEST =	../../uucp
ROOTREMOTE =	$(ROOTETCUUCP)/remote.unknown
REMOTESYMDEST =	../../usr/lib/uucp

$(USERPROG1) := CPPFLAGS += -D_FILE_OFFSET_BITS=64

# Mode is assigned in Targetdirs.  It shoudn't be re-assigned here
#$(ROOTSPPUB) :=				DIRMODE = 1777
$(USERPROG1:%=$(ROOTBIN)/%) :=		FILEMODE = 0555
$(USERPROG1:%=$(ROOTBIN)/%) :=		OWNER = root
$(ROOTSHELLS) :=			FILEMODE = 0555
$(ROOTSHELLS) :=			OWNER = root
$(ROOTLIBSHELLS) :=			FILEMODE = 0555
$(ROOTLIBSHELLS) :=			OWNER = root
$(UUCPPROG1:%=$(ROOTLIBUUCP)/%) :=	FILEMODE = 0110
$(ROOTCRONTAB) :=			FILEMODE = 0444
$(ROOTCRONTAB) :=			OWNER = root
$(ROOTFILES) :=				FILEMODE = 0644
$(ROOTETCUUCP)/Permissions :=		FILEMODE = 0600
$(ROOTETCUUCP)/Systems :=		FILEMODE = 0600
$(ROOTBIN)/ct :=			OWNER = root
$(ROOTSPROG) :=				FILEMODE = 0555
$(ROOTSPROG) :=				OWNER = root

include		./Makefile.inc

.KEEP_STATE:

all:		$(TXTFILES) $(PROG) $(SHELLS)

install:	$(ROOTDIRS) $(ROOTSYMDIRS) $(ROOTPROG)		\
		$(ROOTUUCPPROG) $(ROOTLIBSHELLS) $(ROOTSHELLS)	\
		$(ROOTFILES) $(ROOTSYMFILES) $(ROOTSPROG)	\
		$(ROOTREMOTE) $(ROOTCRONTAB)

$(ROOTLIBUUCP)/% $(ROOTETCUUCP)/% $(ROOTUSRSBIN)/%: %
		$(INS.file)

$(PROG):	$$(POBJS) $(LIBRARY)
		$(LINK.c) $(POBJS) -o $@ $(LDLIBS) $(PLIBS)
		$(POST_PROCESS)

$(POFILE):	$(POFILES)
		$(RM) $@; $(CAT) $(POFILES) > $@

$(POFILES):	$$(POBJS:.o=.po)
		$(RM) $@; $(CAT) $(POBJS:.o=.po) > $@

$(ROOTDIRS):
		$(INS.dir)

$(ROOTSYMDIRS):
		$(RM) $@; $(SYMLINK) $(SYMDEST)/$(@F) $@

$(ROOTREMOTE):
		$(RM) $@; $(SYMLINK) $(REMOTESYMDEST)/$(@F) $@

clean:
		$(RM) $(CLEANFILES)

lint:		lint_SRCS

strip:
		$(STRIP) $(PROG)

include		../Makefile.targ
