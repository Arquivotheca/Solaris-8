#
# uts/sparc/ohci/Makefile
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	99/09/24 SMI"
#
#	This makefile drives the production of the ohci driver kernel module.
#
#	sparc implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= ohci 
OBJECTS		= $(OHCI_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(OHCI_OBJS:%.o=$(LINTS_DIR)/%.ln)
WARLOCK_OUT	= $(OHCI_OBJS:%.o=%.ll)
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
WARLOCK_OBJECTS = $(OHCI_OBJS:%.o=%.ll)
USBA_FILES = $(USBA_OBJS:%.o= -l ../usba/%.ll)

warlock: $(MODULE).ok $(WARLOCK_OBJECTS)

%.wlcmd:
	$(TEST) -f $@ || $(SCCS) get $@

ohci.ok: $(WARLOCK_OBJECTS) ohci.wlcmd warlock_ddi.files usba.files
	$(WARLOCK) -c ohci.wlcmd  $(WARLOCK_OBJECTS) \
	$(USBA_FILES) \
	-l ../warlock/ddi_dki_impl.ll
	$(TOUCH) $@

%.ll: $(UTSBASE)/common/io/usb/hcd/openhci/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -DDEBUG -o $@ $<

warlock_ddi.files:
	@cd ../warlock; pwd; $(MAKE) warlock

usba.files:
	@cd ../usba; pwd; $(MAKE) warlock
