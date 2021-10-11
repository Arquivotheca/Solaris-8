#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/sparc/scsa2usb/Makefile
#
#	This makefile drives the production of the scsa2usb driver
#	kernel module. sparc architecture dependent
#

#pragma ident	"@(#)Makefile	1.2	99/10/22 SMI"

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE = ../..

#
#	Define the module and object file sets.
#
MODULE		= scsa2usb
OBJECTS		= $(SCSA2USB_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SCSA2USB_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
WARLOCK_OUT	= $(SCSA2USB_OBJS:%.o=%.ll)
WARLOCK_OK	= $(MODULE).ok

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

# 
# lint pass one enforcement 
# 
CFLAGS += -v

# depends on misc/usba misc/scsi
#
LDFLAGS		+= -dy -Nmisc/usba -Nmisc/scsi

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

.KEEP_STATE:

all:		$(ALL_DEPS)

def:		$(DEF_DEPS)

clean:		$(CLEAN_DEPS); \
                $(RM) $(WARLOCK_OUT) $(WARLOCK_OK)

clobber:	$(CLOBBER_DEPS); \
		$(RM) $(WARLOCK_OUT) $(WARLOCK_OK)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
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

#
#	lock_lint rules
#
SCSA2USB_FILES	= $(MODULE).ll
USBA_FILES = $(USBA_OBJS:%.o=../usba/%.ll)
OHCI_FILES = $(OHCI_OBJS:%.o=../ohci/%.ll)
SCSI_FILES = $(SCSI_OBJS:%.o= -l ../scsi/%.ll)

warlock: $(MODULE).ok warlock_with_usba

%.wlcmd:
	$(TEST) -f $@ || $(SCCS) get $@

scsa2usb.ok: scsa2usb.ll scsa2usb.wlcmd warlock_ddi.files \
	scsi.files
	$(WARLOCK) -c scsa2usb.wlcmd $(SCSA2USB_FILES)  \
		../warlock/scsi.ll \
		-l ../warlock/ddi_dki_impl.ll \
		$(SCSI_FILES)
	$(TOUCH) $@

%.ll: $(UTSBASE)/common/io/usb/scsa2usb/%.c \
		$(UTSBASE)/common/sys/usb/clients/mass_storage/usb_bulkonly.h \
		$(UTSBASE)/common/sys/usb/scsa2usb/scsa2usb.h
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -DDEBUG -o $@ $<

warlock_with_usba: scsa2usb.wlcmd  $(SCSA2USB_FILES) usba_files \
	ohci_files warlock_ddi.files
	$(WARLOCK) -c ./scsa2usb_with_usba.wlcmd \
		$(USBA_FILES) $(OHCI_FILES) \
		$(SCSA2USB_FILES) \
	-l ../warlock/ddi_dki_impl.ll

usba_files:
	@cd ../usba;pwd; $(MAKE) warlock

ohci_files:
	@cd ../ohci;pwd; $(MAKE) warlock

warlock_ddi.files:
	@cd ../warlock; pwd; $(MAKE) warlock

scsi.files:
	@cd ../scsi; pwd; $(MAKE) warlock
