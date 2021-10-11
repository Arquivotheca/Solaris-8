#
#ident	"@(#)Makefile	1.56	99/10/10 SMI"
#
# Copyright (c) 1995-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/fs.d/Makefile

# The filesystem independent utilities clri, fsdb, dcopy, labelit, and mkfs
# are all built from the source file switchout.c. They are all then links
# to the same object. This is accomplished by:
#	1) building clri from switchout.c (had to choose one)
#	2) installing it in the target directory
#	3) linking the others to clri.
# In a similar manner, ncheck is linked to ff.

STATPROG=	mount umount
DFPROG=		df
PROG=		$(STATPROG) $(DFPROG) fsck volcopy ff
XPG4PROG=	df
SPPROG=		clri
MNTTAB=		mnttab
FSTYPE=		fs


# install rule for static versions
# deliberately ahead of a similar rule in ../Makefile.cmd
#
ROOT=		/proto
$(ROOT)/sbin/%: %.static
	$(INS.rename)

include ../Makefile.cmd

SUBDIR1= lofs
SUBDIR2= fd pcfs nfs hsfs proc udfs ufs tmpfs cachefs autofs mntfs
i386_SUBDIRS= s5fs xmemfs
i386_I18NDIRS= xmemfs
SUBDIRS= $(SUBDIR1) $(SUBDIR2) $($(MACH)_SUBDIRS)
ALL_SUBDIRS= $(SUBDIR1) $(SUBDIR2) $(i386_SUBDIRS)
I18NDIRS= $(SUBDIR2) $(i386_I18NDIRS)

CLOBBERFILES += $(STATIC) $(POFILES_XPG4)

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=          TARGET= catalog

SBINF=		umount mount
USRSBINF=	umount mount df clri fsck volcopy ff
USRSBINCLRI=	dcopy fsdb labelit mkfs
USRSBINFF=	ncheck

ETC2USRSBIN=	umount mount clri fsdb mkfs fsck labelit dcopy volcopy ff ncheck
USRBIN2USRSBIN=	df

ROOTSBINF=		$(SBINF:%=$(ROOTSBIN)/%)
ROOTUSRSBINF=		$(USRSBINF:%=$(ROOTUSRSBIN)/%)
ROOTUSRSBINCLRI=	$(USRSBINCLRI:%=$(ROOTUSRSBIN)/%)
ROOTUSRSBINFF=		$(USRSBINFF:%=$(ROOTUSRSBIN)/%)
ROOTETCMNTTAB=		$(MNTTAB:%=$(ROOTETC)/%)
ROOTFSTYPE=		$(ROOTETC)/default/$(FSTYPE)
SYMETC2USRSBIN	=	$(ETC2USRSBIN:%=$(ROOTETC)/%)
SYMUSRBIN2USRSBIN=	$(USRBIN2USRSBIN:%=$(ROOTBIN)/%)
SYMDEVNM=		$(ROOTUSRSBIN)/devnm

# This flag is being added only for SCO (x86) compatibility  
$(DFPROG) := CFLAGS += $(iBCS2FLAG)

CPPFLAGS += -D_LARGEFILE64_SOURCE
%.xpg4.o := CPPFLAGS += -DXPG4
$(XPG4) := LDLIBS += -lcmd
$(SPPROG) fsck mount mount.static df ff volcopy :=	LDLIBS += -lcmd

$(ROOTETCMNTTAB) := FILEMODE = 444
$(ROOTETCMNTTAB) := OWNER = root
$(ROOTETCMNTTAB) := GROUP = root
$(ROOTFSTYPE)	 := FILEMODE = 444

# for messaging catalog
#
POFILE= fs.d.po
POFILES1= $(PROG:%=%.po) switchout.po
POFILES2= $(I18NDIRS:%=%/%.po)
POFILES_XPG4 = df.po.xpg4
POFILES=  $(POFILES1) $(POFILES2) $(POFILES_XPG4)
$(POFILES_XPG4) := CFLAGS += -DXPG4
volcopy.po :=   XGETFLAGS += -a -x volcopy.xcl
$(POFILES_XPG4) :=	XGETFLAGS += -a -x df.xcl
$(DFPROG).po := XGETFLAGS += -a -x df.xcl

