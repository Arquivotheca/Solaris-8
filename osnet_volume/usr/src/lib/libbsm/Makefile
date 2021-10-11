#
# Copyright (c) 1988,1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.79	99/10/14 SMI"
#
# lib/libbsm/Makefile
#

include ../../Makefile.master
include ../Makefile.lib

SUBDIRS=	spec .WAIT $(MACH) $(BUILD64) $(MACH64)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
package :=	TARGET= package

.KEEP_STATE:

all clean clobber delete install lint package: $(SUBDIRS)

OBJECTS=	adr.o \
		adrf.o \
		adrm.o \
		au_open.o \
		au_preselect.o \
		au_to.o \
		au_usermask.o \
		audit_allocate.o \
		audit_class.o \
		audit_cron.o \
		audit_crontab.o \
		audit_at.o \
		audit_event.o \
		audit_ftpd.o \
		audit_halt.o \
		audit_inetd.o \
		audit_init.o \
		audit_login.o \
		audit_mountd.o \
		audit_mgrs.o \
		audit_passwd.o \
		audit_reboot.o \
		audit_rexd.o \
		audit_rexecd.o \
		audit_rshd.o \
		audit_settid.o \
		audit_shutdown.o \
		audit_su.o \
		audit_uadmin.o \
		audit_user.o \
		bsm.o \
		generic.o \
		getacinfo.o \
		getauditflags.o \
		getdaent.o \
		getdment.o \
		getfaudflgs.o

COMMONDIR = common

#
# Macros for libbsm header files. These define user-level only interfaces.
#
GENHDRS = audit_uevents.h
HDRS = libbsm.h devices.h
COMMONHDRS =	$(HDRS:%=$(COMMONDIR)/%)
ROOTHDRDIR = 	$(ROOT)/usr/include/bsm
ROOTCHDRS = 	$(HDRS:%=$(ROOTHDRDIR)/%)
ROOTHDRS = 	$(GENHDRS:%=$(ROOTHDRDIR)/%)

CHECKCHDRS =	$(COMMONHDRS:%.h=%.check)
CHECKHDRS =	$(GENHDRS:%.h=%.check)

$(ROOTHDRS) := 	FILEMODE = 0644
$(ROOTCHDRS) :=	FILEMODE = 0644

#
# Macros for libbsm database files. These should probably be installed
# from somewhere else. Until we find that better place, install them
# from here.
#

ROOTETCSECURITY = 	$(ROOT)/etc/security
$(ROOTETCSECURITY) := 	DIRMODE = 0755
$(ROOTETCSECURITY) := 	OWNER = root
$(ROOTETCSECURITY) := 	GROUP = sys

ESFILES =		audit_class audit_event
ESSRC =			$(ESFILES:%=%.txt)
ETCSECURITYFILES =	$(ESFILES:%=$(ROOTETCSECURITY)/%)
$(ETCSECURITYFILES) :=	FILEMODE = 0644
$(ETCSECURITYFILES) :=	OWNER = root
$(ETCSECURITYFILES) :=	GROUP = sys

RESFILES = 		audit_control audit_user
RESSRC =		$(RESFILES:%=%.txt)
ROOTETCSECURITYFILES =	$(RESFILES:%=$(ROOTETCSECURITY)/%)
$(ROOTETCSECURITYFILES) := FILEMODE = 0640
$(ROOTETCSECURITYFILES) := OWNER = root
$(ROOTETCSECURITYFILES) := GROUP = sys

#
# /etc/security/audit/localhost/files is a symbolic link to /var/audit.
# This is provided so that auditreduce will work in the default configuration.
#
RESA=$(ROOTETCSECURITY)/audit
RESAL=$(RESA)/localhost
VARAUDIT=$(ROOT)/var/audit
AUDITDIRS=$(RESA) $(RESAL) $(VARAUDIT)
$(AUDITDIRS) := FILEMODE = 0750
$(AUDITDIRS) := OWNER = root
$(AUDITDIRS) := GROUP = sys

ARSYMLNK=$(RESAL)/files

#
# message catalogue file
#
TEXT_DOMAIN= SUNW_OST_OSLIB
POFILE= libbsm.po
CATALOG=libbsm.po
POFILES= $(OBJECTS:%.o=%.po)

CPPFLAGS += -I$(COMMONDIR) $(CPPFLAGS.master)

.KEEP_STATE:

install: install_data install_dirs

#		$(ROOTUSRLIB) $(ROOTLIBS) $(ROOTLINKS)

install_h: $(ROOTHDRDIR) $(ROOTHDRS) $(ROOTCHDRS)

check:	$(CHECKHDRS) $(CHECKCHDRS)

install_data: $(ESSRC) $(RESSRC) $(ROOTETCSECURITY) $(ETCSECURITYFILES) \
	$(ROOTETCSECURITYFILES)

install_dirs: $(AUDITDIRS) $(ARSYMLNK)

audit_uevents.h: mkhdr.sh audit_event.txt
	sh mkhdr.sh

$(ETCSECURITYFILES) $(ROOTETCSECURITYFILES) $(RESA): $(ETCSECURITY) \
	$(ROOTETCSECURITY)

$(RESAL): $(RESA)

$(ARSYMLNK): $(RESAL)

$(ROOTHDRDIR):
	$(INS.dir)

$(ROOTHDRDIR)/%:%
	$(INS.file)

$(ROOTHDRDIR)/%:$(COMMONDIR)/%
	$(INS.file)

$(AUDITDIRS):
	$(INS.dir)

$(ARSYMLNK): 
	$(RM) $@
	$(SYMLINK) ../../../../var/audit $@

$(ETCSECURITY)/%: %.txt
	$(INS.rename)

$(ROOTETCSECURITY):
	$(INS.dir)

$(ROOTETCSECURITY)/%: %.txt
	$(INS.rename)

%.po: common/%.c
	$(COMPILE.cpp) $<  > $<.i
	$(BUILD.po)

_msg: $(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(CATALOG)
	$(CP) $(POFILE) $(MSGDOMAIN)

catalog: _msg

po_clean:
	$(RM) $(POFILES) $(POFILE)

clobber: po_clean

$(POFILE): .WAIT $(POFILES)
	$(RM) $@
	$(CAT) $(POFILES) > $@

$(MSGDOMAIN):
	$(INS.dir)


spec $(MACH) $(MACH64) :	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
