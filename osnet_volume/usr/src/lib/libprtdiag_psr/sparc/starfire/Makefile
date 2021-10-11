#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	99/08/24 SMI"
#
# lib/libprtdiag_psr/sparc/starfire/Makefile

UTSBASE = ../../../../uts

PLATFORM_OBJECTS= starfire.o

include ../Makefile.com

IFLAGS = -I$(USR_PLAT_DIR)/sun4u/include -I ../../../libprtdiag/inc
LINTFLAGS += $(IFLAGS)

PLATFORM=SUNW,Ultra-Enterprise-10000

.KEEP_STATE:

PLATLIBS= $(USR_PLAT_DIR)/$(PLATFORM)/lib/

install:	all $(USR_PSM_LIBS)

$(USR_PSM_LIB_DIR):
	cd $(UTSBASE)/sun4u/starfire; make $(USR_PSM_LIB_DIR)

#
# install rule
#
$(USR_PSM_LIB_DIR)/%: % $(USR_PSM_LIB_DIR)
	$(INS.file)

