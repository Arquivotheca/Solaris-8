#
# Copyright (c) 1989-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.44	99/07/07 SMI"
#

include		../../Makefile.cmd

sparc_PROG=
i386_PROG=initboot initpcihpc

PROG=		\
	ANNOUNCE	\
	MOUNTFSYS	\
	README		\
	RMTMPFILES	\
	audit		\
	autofs		\
	buildmnttab	\
	cachefs.daemon	\
	cachefs.root	\
	cacheos		\
	cacheos.finish	\
	coreadm		\
	cron		\
	devfsadm	\
	devlinks	\
	drvconfig	\
	initpcmcia	\
	keymap		\
	ldap.client	\
	mkdtab		\
	network		\
	nfs.client	\
	nfs.server	\
	nscd		\
	pcmcia		\
	rootusr		\
	rpc		\
	savecore	\
	standardmounts	\
	sysetup		\
	syslog		\
	utmpd		\
	uucp		\
	volmgt		\
	$($(MACH)_PROG)


OTHERPROG=	README.rcS README.rc2 README.rc3

TXTS= mk.init.d.sh

ROOTETCINITD=		$(ROOTETC)/init.d
ROOTETCRCSD=		$(ROOTETC)/rcS.d
ROOTETCRC0D=		$(ROOTETC)/rc0.d
ROOTETCRC1D=		$(ROOTETC)/rc1.d
ROOTETCRC2D=		$(ROOTETC)/rc2.d
ROOTETCRC3D=		$(ROOTETC)/rc3.d

DIRS=	$(ROOTETCINITD) \
	$(ROOTETCRCSD) \
	$(ROOTETCRC0D) \
	$(ROOTETCRC1D) \
	$(ROOTETCRC2D) \
	$(ROOTETCRC3D)

ROOTETCINITDPROG=	$(PROG:%=$(ROOTETCINITD)/%)

DIRMODE = 0755
FILEMODE = 0744
$(ROOTETCINITD)/README :=	FILEMODE = 0644
$(ROOTETCRCSD)/README :=	FILEMODE = 0644
$(ROOTETCRC2D)/README :=	FILEMODE = 0644
$(ROOTETCRC3D)/README :=	FILEMODE = 0644
OWNER = root
GROUP = sys

.KEEP_STATE:

all:		$(PROG) $(TXTS)

# Don't re-install directories already installed by Targetdirs
#$(DIRS):
#		$(INS.dir)

$(ROOTETCINITD)/%:	%
		$(INS.file)

$(ROOTETCRCSD)/%:	%.rcS
		$(INS.rename)

$(ROOTETCRC2D)/%:	%.rc2
		$(INS.rename)

$(ROOTETCRC3D)/%:	%.rc3
		$(INS.rename)

install:	all $(ROOTETCINITDPROG) \
		$(ROOTETCRCSD)/README \
		$(ROOTETCRC2D)/README \
		$(ROOTETCRC3D)/README

lint:

clean:

clobber:
