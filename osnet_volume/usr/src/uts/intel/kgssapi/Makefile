#
# uts/intel/kgssapi/Makefile
#
# Copyright (c) 1991-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.12	99/05/04 SMI"
#
#	This makefile drives the production of the kgssapi kernel module.
#
#	intel implementation architecture dependent
#
#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= kgssapi
OBJECTS		= $(KGSSD_OBJS:%=$(OBJS_DIR)/%)
OBJECTS		+= $(KGSSD_DERIVED_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(KGSSD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)
#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_KGSS_DIR)

#
# lint pass one enforcement
#
CFLAGS += -v


OPTION_DEFS	+= -DNO_STDLIB_H

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
include $(UTSBASE)/intel/Makefile.targ
