#
# uts/sun4m/fd/Makefile
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.9	97/11/12 SMI"
#
#	This makefile drives the production of the fd driver kernel module.
#
#	sun4m implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= fd
OBJECTS		= $(FD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(FD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

ASSYM_H		= $(OBJS_DIR)/fd_assym.h
FDOFF_C		= $(OBJS_DIR)/fdoffsets.c
FDOFF_S		= $(OBJS_DIR)/fdoffsets.s
FDOFF_TMP	= $(OBJS_DIR)/fdoffsets.tmp

#
#	Include common rules.
#
include $(UTSBASE)/sun4m/Makefile.sun4m

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
# Overrides
#
MODSTUBS_DIR	= $(OBJS_DIR)
$(MODSTUBS_O)	:= AS_CPPFLAGS += -DFD_MODULE
$(OBJS_DIR)/fd_asm.o := AS_CPPFLAGS += -I$(OBJS_DIR)
CLEANFILES      += $(MODSTUBS_O) $(ASSYM_H) \
		   $(FDOFF_C) $(FDOFF_S) $(FDOFF_TMP)

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

$(BINARY):      $(ASSYM_H)

#
#	Include common targets.
#
include $(UTSBASE)/sun4m/Makefile.targ

#
#       Assym targets
#
$(ASSYM_H):     $(FDOFFSETS)
	@grep "^#" $(FDOFFSETS) > $(FDOFF_C)
	$(CC) $(CPPFLAGS) -g -S -o $(FDOFF_S) $(FDOFF_C)
	@grep -v "^#" $(FDOFFSETS) > $(FDOFF_TMP)
	$(STABS) -t genassym -m $(MODEL) $(FDOFF_TMP) < $(FDOFF_S) > $(ASSYM_H)
	@rm $(FDOFF_C) $(FDOFF_S) $(FDOFF_TMP)
