#
# uts/sparc/dada/Makefile
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#

#ident "@(#)Makefile	1.5	98/10/11 SMI"

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= dada
OBJECTS		= $(DADA_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(DADA_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

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
#	Overrides.
#
CFLAGS		+= -v

#
#	Default build targets.
#
.KEEP_STATE:

all:		$(ALL_DEPS)

def:		$(DEF_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS) lint64

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

#
#	Defines for local commands.
#
WLCC		= wlcc
TOUCH		= touch

#
#	Warlock targets
#
WARLOCK_OBJECTS	= $(DADA_OBJS:%.o=%.ll)

warlock: $(MODULE).ok $(WARLOCK_OBJECTS)

%.ok: $(WARLOCK_OBJECTS)
	$(TOUCH) $@

%.ll: $(UTSBASE)/sun/io/dada/impl/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<

%.ll: $(UTSBASE)/sun/io/dada/conf/%.c
	$(WLCC) $(CFLAGS) $(CPPFLAGS) -o $@ $<
