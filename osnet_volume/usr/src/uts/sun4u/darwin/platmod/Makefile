#
# uts/sun4u/darwin/platmod/Makefile
# Copyright (c) 1999 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.1	99/07/09 SMI"
#
#	This makefile drives the production of the sun4u darwin platform module.
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
MODULE		= platmod
OBJECTS		= $(DARWIN_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(DARWIN_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DARWIN_MISC_DIR)/$(MODULE)

PLAT_DIR	= .
HERE		= ../darwin/platmod

#
#	Include common rules.
#
include $(UTSBASE)/sun4u/darwin/Makefile.darwin

#
#	Override defaults
#
CLEANFILES	+= $(PLATLIB) $(SYM_MOD)

#
#	Define targets
#
ALL_TARGET	= $(SYM_MOD)
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

check:

LINT_LIB_DIR	= $(DARWIN_LINT_LIB_DIR)

$(PLATLIB):	$(BINARY)
	${LD} -o $(PLATLIB) -G $(BINARY)

$(SYM_MOD):	$(UNIX_O) $(PLATLIB)
	@echo "resolving symbols against unix.o"
	@(cd $(UNIX_DIR); pwd; \
	    PLAT_DIR=$(HERE) SYM_MOD=$(HERE)/$(SYM_MOD) $(MAKE) symcheck)

#
#	Include common targets.
#
include $(UTSBASE)/sun4u/darwin/Makefile.targ
