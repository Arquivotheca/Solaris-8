#
#ident	"@(#)Makefile.com 1.1 99/09/23 SMI"
#
# Copyright (c) 1994,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libprtdiag/Makefile.com
#

#
#	Create default so empty rules don't
#	confuse make
#
CLASS		= 32

LIBRARY		= libprtdiag.a
VERS		= .1

include $(SRCDIR)/../Makefile.lib
include $(SRCDIR)/../../Makefile.psm

LIBS		= $(DYNLIB)
IFLAGS		= -I ../../inc -I$(USR_PSM_INCL_DIR)
LDLIBS		+= -lc -lkstat
CFLAGS		+= -v
DYNFLAGS	+= -Wl,-f/usr/platform/\$$PLATFORM/lib/$(DYNLIBPSR)

# removing the -z defs from the options
ZDEFS =

# definitions for lint
LINTFLAGS=      -u $(IFLAGS)
LINTOUT=        lint.out
CLEANFILES=     $(LINTOUT) $(LINTLIB)

#
# install rule
#
$(USR_PSM_LIB_DIR)/%: % $(USR_PSM_LIB_DIR)
	$(INS.file) ;\
	$(RM) -r $(USR_PSM_LIB_DIR)/libprtdiag.so ;\
	$(SYMLINK) ./libprtdiag.so$(VERS) $(USR_PSM_LIB_DIR)/libprtdiag.so
	
#
# build rules
#
objs/%.o pics/%.o:	../../common/%.c
	$(COMPILE.c) $(IFLAGS) -o $@ $<
	$(POST_PROCESS_O)
