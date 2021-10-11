#
# uts/intel/vgatext/Makefile
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.9	99/05/04 SMI"
#
#	This makefile drives the production of the VGA text driver
#
#	intel implementation architecture independent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= vgatext
OBJECTS		= $(VGATEXT_OBJS:%=$(OBJS_DIR)/%) $(OBJS_DIR)/$(VGATEXT_FONT).o
LINTS		= $(VGATEXT_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
VGATEXT_FONT	= 8859-1
VGATEXT_SRC	= $(UTSBASE)/intel/io/vgatext

#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel

#
#	Define targets
#
ALL_TARGET	= $(BINARY) # $(SRC_CONFILE)
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

# Custom targets and rules

$(OBJS_DIR)/$(VGATEXT_FONT).c:	$(VGATEXT_SRC)/$(VGATEXT_FONT).bdf
	awk -f $(VGATEXT_SRC)/bdf_to_c.awk	\
		$(VGATEXT_SRC)/$(VGATEXT_FONT).bdf > $@

$(OBJS_DIR)/$(VGATEXT_FONT).o:	$(OBJS_DIR)/$(VGATEXT_FONT).c
	$(COMPILE.c) -o $@ $<

#
#	Include common targets.
#
include $(UTSBASE)/intel/Makefile.targ

CLEANFILES	+= $(OBJS_DIR)/$(VGATEXT_FONT).c
