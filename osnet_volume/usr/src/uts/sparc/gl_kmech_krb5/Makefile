#
# Copyright (c) 1991-1994,1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.1	99/07/18 SMI"
#
#	This makefile drives the production of the gl_kmech_krb5 kernel module.
#
#	sparc implementation architecture dependent
#
#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= gl_kmech_krb5
OBJECTS		= $(KGSS_KRB5_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(KGSS_KRB5_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_KGSS_DIR)/$(MODULE)

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

OPTION_DEFS	+= -DNO_STDLIB_H -DKRB5_NO_PRIVACY

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

clobber:        $(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

INCLUDE_PATH	+= $(INC_PATH) -Y I,$(UTSBASE)/common \
	-I$(UTSBASE)/common/gssapi \
	-I$(UTSBASE)/common/gssapi/include \
	-I$(UTSBASE)/common/gssapi/mechs/krb5/include 
