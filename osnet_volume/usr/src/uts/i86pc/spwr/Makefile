#
# uts/i86pc/spwr/Makefile
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma	ident	"@(#)Makefile	1.6	99/04/13 SMI"
#
#	This makefile drives the production of the spwr
#	network driver kernel module.
#
#	i86pc architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= spwr
OBJECTS		= $(SPWR_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SPWR_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/i86pc/io/spwr
ITU_SRCDIR	= $(UTSBASE)/i86pc/io/spwr

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(CONFMOD) $(ITUMOD)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE) $(ROOT_ITUFILE)

DEBUG_FLGS      =
$(NOT_RELEASE_BUILD)DEBUG_DEFS	+= $(DEBUG_FLGS)

CPPFLAGS	+= -DSUNDDI
CPPFLAGS	+= -DM_UNIX -DUNIX=0x400 -DCODE_386 -DETHERNET
CPPFLAGS	+= -DEPIC -DLM9432 -DFREEBUFF

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
