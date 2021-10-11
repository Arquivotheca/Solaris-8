#
# uts/sparc/fp/Makefile
#

#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

#ident	"@(#)Makefile	1.1	99/07/21 SMI"

#
#	This makefile drives the production of Fibre channel Transport
#	layer for the port driver portions
#
#	sparc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= fp
OBJECTS		= $(FP_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(FP_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/sun/io/fibre-channel/impl
WARLOCK_SHELL 	= /bin/ksh

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Overrides
#
CFLAGS		+= -v -dalign

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

browser:	$(BINARY)

browser		:= CFLAGS += -xsb

#
#	Defines for local commands.
#
WARLOCK		= warlock
WLCC		= wlcc
TOUCH		= touch
SCCS		= sccs
TEST		= test
MAKE		= make

FP_FILES = $(MODULE).ll
FCTL_FILES = fctl.ll
WARLOCK_FILES = $(FP_FILES) $(FCTL_FILES)
DDI_DKI_FILES = -l ../warlock/ddi_dki_impl.ll

warlock: $(MODULE).ok

%.wlcmd:
	$(TEST) -f $@  || $(SCCS) get $@

fp.ok: fp.wlcmd $(WARLOCK_FILES)
	cd ../warlock; make
	$(WARLOCK) -c ./fp.wlcmd $(FP_FILES) $(DDI_DKI_FILES)
	$(WARLOCK) -c ./fp_with_fctl.wlcmd $(WARLOCK_FILES) $(DDI_DKI_FILES)
	$(TOUCH) $@

%.ll: $(UTSBASE)/sun/io/fibre-channel/impl/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -DDEBUG -o $@ $<
