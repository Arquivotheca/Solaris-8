#
# uts/i86pc/io/pcscsi/Makefile
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.12	99/04/13 SMI"
#
#	This makefile drives the production of the pcscsi "drv"
#	kernel module.
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
MODULE		= pcscsi
OBJECTS		= $(PCSCSI_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PCSCSI_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/i86pc/io/dktp/hba/pcscsi
ITU_SRCDIR	= $(UTSBASE)/i86pc/io/dktp/hba/pcscsi

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(CONFMOD) $(ITUMOD)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE) $(ROOT_ITUFILE)

#	DBG and MSG are exclusively for the AMD core code debug facility
DEBUG_FLGS	= -DPCSCSI_DEBUG -DDBG -DMSG
$(NOT_RELEASE_BUILD)DEBUG_DEFS  += $(DEBUG_FLGS)

#       SAVE_DMA_SEG_VIRT_ADDRS affects pcscsi.h, pcscsi.c, pcscsi_dma_impl
#       directly, and portability.c, ggmini_solaris.c indirectly (they
#       include pcscsi.h).
#       SG_ALWAYS affects pcscsi_dma_impl.c only.
#       -WO,-B & -XCC=yes = Accept C++ style comment delimiters (i.e., //)
#       All the defines following the above are used by the AMD core code
#       AND its inclue files (some of which are included in the Solaris src).
#       The defines are the same for everybody because it hurts nothing and
#       reduces maintenance difficulties.
CPPFLAGS	+=     -DSAVE_DMA_SEG_VIRT_ADDRS -DSG_ALWAYS \
		       -DSOLARIS -DOTHER_OS -DISABLE_DIO -DDISABLE_SREG \
		       -DDISABLE_MDL -DCOMPAQ_MACHINE
CFLAGS		+=     -W0,-B
LINTFLAGS	+=     -XCC=yes

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
