#
# uts/sparc/hid/Makefile

# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/09/24 SMI"
#
#	This makefile drives the production of the hid kernel driver.
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
MODULE		= hid
OBJECTS		= $(HID_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(HID_OBJS:%.o=$(LINTS_DIR)/%.ln)
WARLOCK_OUT	= $(HID_OBJS:%.o=%.ll)
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
LDFLAGS         += -dy -Nmisc/usba -Nmisc/hidparser

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
HID_FILES	= $(MODULE).ll
USBA_FILES = $(USBA_OBJS:%.o=../usba/%.ll)
OHCI_FILES = $(OHCI_OBJS:%.o=../ohci/%.ll)

warlock: $(MODULE).ok warlock_with_usba

%.wlcmd:
	$(TEST) -f $@ || $(SCCS) get $@

hid.ok: hid.ll hid.wlcmd warlock_ddi.files
	$(WARLOCK) -c hid.wlcmd $(HID_FILES)  \
		-l ../warlock/ddi_dki_impl.ll
	$(TOUCH) $@

%.ll: $(UTSBASE)/common/io/usb/clients/hid/%.c \
		$(UTSBASE)/common/sys/usb/clients/hid/hidvar.h
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -DDEBUG -o $@ $<

warlock_with_usba: hid.wlcmd  $(HID_FILES) usba_files \
	ohci_files warlock_ddi.files
	$(WARLOCK) -c ./hid_with_usba.wlcmd \
		$(USBA_FILES) $(OHCI_FILES) \
		$(HID_FILES) \
	-l ../warlock/ddi_dki_impl.ll

usba_files:
	@cd ../usba;pwd; $(MAKE) warlock

ohci_files:
	@cd ../ohci;pwd; $(MAKE) warlock

warlock_ddi.files:
	cd ../warlock; pwd; $(MAKE) warlock

