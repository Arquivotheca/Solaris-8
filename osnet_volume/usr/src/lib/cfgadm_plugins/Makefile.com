#
# Copyright (c) 1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	99/09/22 SMI"
#
# lib/cfgadm_plugins/Makefile.com

include ../../../../Makefile.psm

MODULE =	cfgadm

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
LINKED_CFG_DIRS		= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/cfgadm)

INS.slink6=	$(RM) -r $@; $(SYMLINK) ../../$(PLATFORM)/lib/$(MODULE) $@ $(CHOWNLINK) $(CHGRPLINK)

$(LINKED_DIRS):		$(USR_PLAT_DIR)
	-$(INS.dir.root.sys)

$(LINKED_LIB_DIRS):	$(USR_PLAT_DIR)
	-$(INS.dir.root.sys)

$(LINKED_CFG_DIRS):	$(USR_PLAT_DIR)
	-$(INS.slink6)
