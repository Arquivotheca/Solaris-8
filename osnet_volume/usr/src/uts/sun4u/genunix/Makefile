#
# uts/sparc/genunix/Makefile
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.13	99/06/16 SMI"
#
#	This makefile drives the production of the generic
#	unix kernel module.
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
MODULE		= genunix
OBJECTS		= $(GENUNIX_OBJS:%=$(OBJS_DIR)/%) \
		  $(NOT_YET_KMODS:%=$(OBJS_DIR)/%)
LINTS		= $(GENUNIX_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(NOT_YET_KMODS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_KERN_DIR)/$(MODULE)

PLATFORM	= sun4u
LIBGEN		= $(OBJS_DIR)/libgenunix.so
LIBSTUBS	= $(GENSTUBS_OBJS:%=$(OBJS_DIR)/%)
SONAME		= $(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/sparc/Makefile.sparc

#
#	Define targets
#
ALL_TARGET	= $(LIBGEN)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Override defaults
#
XARCH_32	= -xarch=v8
ALWAYS_DEFS_32	+= -D__sparcv9cpu
CLEANFILES	+= $(LIBSTUBS) $(LIBGEN)

LINT_LIB_DIR	= $(UTSBASE)/$(PLATFORM)/lint-libs/$(OBJS_DIR)
LINT_LIB	= $(LINT_LIB_DIR)/llib-lunix.ln
GEN_LINT_LIB	=

#
#	Include v9 files
#
include $(UTSBASE)/sparc/v9/Makefile.files

# 
# lint pass one enforcement 
# 
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

.PARALLEL:	$(LIBSTUBS)

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:  	$(INSTALL_DEPS)

$(LIBGEN):	$(BINARY) $(LIBSTUBS)
	$(LD) -o $@ -G -h $(SONAME) $(BINARY) $(LIBSTUBS)

#
#	Include common targets.
#
include $(UTSBASE)/sparc/Makefile.targ

#
#	Include v9 rules.
#
include $(UTSBASE)/sparc/v9/Makefile.rules

#
#	Include sun4u workarounds.
#
include $(UTSBASE)/sun4u/Makefile.workarounds

ALL_DEFS +=	$(WORKAROUND_DEFS)
