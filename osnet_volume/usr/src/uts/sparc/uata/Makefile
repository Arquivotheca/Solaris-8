#
# uts/sparc/uata/Makefile
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

#ident	"@(#)Makefile	1.9	98/10/15 SMI"

#
#	This makefile drives the production of the ata driver kernel module.
#
#	Sparc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= uata
OBJECTS		= $(ATA_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ATA_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc
#
#       Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Overrides.
#
CLOBBERFILES	+= $(MODULE)
CFLAGS		+= -v
#
#       Default build targets.
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
include $(UTSBASE)/sparc/Makefile.targ

#
#       Defines for local commands.
#
WARLOCK		= warlock
WLCC            = wlcc
TOUCH           = touch
SCCS		= sccs
TEST		= test

#
#       Warlock targets
#
# DADA_FILES = $(DADA_OBJS:%.o=-l ../dada/%.ll)

SCSI_FILES = $(SCSI_OBJS:%.o= -l ../scsi/%.ll)

WARLOCK_OBJECTS = $(ATA_OBJS:%.o=%.ll)


warlock:	$(MODULE).ok $(WARLOCK_OBJECTS) uata.wlcmd

%.ok:	$(WARLOCK_OBJECTS) uata.wlcmd warlock_ddi.files scsi.files
	$(WARLOCK) -c ./uata.wlcmd $(WARLOCK_OBJECTS) \
	../warlock/scsi.ll \
	-l ../warlock/ddi_dki_impl.ll \
	$(SCSI_FILES)
	$(TOUCH) $@

%.wlcmd:
	$(TEST) -f uata.wlcmd  || $(SCCS) get uata.wlcmd

%.ll:	$(UTSBASE)/sun/io/dada/adapters/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

%.ll:	$(UTSBASE)/sun/io/dada/adapters/ghd/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

scsi.files:
	@cd ../scsi; pwd; $(MAKE) warlock

warlock_ddi.files:
	@cd ../warlock; pwd; $(MAKE) warlock
