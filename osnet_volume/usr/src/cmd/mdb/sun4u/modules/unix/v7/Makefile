#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	99/09/16 SMI"

include ../../../../../../Makefile.psm

MODULE = unix.so
MDBTGT = kvm
PLATFORM = sun4u

MODSRCS = unix.c

#
# Used when building links in /usr/platform/$(PLATFORM)/lib/mdb
#
LINKED_PLATFORMS	= SUNW,Ultra-1
LINKED_PLATFORMS	+= SUNW,Ultra-4
LINKED_PLATFORMS	+= SUNW,Ultra-250
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise
LINKED_PLATFORMS	+= SUNW,Ultra-Enterprise-10000

USR_PSM_LIB_MDB_LINKS	= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/mdb)
INS.mdb_slink		= $(RM) -r $@; $(SYMLINK) ../../$(PLATFORM)/lib/mdb $@ $(CHOWNLINK) $(CHGRPLINK)

$(USR_PSM_LIB_MDB_LINKS):
	-$(INS.mdb_slink)

include ../../../../../Makefile.cmd
include ../../../../sparc/Makefile.sparcv7

ROOTMOD = $(ROOT)/usr/platform/sun4u/lib/mdb/$(MDBTGT)

$(ROOT)/usr/platform/sun4u/lib/mdb:
	$(INS.dir)

$(ROOTMOD): $(ROOT)/usr/platform/sun4u/lib/mdb

$(ROOTMOD)/%: % $(USR_PSM_LIB_MDB_LINKS)
	$(INS.file) 

include ../../../../Makefile.module

CPPFLAGS += -DMP -D_MACHDEP
CPPFLAGS += -I../../../../common
CPPFLAGS += -I$(SRC)/uts/sun4u
CPPFLAGS += -I$(SRC)/uts/sfmmu
CPPFLAGS += -I$(SRC)/uts/sparc/v9
