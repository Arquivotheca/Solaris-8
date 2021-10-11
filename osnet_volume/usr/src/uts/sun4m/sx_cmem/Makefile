#
# uts/sparc/sx_cmem/Makefile
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
#ident  "@(#)Makefile 1.6    97/11/12 SMI" 
#
#	This makefile drives the production of the SX contiguous memory driver
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= sx_cmem
OBJECTS		= $(SX_CMEM_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SX_CMEM_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/sun4m/io

#
#	Include sun4m rules.
#
include $(UTSBASE)/sun4m/Makefile.sun4m

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

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
