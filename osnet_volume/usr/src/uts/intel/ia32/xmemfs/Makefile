#
# uts/i86/xmemfs/Makefile
# Copyright (c) 1999 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.6	97/11/12 SMI"
#
#	This makefile drives the production of the xmemfs file system
#	kernel module.
#
#	i86 architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../../..

#
#	Define the module and object file sets.
#
MODULE		= xmemfs
OBJECTS		= $(XMEMFS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(XMEMFS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_FS_DIR)/$(MODULE)

INC_PATH	+= -I.. -I../../../i86pc

#
#	Include common rules.
#
include $(UTSBASE)/intel/ia32/Makefile.ia32

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Overrides.
#
CFLAGS		+= -v

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
include $(UTSBASE)/intel/ia32/Makefile.targ
