#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/sparc/usba/Makefile

#ident	"@(#)Makefile	1.3	99/10/07 SMI"

#
#	This makefile drives the production of the usba kernel module.
#
#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= usba
OBJECTS		= $(USBA_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(USBA_OBJS:%.o=$(LINTS_DIR)/%.ln)
WARLOCK_OUT	= $(USBA_OBJS:%.o=%.ll)
WARLOCK_OK	= $(MODULE).ok
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

# 
# lint pass one enforcement 
# 
CFLAGS += -v

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

clobber:	$(CLOBBER_DEPS)

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
WLCC		= wlcc
TOUCH		= touch
WARLOCK		= warlock

#
#	Warlock targets
#

USBA_FILES = $(USBA_OBJS:%.o=%.ll)

warlock: $(MODULE).ok 

%.ok:	$(USBA_FILES)
	$(TOUCH) $@

%.ll: $(UTSBASE)/common/io/usb/usba/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -DDEBUG -o $@ $<
