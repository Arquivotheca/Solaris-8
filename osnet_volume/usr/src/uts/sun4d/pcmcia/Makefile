#
# uts/sun4d/pcmcia/Makefile
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)Makefile	1.1	99/08/06 SMI"
#
#	This file makes the pcmcia nexus for a sun4d system
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
MODULE		= pcmcia
OBJECTS		= $(PCMCIA_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PCMCIA_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_MISC_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sun4d/Makefile.sun4d

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Override defaults to build a unique, local modstubs.o.
#
MODSTUBS_DIR	 = $(OBJS_DIR)
$(MODSTUBS_O)	:= AS_CPPFLAGS += -DPCMCIA_MODULE
CLEANFILES	+= $(MODSTUBS_O)
DEF_BUILDS	= $(DEF_BUILDS32)
ALL_BUILDS	= $(ALL_BUILDS32)
CLEANLINTFILES	+= $(LINT32_FILES)

#
# lint pass one enforcement
#
CFLAGS += -v

# dependency
LDFLAGS	+=	-dy -Nmisc/busra -Nmisc/cs -Nmisc/cis

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
include $(UTSBASE)/sun4d/Makefile.targ
