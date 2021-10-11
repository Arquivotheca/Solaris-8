#
#pragma ident	"@(#)Makefile.com	1.2	99/10/07 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libc_psr/spec/Makefile.com

include $(SRC)/Makefile.psm

MODULE=		abi

#
# links in /usr/platform
#
LINKED_PLATFORMS	= SUNW,Ultra-1
LINKED_PLATFORMS	+= SUNW,Ultra-4
LINKED_PLATFORMS	+= SUNW,Ultra-250
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise-10000

LINKED_DIRS		= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%)
LINKED_LIB_DIRS		= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib)
LINKED_ABI_DIRS		= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/abi)

INS.slink6=	$(RM) -r $@; $(SYMLINK) ../../$(PLATFORM)/lib/$(MODULE) $@ $(CHOWNLINK) $(CHGRPLINK)

links:	$(LINKED_DIRS) $(LINKED_LIB_DIRS) $(LINKED_ABI_DIRS)

$(LINKED_ABI_DIRS):	$(LINKED_LIB_DIRS)
	-$(INS.slink6)

FRC:
