#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/11/08 SMI"
#
# lib/libprtdiag_psr/sparc/desktop/Makefile

UTSBASE = ../../../../uts

PLATFORM_OBJECTS= desktop.o

include ../Makefile.com

IFLAGS = -I$(USR_PLAT_DIR)/sun4u/include -I ../../../libprtdiag/inc
LINTFLAGS += $(IFLAGS)

#
# desktop platform library should install into
# SUNW,Ultra-1. All other desktop platforms can
# link to /usr/platform/SUNW,Ultra-1/lib/libprtdiag_psr.so
#
PLATFORM=SUNW,Ultra-1

#
# For all other sun4u platforms NOT delivered by ON (ie. delivered 
# by SME or external hardware vendors) we add a default library in
# /usr/platform/sun4u/lib so that prtdiag will default to using 
# the desktop library for any of these platforms. (this will be a
# link to ../../SUNW,Ultra-1/lib
#
DEF_PLATFORM=sun4u
USR_DEF_PSM_LIB_DIR=$(USR_PLAT_DIR)/$(DEF_PLATFORM)/lib
USR_DEF_PSM_LIBS=$(LIBS:%=$(USR_DEF_PSM_LIB_DIR)/%)

.KEEP_STATE:

PLATLIBS= $(USR_PLAT_DIR)/$(PLATFORM)/lib/

install:	all $(USR_PSM_LIBS) $(USR_DEF_PSM_LIBS)

#
# install rules for SUNW,Ultra-1/lib/libprtdiag_psr.so.1
#
$(USR_PSM_LIB_DIR)/%: % $(USR_PSM_LIB_DIR)
	$(INS.file)

$(USR_PSM_LIB_DIR):
	cd $(UTSBASE)/sun4u; make $(USR_PSM_LIB_DIR)

#
# install rules for sun4u/lib/libprtdiag_psr.so.1
#
$(USR_DEF_PSM_LIB_DIR)/%: % $(USR_DEF_PSM_LIB_DIR)
	$(RM) -r $@; \
	$(SYMLINK) ../../$(PLATFORM)/lib/libprtdiag_psr.so.1 $@ $(CHOWNLINK) $(CHGRPLINK)

$(USR_DEF_PSM_LIB_DIR):
	cd $(UTSBASE)/sun4u; make $(USR_DEF_PSM_LIB_DIR)

