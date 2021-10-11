#
# @(#)Makefile 1.25 99/05/12 SMI
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/loadkeys/Makefile

PROG= loadkeys dumpkeys
sparc_SUBDIRS= type_4 type_6 type_101
ppc_SUBDIRS= type_101
i386_SUBDIRS= type_6 type_101
SUBDIRS= $($(MACH)_SUBDIRS)
SRCS=	$(PROG:%=%.c)

ROOTHELPER= $(ROOTLIB)/set_keyboard_layout
sparc_EXTRA_INSTALL_TARGETS=
i386_EXTRA_INSTALL_TARGETS=$(ROOTHELPER)
EXTRA_INSTALL_TARGETS= $($(MACH)_EXTRA_INSTALL_TARGETS)

include ../Makefile.cmd

.KEEP_STATE:

.PARALLEL:	$(SUBDIRS)

all:  $(PROG) $(SUBDIRS)

install: $(PROG) $(ROOTPROG) $(SUBDIRS) $(EXTRA_INSTALL_TARGETS)

$(ROOTLIB)/%: %
	$(INS.file)

# explicit yacc work for the NSE
#
loadkeys.c: loadkeys.y
	$(YACC.y) loadkeys.y
	mv y.tab.c $@

loadkeys: loadkeys.c
	$(LINK.c) -o $@ loadkeys.c $(LDLIBS)
	$(POST_PROCESS)

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=          TARGET= catalog

clobber:	$(SUBDIRS)
	$(RM) $(CLOBBERFILES)

clean:	$(SUBDIRS)
	$(RM) loadkeys.c

lint: $(SUBDIRS)
	$(LINT.c) $(SRCS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
