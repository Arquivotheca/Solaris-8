#
# uts/sun4u/javelin/envctrltwo/Makefile
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.3	98/07/02 SMI"
#
#	This makefile drives the production of the envctrltwo driver kernel
#	module in the sun4u javelin systems.
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../../..

#
#	Define the module and object file sets.
#
MODULE		= envctrltwo
OBJECTS		= $(ENVCTRLTWO_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ENVCTRLTWO_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_JAVELIN_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/javelin/Makefile.javelin

#
#       Define targets
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

install:  	$(INSTALL_DEPS)

LINT_LIB_DIR	= $(JAVELIN_LINT_LIB_DIR)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/javelin/Makefile.targ
