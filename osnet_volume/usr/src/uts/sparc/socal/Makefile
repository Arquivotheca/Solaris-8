#
# uts/sparc/socal/Makefile

# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

#ident "@(#)Makefile	1.5	98/10/11 SMI"

#
#	This makefile drives the production of the socal driver kernel
#	module, which is used for the Photon/FC-AL product
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
MODULE		= socal
OBJECTS		= $(SOCAL_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SOCAL_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Overrides
#
CFLAGS		+= -v -dalign

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(ALL_DEPS)

def:		$(DEF_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

browser:	$(BINARY)

browser		:= CFLAGS += -xsb
