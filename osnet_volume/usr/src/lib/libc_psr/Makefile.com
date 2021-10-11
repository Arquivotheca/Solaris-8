#
#ident	"@(#)Makefile.com 1.7 99/09/12 SMI"
#
# Copyright (c) 1994,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libc_psr/Makefile.com
#

#
#	Create default so empty rules don't
#	confuse make
#
CLASS		= 32

LIBRARY		= libc_psr.a
VERS		= .1

include $(SRCDIR)/../Makefile.lib
include $(SRCDIR)/../../Makefile.psm

LIBS		= $(DYNLIB)
IFLAGS		= -I$(SRCDIR)/../libc/inc -I$(SRC)/uts/$(PLATFORM) \
		  -I$(ROOT)/usr/platform/$(PLATFORM)/include
CPPFLAGS	= -D_REENTRANT -D$(MACH) $(IFLAGS) $(CPPFLAGS.master)
ASDEFS		= -D__STDC__ -D_ASM $(CPPFLAGS)
ASFLAGS		= -P $(ASDEFS)

#
# Used when building links in /usr/platform/$(PLATFORM)/lib
#
LINKED_PLATFORMS	= SUNW,Ultra-1
LINKED_PLATFORMS	+= SUNW,Ultra-4
LINKED_PLATFORMS	+= SUNW,Ultra-250
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise-10000

#
# install rule
#
$(USR_PSM_LIB_DIR)/%: % $(USR_PSM_LIB_DIR)
	$(INS.file)

#
# build rules
#
pics/%.o: $(SRCDIR)/$(MACH)/$(PLATFORM)/%.s
	$(AS) $(ASFLAGS) $< -o $@
	$(POST_PROCESS_O)
