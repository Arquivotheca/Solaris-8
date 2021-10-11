#
# uts/sparc/audiosup/Makefile
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#	This makefile drives the production of the audio support (audiosup)
#	module.
#
#pragma ident	"@(#)Makefile	1.1	99/05/26 SMI"

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE = ../..

#
#	Define the module and object file sets.
#
MODULE		= audiosup
OBJECTS		= $(AUDIO_SUP_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(AUDIO_SUP_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_MISC_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/sun/io

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE_64)

# 
#	Overrides, lint pass one enforcement 
# 
CFLAGS		+= -v

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
include $(UTSBASE)/sparc/Makefile.targ
