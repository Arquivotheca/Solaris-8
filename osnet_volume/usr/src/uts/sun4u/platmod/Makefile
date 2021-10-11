#
# uts/sun4u/platmod/Makefile
# Copyright (c) 1995 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.3	98/06/26 SMI"
#
#	This makefile drives the production of the sun4u default platform module.
#
#	sun4u implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= platmod
OBJECTS		= $(PLATMOD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PLATMOD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_PLAT_DIR)/$(MODULE)

PLAT_DIR	= .

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/Makefile.sun4u

#
#	Override defaults
#
CLEANFILES	+= $(PLATLIB)

#
#	Define targets
#
ALL_TARGET	= $(PLATLIB)
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

$(PLATLIB):	$(BINARY)
	$(LD) -o $(PLATLIB) -G $(BINARY) -h misc/platmod

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/Makefile.targ
