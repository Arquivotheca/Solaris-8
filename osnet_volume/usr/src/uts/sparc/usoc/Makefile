#
# uts/sparc/usoc/Makefile
#

#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#

#ident	"@(#)Makefile	1.1	99/07/21 SMI"

#
#	This makefile drives the production of the usoc driver kernel
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
MODULE		= usoc
OBJECTS		= $(USOC_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(USOC_OBJS:%.o=$(LINTS_DIR)/%.ln)
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
