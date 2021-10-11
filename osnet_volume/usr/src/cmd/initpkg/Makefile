#
# Copyright (c) 1989-1994, 1997-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.34	99/06/18 SMI"
#

SHFILES=	dfstab vfstab inittab
CPFILES=	rcS rc0 rc1 rc2 rc3 mountall shutdown swapadd umountall
ALL=		$(SHFILES) $(CPFILES)
TXTS=		rcS.d/mk.rcS.d.sh rc0.d/mk.rc0.d.sh \
		rc1.d/mk.rc1.d.sh rc2.d/mk.rc2.d.sh rc3.d/mk.rc3.d.sh
CLOBBERFILES=	$(ALL)
SUBDIRS=	rcS.d rc0.d rc1.d rc2.d rc3.d

include ../Makefile.cmd

ETCINITD=	$(ROOTETC)/init.d
ETCDFSD=	$(ROOTETC)/dfs
DIRS=		$(ETCINITD) $(ETCDFSD)

SBINF=		rcS mountall rc0 rc1 rc2 rc3 swapadd umountall
SBINL=		rc5 rc6
USRSBINF=	mountall shutdown umountall

sparc_ETCTABS=
i386_ETCTABS=	bootrc
ETCTABS=	vfstab inittab nscd.conf $($(MACH)_ETCTABS)

DFSTAB=		dfstab
SBINETC=	rcS mountall rc0 rc1 rc2 rc3 rc5 rc6 swapadd umountall
USRSBINETC=	shutdown

OWNER=		root
GROUP=		sys
FILEMODE=	0744

ROOTSBINF=	$(SBINF:%=$(ROOTSBIN)/%)
ROOTSBINL=	$(SBINL:%=$(ROOTSBIN)/%)
ROOTUSRSBINF=	$(USRSBINF:%=$(ROOTUSRSBIN)/%)
ROOTETCTABS=	$(ETCTABS:%=$(ROOTETC)/%)
ROOTDFSTAB=	$(DFSTAB:%=$(ETCDFSD)/%)
SYMSBINF=	$(SBINETC:%=$(ROOTETC)/%)
SYMUSRSBINF=	$(USRSBINETC:%=$(ROOTETC)/%)

$(ROOTETC)/inittab		:= FILEMODE =	0644
$(ROOTETC)/vfstab		:= FILEMODE =	0644
$(ROOTETC)/nscd.conf		:= FILEMODE =	0644
$(ROOTETC)/bootrc		:= FILEMODE =	0755
$(ROOTDFSTAB)			:= FILEMODE =	0644
$(ROOTSBIN)/mountall		:= FILEMODE =	0555
$(ROOTUSRSBIN)/mountall		:= FILEMODE =	0555
$(ROOTSBIN)/umountall		:= FILEMODE =	0555
$(ROOTUSRSBIN)/umountall	:= FILEMODE =	0555
$(ROOTUSRSBIN)/shutdown		:= FILEMODE =	0755

$(ETCDFSD)/% : %
	$(INS.file)

.KEEP_STATE:

all: $(ALL) all_init.d $(TXTS)

$(SYMSBINF):
	$(RM) $@; $(SYMLINK) ../sbin/$(@F) $@

$(SYMUSRSBINF):
	$(RM) $@; $(SYMLINK) ../usr/sbin/$(@F) $@

$(ROOTSBINL):	$(ROOTSBIN)/rc0
	$(RM) $@; $(LN) $(ROOTSBIN)/rc0 $@

all_init.d: FRC
	@cd init.d; pwd; $(MAKE) $(MFLAGS) all

ins_init.d: FRC
	@cd init.d; pwd; $(MAKE) $(MFLAGS) install

$(SHFILES):
	sh $@.sh $(ROOT)

install: $(ALL) ins_all ins_init.d $(SUBDIRS)

ins_all : $(ROOTSBINF) $(ROOTSBINL) $(ROOTUSRSBINF) $(ROOTETCTABS) \
	$(ROOTDFSTAB)  $(SYMSBINF) $(SYMUSRSBINF)

# Don't re-install directories already installed by Targetdirs
#$(DIRS):
#	$(INS.dir)

$(SUBDIRS):	FRC
	@cd $@; pwd; ROOT=$(ROOT) CH=$(CH) sh mk.$@.sh

FRC:

clean lint:

include	../Makefile.targ
