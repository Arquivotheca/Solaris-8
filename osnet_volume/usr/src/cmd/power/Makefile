#
# Copyright (c) 1994 - 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.17	99/12/06 SMI"
#
# cmd/power/Makefile
#
#

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
cstyle :=	TARGET= cstyle

SUBDIRS	=	initpkg
DAEMON_SRCS =	powerd.c sysstat.c
DAEMON_OBJS =	$(DAEMON_SRCS:%.c=%.o)
DAEMON =	powerd
PMCFG_SRCS = 	pmconfig.c
PMCFG_OBJS =	$(PMCFG_SRCS:%.c=%.o)
PMCFG = 	$(PMCFG_SRCS:%.c=%)
SRCS = 		$(DAEMON_SRCS) $(PMCFG_SRCS)
OBJS = 		$(SRCS:%.c=%.o)
SCRIPTS =	sysidpm.sh
SYSIDPM = 	sysidpm
PROG =		$(DAEMON) $(PMCFG) $(SYSIDPM)
ETCFILES =	power.conf
POWERPERM =	power

include ../Makefile.cmd

TEXT_DOMAIN=	SUNW_OST_OSCMD

POFILE=		power_all.po
POFILES=	pmconfig.po
# needed because POFILES is deleted by clobber, but it's under
# SCCS control here.
OPOFILES=	sysidpm.po

#
# keep 'pmconfig' single threaded since it calls vfork
# see BugID 4264654
#
LDLIBS +=	-lcmd
DAEMON_LDLIBS =	-lkstat -lthread -ldevinfo
DAEMON_FLAGS =	-D_REENTRANT

OWNER= root
ROOTUSRSBINPMCFG= $(PMCFG:%=$(ROOTUSRSBIN)/%)
$(ROOTUSRSBINPMCFG) := GROUP= bin
$(ROOTUSRSBINPMCFG) := FILEMODE= 4555

GROUP= sys
ROOTLIBPOWER= $(ROOTLIB)/power
ROOTLIBPOWERDAEMON= $(DAEMON:%=$(ROOTLIBPOWER)/%)
$(ROOTLIBPOWER) := FILEMODE= 755

ROOTETCFILES= $(ETCFILES:%=$(ROOTETC)/%)
$(ROOTETCFILES) := FILEMODE= 644

DFLTD= $(ROOTETC)/default
ROOTDFLFILES= $(POWERPERM:%=$(DFLTD)/%)
$(ROOTDFLFILES) := FILEMODE= 444

ROOTUSRSBINSYSIDPM= $(SYSIDPM:%=$(ROOTUSRSBIN)/%)
$(ROOTUSRSBINSYSIDPM) := FILEMODE= 755

#
# lint pass one enforcement
#
CFLAGS += -v

.KEEP_STATE:

all: $(PROG) $(SUBDIRS) $(POWERPERM).dfl $(ETCFILES) $(SCRIPTS)

install clean: $(SUBDIRS)

$(DAEMON): $(DAEMON_OBJS)
	$(LINK.c) $(DAEMON_FLAGS) $(DAEMON_OBJS) -o $@ $(DAEMON_LDLIBS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

install: all $(ROOTUSRSBINPMCFG) $(ROOTLIBPOWER) \
	$(ROOTLIBPOWERDAEMON) $(ROOTETCFILES) $(ROOTUSRSBINSYSIDPM) \
	$(ROOTDFLFILES)

$(DFLTD)/%:	%.dfl
	$(INS.rename)

$(ROOTLIBPOWER):
	$(INS.dir)

$(ROOTLIBPOWER)/%:	%
#	$(INS.file)
	/usr/bin/rm -f $(ROOTLIBPOWERDAEMON)
	install -s -m 0555 -u root -g bin -f $(ROOTLIBPOWER) powerd

$(POFILE):	$(POFILES) $(OPOFILES)
	$(RM)	$@
	cat	$(POFILES) $(OPOFILES)	> $@

clean:
	$(RM) $(OBJS) $(SYSIDPM)
	$(RM) $(POFILE) $(POFILES)

lint := LINTFLAGS=-auxn
lint:
	$(LINT.c) $(DAEMON_SRCS)
	$(LINT.c) $(PMCFG_SRCS)

cstyle:
	$(CSTYLE) $(SRCS)

include ../Makefile.targ
