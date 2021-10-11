#
# uts/i86pc/pcplusmp/Makefile
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident  "@(#)Makefile 1.16     99/04/13 SMI"
#
#	This makefile drives the production of the pcplusmp "mach"
#	kernel module.
#
#	pcplusmp implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= pcplusmp
OBJECTS		= $(PCPLUSMP_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PCPLUSMP_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MACH_DIR)/$(MODULE)
ITU_SRCDIR	= $(UTSBASE)/i86pc/io/pcplusmp

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(ITUMOD)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_ITUFILE)

DEBUG_FLGS      =
$(NOT_RELEASE_BUILD)DEBUG_DEFS	+= $(DEBUG_FLGS)

CFLAGS		+= -Dbug1157974 -Dbug1155030

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
include $(UTSBASE)/i86pc/Makefile.targ

