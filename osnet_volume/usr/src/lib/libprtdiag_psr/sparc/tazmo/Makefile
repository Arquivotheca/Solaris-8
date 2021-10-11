#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	99/08/05 SMI"
#
# lib/libprtdiag_psr/sparc/tazmo/Makefile

UTSBASE = ../../../../uts

PLATFORM_OBJECTS= tazmo.o

include ../Makefile.com

IFLAGS = -I$(USR_PLAT_DIR)/sun4u/include -I ../../../libprtdiag/inc
LINTFLAGS += $(IFLAGS)

#
# Workgroup Server platform library should install into
# SUNW,Ultra-4. All other desktop platforms can
# link to /usr/platform/SUNW,Ultra-4/lib/libprtdiag_psr.so
#
PLATFORM=SUNW,Ultra-4

.KEEP_STATE:

PLATLIBS= $(USR_PLAT_DIR)/$(PLATFORM)/lib/

install:	all $(USR_PSM_LIBS)

$(USR_PSM_LIB_DIR):
	cd $(UTSBASE)/sun4u/tazmo; make $(USR_PSM_LIB_DIR)

#
# install rule
#
$(USR_PSM_LIB_DIR)/%: % $(USR_PSM_LIB_DIR)
	$(INS.file)

