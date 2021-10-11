#
# Copyright (c) 1992-1997 Sun Microsystems, Inc.
#
# ident	"@(#)Makefile	1.28	99/10/17	SMI"
#
# cmd/rpcsvc/nis/utils/Makefile
#
# Makefile for the nis_plus utility programs.
#

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint

SUBDIRS= nisaddcred

# links to nischmod
LPROG=	nischgrp  nischown   nischttl

PROG=	niscat    nismatch   nistest    nisdefaults nischmod    nisrm      \
        nisgrep   nismkdir   nisrmdir   nistbladm   nisgrpadm   nisln      \
        nisls     niserror   nispath	nisprefadm

SRCS=	niscat.c  nismatch.c nistest.c  nisdefaults.c nischmod.c  nisrm.c    \
	nisgrep.c nismkdir.c nisrmdir.c nistbladm.c   nisgrpadm.c nisln.c    \
        nisls.c   niserror.c nispath.c	nisprefadm.c

OBJS=	niscat.o  nismatch.o nistest.o  nisdefaults.o nischmod.o  nisrm.o    \
	nisgrep.o nismkdir.o nisrmdir.o nistbladm.o   nisgrpadm.o nisln.o    \
        nisls.o   niserror.o nispath.o	nisprefadm.o

RPROG=  nisaddent nisauthconf
RSRCS=  nisaddent.c nisauthconf.c
ROBJS=  nisaddent.o nisauthconf.o

UTILSRCS= nis_util.c
UTILOBJS= nis_util.o

BACKUP_PROG=	nisbackup
BACKUP_SRCS=	nisbackup.cc ../rpc.nisd/nis_log_common.c
BACKUP_OBJS=	nisbackup.o nis_log_common.o 

RESTORE_PROG=	nisrestore
RESTORE_SRCS=	nisrestore.cc ../rpc.nisd/nis_log_common.c
RESTORE_OBJS=	nisrestore.o nis_log_common.o 

BKRSTPROG=	$(BACKUP_PROG) $(RESTORE_PROG)

DERIVED_FILES=	../rpc.nisd/nis_svc.h

include ../../../Makefile.cmd

ROOTNSLIB=	$(ROOT)/usr/lib/nis
ROOTUSRSBIN=	$(ROOT)/usr/sbin
ROOTNSPROG=	$(RPROG:%=$(ROOTNSLIB)/%)
ROOTLPROG=	$(LPROG:%=$(ROOTBIN)/%)
ROOTUSBPROG=	$(BKRSTPROG:%=$(ROOTUSRSBIN)/%)
CLOBBERFILES +=	$(RPROG) $(LPROG) $(BACKUP_PROG) $(RESTORE_PROG)\
		$(DERIVED_FILES)

PROTOCOL_DIR=	$(ROOT)/usr/include/rpcsvc
CPPFLAGS += -I$(SRC)/lib/libnsl/include
LDLIBS += -lnsl -lgen util.a
nispasswd := LDLIBS = $(LDLIBS.cmd) -lnsl

.KEEP_STATE:

all: $(DERIVED_FILES) util.a $(SUBDIRS) $(PROG) $(RPROG) $(LPROG)\
	nisrestore nisbackup

install: all $(SUBDIRS) $(ROOTPROG) $(ROOTNSPROG) $(ROOTLPROG) $(ROOTUSBPROG)

clean:
	$(RM) $(OBJS) $(ROBJS) $(UTILOBJS) $(BACKUP_OBJS) $(RESTORE_OBJS) util.a

lint: lint_SRCS

nispasswd:
	@$(ECHO) "\`\`nispasswd'' is now a link to \`\`passwd''."

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
# build an archive library so that those programs that need stuff can
# get it, but others won't link in unneeded routines.
#

util.a: $(UTILOBJS)
	$(AR) -crv $@ $(UTILOBJS)

$(PROG:%=%): $$@.o util.a
	$(LINK.c) -o $@ $@.o $(LDLIBS)
	$(POST_PROCESS)

$(LPROG): nischmod
	-$(RM) $@ ; $(LN) nischmod $@

$(ROOTLPROG): $(ROOTBIN)/nischmod
	-$(RM) $@ ; $(LN) $(ROOTBIN)/nischmod $@

$(ROOTNSLIB):
	$(INS.dir)

$(ROOTNSLIB)/%: % $(ROOTNSLIB)
	$(INS.file)

$(RESTORE_PROG): $(DERIVED_FILES) $(RESTORE_OBJS) util.a
	$(LINK.cc) -o $@ $(RESTORE_OBJS) $(LDLIBS) -lnisdb
	$(POST_PROCESS)

$(BACKUP_PROG): $(DERIVED_FILES) $(BACKUP_OBJS) util.a
	$(LINK.cc) -o $@ $(BACKUP_OBJS) $(LDLIBS) -lnisdb
	$(POST_PROCESS)

nis_log_common.o: ../rpc.nisd/nis_log_common.c $(DERIVED_FILES)
	$(COMPILE.c) ../rpc.nisd/nis_log_common.c
	$(POST_PROCESS_O)

#
# Rules for building the derived files :
#
$(DERIVED_FILES): $(PROTOCOL_DIR)/nis.x $(PROTOCOL_DIR)/nis_object.x
	$(RPCGEN) -C -h $(PROTOCOL_DIR)/nis.x |\
	sed -n -e '/EDIT_START/,$$ p' |\
	sed -e 's/_2_svc/_svc/' |\
	sed -e 's/_3_svc/_svc/' |\
	sed -e 's/_3/_clnt/' |\
	sed -e 's/_2/_clnt/' >../rpc.nisd/nis_svc.h

include ../../../Makefile.targ

FRC:
