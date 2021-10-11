#
# uts/sparc/usb_mid/Makefile

# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#	This makefile drives the production of the usb_mid driver kernel module.
#	sparc architecture dependent
#

#ident	"@(#)Makefile	1.3	99/09/24 SMI"

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE = ../..

#
#	Define the module and object file sets.
#
MODULE		= usb_mid
OBJECTS		= $(USB_MID_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(USB_MID_OBJS:%.o=$(LINTS_DIR)/%.ln)
WARLOCK_OUT	= $(USB_MID_OBJS:%.o=%.ll)
WARLOCK_OK	= $(MODULE).ok
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

# 
# lint pass one enforcement 
# 
CFLAGS += -v

#
# depends on misc/usba
#
LDFLAGS         += -dy -Nmisc/usba

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
USB_MID_FILES	= $(MODULE).ll
USBA_FILES = $(USBA_OBJS:%.o=../usba/%.ll)
OHCI_FILES = $(OHCI_OBJS:%.o=../ohci/%.ll)

warlock: $(MODULE).ok warlock_with_usba

%.wlcmd:
	$(TEST) -f $@ || $(SCCS) get $@

usb_mid.ok: usb_mid.ll usb_mid.wlcmd warlock_ddi.files
	$(WARLOCK) -c usb_mid.wlcmd $(USB_MID_FILES)  \
		-l ../warlock/ddi_dki_impl.ll
	$(TOUCH) $@

%.ll: $(UTSBASE)/common/io/usb/usb_mid/%.c \
		$(UTSBASE)/common/sys/usb/usb_mid/usb_midvar.h
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -DDEBUG -o $@ $<

warlock_with_usba: usb_mid.wlcmd  $(USB_MID_FILES) usba_files \
	ohci_files warlock_ddi.files
	$(WARLOCK) -c ./usb_mid_with_usba.wlcmd \
		$(USBA_FILES) $(OHCI_FILES) \
		$(USB_MID_FILES) \
		-l ../warlock/ddi_dki_impl.ll

usba_files:
	@cd ../usba;pwd; $(MAKE) warlock

ohci_files:
	@cd ../ohci;pwd; $(MAKE) warlock

warlock_ddi.files:
	cd ../warlock; pwd; $(MAKE) warlock
