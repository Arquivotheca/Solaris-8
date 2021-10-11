#
# uts/intel/ia32/Makefile
# Copyright (c) 1991-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.13	99/05/04 SMI"
#
#	This makefile drives the production of all implementation architecture
#	independent modules for the Intel ia32 processor.

UTSBASE = ../..

include Makefile.ia32

KMODS_BUTFOR_MMU32	= $(KMODS:%mmu32=)
KMODS_BUTFOR_MMU36	= $(KMODS:%mmu36=)
LINT_LIBS_MMU32	 = $(LINT_LIB) \
		   $(KMODS_BUTFOR_MMU36:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(SVVS_KMODS:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(XMODS:%=$(LINT_LIB_DIR)/llib-l%.ln)

LINT_LIBS_MMU36	 = $(LINT_LIB) \
		   $(KMODS_BUTFOR_MMU32:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(SVVS_KMODS:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(XMODS:%=$(LINT_LIB_DIR)/llib-l%.ln)

#
#
#
def		:=	TARGET= def
all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
modlintlib	:=	TARGET= modlintlib
clean.lint	:=	TARGET= clean.lint
check		:=	TARGET= check
install_h	:=	TARGET= install_h

.KEEP_STATE:

.PARALLEL:	$(KMODS) $(SVVS) $(XMODS)

def all install clean clobber: $(KMODS) $(SVVS) $(XMODS)

modlintlib clean.lint: $(KMODS) $(SVVS) $(XMODS)

$(KMODS):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(SVVS) $(XMODS):	FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

install_h check:	FRC

#
#	Full kernel lint target.
#
LINT_TARGET	= globallint

globallint:
	@-$(ECHO) "\nFULL KERNEL: global crosschecks with mmu32:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS_MMU32) 2>&1 | $(LGREP.2)
	@-$(ECHO) "\nFULL KERNEL: global crosschecks with mmu36:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS_MMU36) 2>&1 | $(LGREP.2)

lint:	modlintlib $(LINT_DEPS)

include ../Makefile.targ
