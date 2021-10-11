#
# uts/intel/Makefile
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.4	00/10/13 SMI"
#
#	This makefile drives the production of all implementation architecture
#	independent modules for Intel processors.

UTSBASE = ..

include Makefile.intel

SUBDIRS = ia32
$(BUILD64)SUBDIRS += ia64

LINT_LIBS	= $(LINT_LIB) $(GEN_LINT_LIB) \
		  $(LINT_KMODS:%=$(LINT_LIB_DIR)/llib-l%.ln) 


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

.PARALLEL:	$(KMODS) $(SVVS) $(XMODS) $(SUBDIRS) config

def all install clean clobber: $(KMODS) $(SVVS) $(XMODS) $(SUBDIRS) config

modlintlib clean.lint: $(LINT_KMODS) $(SVVS) $(XMODS) $(SUBDIRS)

$(KMODS) $(SUBDIRS) config $(SVVS) $(XMODS):	FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(TARGET); \
	else \
		true; \
	fi

install_h check:	FRC
	@cd sys; pwd; $(MAKE) $(TARGET)
	@cd ia32/sys; pwd; $(MAKE) $(TARGET)
	@cd ia64/sys; pwd; $(MAKE) $(TARGET)

#
#	Full kernel lint target.
#
LINT_TARGET	= globallint

globallint:
	@-$(ECHO) "\nFULL KERNEL: global crosschecks:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS) 2>&1 | $(LGREP.2)

lint:	modlintlib $(LINT_DEPS)


include ../Makefile.targ
