#
# uts/intel/ia32/krtld/Makefile
# Copyright (c) 1994-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.9	99/07/26 SMI"
#
#	kernel linker/loader makefile.
#
#	ia32 architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../../..

#
#	Define the module and object file sets.
#
MODULE		= krtld
OBJECTS		= $(KRTLD_BOOT_OBJS:%=$(OBJS_DIR)/%) \
		  $(KRTLD_OBJS:%=$(OBJS_DIR)/%)
		  
LINTS		= $(KRTLD_BOOT_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(KRTLD_OBJS:%.o=$(LINTS_DIR)/%.ln)
PICS		= $(KRTLD_BOOT_OBJS)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

INC_PATH	+= -I$(UTSBASE)/common/krtld -I.

#
#	Include common rules.
#
include $(UTSBASE)/intel/ia32/Makefile.ia32

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
CPPFLAGS	+= -DELF_TARGET_386
MAPFILE		= $(UTSBASE)/common/krtld/mapfile
LDFLAGS		= -e _kobj_boot -M$(MAPFILE) -Breduce -znoversion

$(PICS)		:= CFLAGS += -K pic

#
# krtld has its own private copies of these functions
#
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrcmp=$(MODULE)_strcmp
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrncmp=$(MODULE)_strncmp
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrlen=$(MODULE)_strlen
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrcpy=$(MODULE)_strcpy
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrcat=$(MODULE)_strcat
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrchr=$(MODULE)_strchr
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dbzero=$(MODULE)_bzero
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dbcopy=$(MODULE)_bcopy

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

$(BINARY):	$(MAPFILE)

#
#	Include common targets.
#
include $(UTSBASE)/intel/ia32/Makefile.targ
