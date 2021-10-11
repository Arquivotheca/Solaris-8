#
# uts/sparc/dad/Makefile
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#
#	This makefile drives the production of the dad driver kernel module.
#
#	sparc architecture dependent
#

#ident	"@(#)Makefile	1.9	98/10/15 SMI"

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= dad
OBJECTS		= $(DAD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(DAD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/sun/io/dada/targets

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc


#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Overrides.
#
CFLAGS		+= -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(ALL_DEPS)

def:		$(DEF_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS) lint64

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include	$(UTSBASE)/sparc/Makefile.targ


#
#	Defines for local commands.
#
WARLOCK		= warlock
WLCC		= wlcc
TOUCH		= touch
SCCS		= sccs
TEST		= test
MAKE		= make

#
# Warlock targets
#
# Note that in warlock_with_{esp,isp} it is important to load sd.ll
# before {isp,esp}.ll; the reason is that both have _init/_info/_fini
# and warlock can only handle one extern function by a given name;
# any loaded after the first are ignored.

DAD_FILES   = $(MODULE).ll
DADA_FILES = $(DADA_OBJS:%.o=-l ../dada/%.ll)

warlock: warlock_alone

warlock_alone: $(MODULE).ok

%.wlcmd:
	$(TEST) -f $@  || $(SCCS) get $@

dada_files:
	@cd ../dada; pwd; $(MAKE) warlock

dad.ok: dad.wlcmd $(DAD_FILES)  dada_files warlock_ddi.files
	$(WARLOCK) -c ./dad.wlcmd $(DAD_FILES) -l $(DADA_FILES) \
	-l ../warlock/ddi_dki_impl.ll
	$(TOUCH) $@

%.ll: $(UTSBASE)/sun/io/dada/targets/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

warlock_ddi.files:
	@cd ../warlock; pwd; $(MAKE) warlock
