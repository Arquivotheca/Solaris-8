#
# uts/i86pc/mmu32/Makefile
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.5	99/05/14 SMI"
#
#	This makefile drives the production of the i86 mmu32 driver module.
#
#	i86 implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..
#
#	Define the module and object file sets.
#
MODULE		= mmu32 
OBJECTS		= $(MMU32_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(MMU32_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_MMU_DIR)/$(MODULE)
MMU_DIR		= .

LIBS		= $(GENLIB) $(MMULIB)

GENUNIX		= genunix
GENUNIX_DIR	= ../../intel/ia32/$(GENUNIX)

MMUOPTS		= -L ./$(OBJS_DIR) -l $(MMUNAME)

LIBOPTS		= -L $(GENUNIX_DIR)/$(OBJS_DIR) -l $(GENUNIX) $(MMUOPTS)

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Override defaults
#
ALL_TARGET	= $(SYM_MOD)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= def $(BINARY) $(ROOTMODULE)
MODSTUBS_O	= $(OBJS_DIR)/modstubs.o
CLEANFILES	+= $(MMULIB) $(SYM_MOD) $(MODSTUBS_O)

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

$(MMULIB):	$(BINARY)
	$(LD) -o $(MMULIB) -G $(BINARY)

$(SYM_MOD):	$(UNIX_O) $(MODSTUBS_O) $(LIBS) $(MMULIB)
	@echo "resolving symbols against unix.o"
	$(LD) -dy -b -o $(SYM_MOD) $(UNIX_O) $(MODSTUBS_O) $(LIBOPTS)


#	Include common targets.
#
include $(UTSBASE)/i86pc/Makefile.targ
