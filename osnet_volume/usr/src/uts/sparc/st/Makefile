#
# uts/sparc/st/Makefile
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.29	98/10/15 SMI"
#
#	This makefile drives the production of the st driver kernel module.
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
MODULE		= st
OBJECTS		= $(ST_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ST_OBJS:%.o=$(LINTS_DIR)/%.ln)
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

#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

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
# Note that in warlock_with_{esp,isp} it is important to load st.ll
# before {isp,esp}.ll; the reason is that both have _init/_info/_fini
# and warlock can only handle one extern function by a given name;
# any loaded after the first are ignored.

ST_FILES   = $(MODULE).ll
SCSI_FILES = $(SCSI_OBJS:%.o=-l ../scsi/%.ll)

warlock: warlock_alone warlock_with_esp warlock_with_isp warlock_with_fas \
	warlock_with_glm

warlock_alone: $(MODULE).ok

%.wlcmd:
	$(TEST) -f $@  || $(SCCS) get $@

warlock_with_esp: st_with_esp.wlcmd $(ST_FILES) scsi_files esp_files \
	warlock_ddi.files
	$(WARLOCK) -c ./st_with_esp.wlcmd \
		$(ST_FILES) ../esp/esp $(SCSI_FILES)  \
		-l ../warlock/ddi_dki_impl.ll

warlock_with_fas: st_with_fas.wlcmd $(ST_FILES) scsi_files fas_files \
	warlock_ddi.files
	$(WARLOCK) -c ./st_with_fas.wlcmd \
		$(ST_FILES) \
		../fas/fas ../fas/fas_callbacks \
		$(SCSI_FILES)  \
		-l ../warlock/ddi_dki_impl.ll

warlock_with_isp: st_with_isp.wlcmd $(ST_FILES) scsi_files isp_files \
	warlock_ddi.files
	$(WARLOCK) -c ./st_with_isp.wlcmd \
		$(ST_FILES) ../isp/isp $(SCSI_FILES)  \
		-l ../warlock/ddi_dki_impl.ll

warlock_with_glm: st_with_glm.wlcmd $(ST_FILES) scsi_files glm_files \
	warlock_ddi.files
	$(WARLOCK) -c ./st_with_glm.wlcmd \
		$(ST_FILES) ../glm/glm $(SCSI_FILES) \
		-l ../warlock/ddi_dki_impl.ll

scsi_files:
	@cd ../scsi; pwd; $(MAKE) warlock

esp_files:
	@cd ../esp; pwd; $(MAKE) warlock

fas_files:
	@cd ../fas; pwd; $(MAKE) warlock

isp_files:
	@cd ../isp; pwd; $(MAKE) warlock

glm_files:
	@cd ../glm; pwd; $(MAKE) warlock

st.ok: st.wlcmd $(ST_FILES) scsi_files warlock_ddi.files
	$(WARLOCK) -c ./st.wlcmd $(ST_FILES) $(SCSI_FILES) \
	-l ../warlock/ddi_dki_impl.ll
	$(TOUCH) $@

%.ll: $(UTSBASE)/common/io/scsi/targets/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -DDEBUG -o $@ $<

warlock_ddi.files:
	@cd ../warlock; pwd; $(MAKE) warlock

scsi.files:
	@cd ../scsi; pwd; $(MAKE) warlock
