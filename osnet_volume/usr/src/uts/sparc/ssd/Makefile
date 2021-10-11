#
# uts/sparc/ssd/Makefile

# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

#ident "@(#)Makefile	1.12	98/10/15 SMI"

#
#	This makefile drives the production of the ssd driver kernel module.
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
MODULE		= ssd
OBJECTS		= $(SSD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SSD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/sun/io/scsi/targets

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
# lint pass one enforcement 
# 
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(ALL_DEPS)

def:		$(DEF_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include	$(UTSBASE)/sparc/Makefile.targ

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

#
# Warlock targets

SSD_FILES   = $(MODULE).ll
SCSI_FILES = $(SCSI_OBJS:%.o=-l ../scsi/%.ll)

warlock: warlock_alone

warlock_alone: $(MODULE).ok

%.wlcmd:
	$(TEST) -f $@ || $(SCCS) get $@

scsi_files:
	@cd ../scsi; pwd; $(MAKE) warlock

ssd.ok: ssd.wlcmd $(SSD_FILES) scsi_files
	$(WARLOCK) -c ./ssd.wlcmd $(SSD_FILES) \
	$(SCSI_FILES) \
	-l ../warlock/ddi_dki_impl.ll
	$(TOUCH) $@

%.ll: $(UTSBASE)/sun/io/scsi/targets/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

warlock_ddi.files:
	@cd ../warlock; pwd; $(MAKE) warlock

scsi.files:
	@cd ../scsi; pwd; $(MAKE) warlock
