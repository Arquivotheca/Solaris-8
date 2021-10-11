#
# uts/i86pc/isa/Makefile
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.9	99/03/04 SMI"
#
#	This makefile drives the production of the ISA nexus driver
#
#	i86pc implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= isa
OBJECTS		= $(ISANEXUS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ISANEXUS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
# lint pass one enforcement
#
CFLAGS += -v

#
# Define our version of dma_engine and i8237A interfaces
#
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_init=$(MODULE)_i_dmae_init
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_acquire=$(MODULE)_i_dmae_acquire
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_free=$(MODULE)_i_dmae_free
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_get_best_mode=$(MODULE)_i_dmae_get_best_mode
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -D_dmae_nxcookie=$(MODULE)__dmae_nxcookie
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_prog=$(MODULE)_i_dmae_prog
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_swsetup=$(MODULE)_i_dmae_swsetup
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_swsetup=$(MODULE)_i_dmae_swsetup
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_swstart=$(MODULE)_i_dmae_swstart
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_stop=$(MODULE)_i_dmae_stop
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_enable=$(MODULE)_i_dmae_enable
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_disable=$(MODULE)_i_dmae_disable
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Di_dmae_get_chan_stat=$(MODULE)_i_dmae_get_chan_stat
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_init=$(MODULE)_d37A_init
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_dma_valid=$(MODULE)_d37A_dma_valid
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_dma_release=$(MODULE)_d37A_dma_release
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_dma_disable=$(MODULE)_d37A_dma_disable
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_dma_enable=$(MODULE)_d37A_dma_enable
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_get_best_mode=$(MODULE)_d37A_get_best_mode
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_intr=$(MODULE)_d37A_intr
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -DdEISA_setchain=$(MODULE)_dEISA_setchain
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_prog_chan=$(MODULE)_d37A_prog_chan
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_dma_swsetup=$(MODULE)_d37A_dma_swsetup
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_dma_swstart=$(MODULE)_d37A_dma_swstart
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_dma_stop=$(MODULE)_d37A_dma_stop
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_get_chan_stat=$(MODULE)_d37A_get_chan_stat
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_set_mode=$(MODULE)_d37A_set_mode
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_write_addr=$(MODULE)_d37A_write_addr
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_read_addr=$(MODULE)_d37A_read_addr
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_write_count=$(MODULE)_d37A_write_count
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dd37A_read_count=$(MODULE)_d37A_read_count

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
include $(UTSBASE)/i86pc/Makefile.targ