%.po.xpg4:	%.c
	$(COMPILE.cpp) $< > $<.i
	$(BUILD.po)

# build rule for xpg4 objects
%.xpg4.o: %.c
	$(COMPILE.c) -o $@ $<

.KEEP_STATE:

# This is too intense when building the whole world.
# .PARALLEL:	$(SUBDIRS)

all:		$(SUBDIRS) .WAIT all_local 

_msg: $(I18NDIRS) $(POFILES1) $(POFILES_XPG4)
	$(RM) $(POFILE)
	cat $(POFILES) > $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	cp $(POFILE) $(MSGDOMAIN)

all_local:	$(PROG) $(XPG4PROG) $(STATIC) $(SPPROG) $(MNTTAB) $(FSTYPE) 

ff volcopy: deffs.o $$(@F).o
		$(LINK.c) -o $@ $@.o deffs.o $(LDLIBS)
		$(POST_PROCESS)

df df.xpg4: deffs.o fslib.o $$(@F).o
		$(LINK.c) -o $@ $@.o deffs.o fslib.o $(LDLIBS)
		$(POST_PROCESS)

fsck: fsck.o deffs.o preenlib.o
	$(LINK.c) -o $@ fsck.o deffs.o preenlib.o $(LDLIBS) -ldl
	$(POST_PROCESS)

mount mount.static: deffs.o mount.o fslib.o
		$(LINK.c) -o $@ mount.o deffs.o fslib.o $(LDLIBS)
		$(POST_PROCESS)

umount umount.static: umount.o fslib.o
		$(LINK.c) -o $@ umount.o fslib.o $(LDLIBS)
		$(POST_PROCESS)

$(SPPROG):	switchout.o deffs.o
	$(LINK.c) -o $@ switchout.o deffs.o $(LDLIBS)
	$(POST_PROCESS)

install: $(SUBDIRS) .WAIT install_local

install_local:	all_local $(ROOTSBINF) $(ROOTUSRSBINF) $(ROOTUSRSBINCLRI) \
		$(ROOTUSRSBINFF) $(ROOTETCMNTTAB) $(ROOTFSTYPE) \
		$(ROOTXPG4PROG) $(SYMETC2USRSBIN) $(SYMUSRBIN2USRSBIN) \
		$(SYMDEVNM)

$(ROOTETC)/default/%:%
		$(INS.file)

# Links from /etc to /usr/sbin such as /etc/clri -> ../usr/sbin/clri
$(SYMETC2USRSBIN):
	-$(RM) $@; $(SYMLINK) ../usr/sbin/$(@F) $@

# Links from /usr/bin to /usr/sbin such as /usr/bin/df -> ../sbin/df
$(SYMUSRBIN2USRSBIN):
	-$(RM) $@; $(SYMLINK) ../sbin/$(@F) $@

# Symlink from devnm to df in /usr/sbin
$(SYMDEVNM):
	-$(RM) $@; $(SYMLINK) ./df $@

# Multiple names for switchout (clri, dcopy, fsdb, labelit, mkfs)
$(ROOTUSRSBINCLRI):	$(ROOTUSRSBIN)/clri
	-$(RM) $@; $(SYMLINK) ./clri $@

$(MNTTAB):
	touch $(MNTTAB)

$(FSTYPE):
	@$(RM) $@; $(ECHO) "LOCAL=ufs" >$@

# Multiple names for ff (ncheck)
$(ROOTUSRSBINFF):	$(ROOTUSRSBIN)/ff
	-$(RM) $@; $(SYMLINK) ./ff $@

clean: $(ALL_SUBDIRS) .WAIT clean_local

clean_local:

clobber: $(ALL_SUBDIRS) .WAIT clobber_local

clobber_local:	clean_local
	$(RM) $(PROG) $(SPPROG) $(MNTTAB) $(FSTYPE) $(CLOBBERFILES)

lint:

$(ALL_SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(MFLAGS) $(TARGET)

FRC:
