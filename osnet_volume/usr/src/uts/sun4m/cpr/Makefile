#
# uts/sun4m/cpr/Makefile
# Copyright (c) 1991 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.10	97/11/12 SMI"
#
#	This makefile drives the production of the bpp driver kernel module.
#
#	sun4m implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= cpr
OBJECTS		= $(CPR_OBJS:%=$(OBJS_DIR)/%) \
		  $(CPR_IMPL_OBJS:%=$(OBJS_DIR)/%) \
		  $(CPR_SPARC_OBJS:%=$(OBJS_DIR)/%)

LINTS		= $(CPR_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(CPR_IMPL_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(CPR_SPARC_OBJS:%.o=$(LINTS_DIR)/%.ln)

ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sun4m/Makefile.sun4m

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
# lint pass one enforcement
#
CFLAGS += -v

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
include $(UTSBASE)/sun4m/Makefile.targ
