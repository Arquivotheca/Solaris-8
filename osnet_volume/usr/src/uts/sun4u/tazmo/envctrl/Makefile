#
# uts/sun4u/tazmo/envctrl/Makefile
# Copyright (c) 1999 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile 1.3	99/02/09 SMI"
#
#	This makefile drives the production of the envctrl driver kernel
#	module.
#
#	sun4u implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../../..

#
#	Define the module and object file sets.
#
MODULE		= envctrl
OBJECTS		= $(ENVCTRL_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ENVCTRL_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_TAZMO_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/tazmo/Makefile.tazmo

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
# Turn on doubleword alignment for 64 bit registers
#
CFLAGS += -dalign

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

LINT_LIB_DIR    = $(TAZMO_LINT_LIB_DIR)
#
#	Include common targets.
#
include $(UTSBASE)/sun4u/tazmo/Makefile.targ
