#
# uts/sparc/ses/Makefile
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident  "@(#)Makefile 1.10	99/03/03 SMI"
#
# This makefile drives the production of the ses driver kernel module.
#
# sparc architecture dependent
#

#
# Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
# Define the module and object file sets.
#
MODULE		= ses
OBJECTS		= $(SES_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SES_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/scsi/targets
WARLOCK_TARGETS	= warlock.ok warlock_with_esp.ok warlock_with_fas.ok \
		  warlock_with_isp.ok warlock_with_glm.ok

#
# Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
# Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)
CLEANFILES	+= $(WARLOCK_TARGETS)

#
# Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

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
# Defines for local commands.
#
WARLOCK		= warlock
WLCC		= wlcc
TOUCH		= touch
SCCS		= sccs
TEST		= test

#
# Warlock targets
#
SES_FILES	= $(MODULE).ll
SCSI_FILES	= $(SCSI_OBJS:%.o=-l ../scsi/%.ll)


warlock: $(WARLOCK_TARGETS)

warlock.ok: $(MODULE).wlcmd $(SES_FILES)
	@cd ../warlock; $(MAKE) warlock
	@cd ../scsi; $(MAKE) warlock
	$(WARLOCK) -c ./$(MODULE).wlcmd $(SES_FILES) ../warlock/scsi.ll \
	        -l ../warlock/ddi_dki_impl.ll  \
	        $(SCSI_FILES)
	@ $(TOUCH) $@
 
warlock_with_esp.ok: ses_with_esp.wlcmd $(SES_FILES)
	@cd ../warlock; $(MAKE) warlock
	@cd ../scsi; $(MAKE) warlock
	@cd ../esp; $(MAKE) warlock;
	$(WARLOCK) -c ./ses_with_esp.wlcmd $(SES_FILES) ../esp/esp  \
	        ../warlock/scsi.ll \
	        -l ../warlock/ddi_dki_impl.ll \
	        $(SCSI_FILES)
	@ $(TOUCH) $@

warlock_with_fas.ok: ses_with_fas.wlcmd $(SES_FILES)
	@cd ../warlock; $(MAKE) warlock
	@cd ../scsi; $(MAKE) warlock
	@cd ../fas; $(MAKE) warlock;
	$(WARLOCK) -c ./ses_with_fas.wlcmd $(SES_FILES) \
	        ../fas/fas ../fas/fas_callbacks \
	        ../warlock/scsi.ll \
	        -l ../warlock/ddi_dki_impl.ll \
	        $(SCSI_FILES)
	@ $(TOUCH) $@

warlock_with_isp.ok: ses_with_isp.wlcmd $(SES_FILES)
	@cd ../warlock; $(MAKE) warlock
	@cd ../scsi; $(MAKE) warlock
	@cd ../isp; $(MAKE) warlock;
	$(WARLOCK) -c ./ses_with_isp.wlcmd $(SES_FILES) ../isp/isp \
	        ../warlock/scsi.ll \
	        -l ../warlock/ddi_dki_impl.ll \
	        $(SCSI_FILES)
	@ $(TOUCH) $@

warlock_with_glm.ok: ses_with_glm.wlcmd $(SES_FILES)
	@cd ../warlock; $(MAKE) warlock
	@cd ../scsi; $(MAKE) warlock
	@cd ../glm; $(MAKE) warlock;
	$(WARLOCK) -c ./ses_with_glm.wlcmd $(SES_FILES) ../glm/glm \
	        ../warlock/scsi.ll \
	        -l ../warlock/ddi_dki_impl.ll \
	        $(SCSI_FILES)
	@ $(TOUCH) $@

%.ll: $(UTSBASE)/common/io/scsi/targets/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<


