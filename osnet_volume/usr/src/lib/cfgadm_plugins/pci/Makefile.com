#
# Copyright (c) 1999, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.2	99/03/26 SMI"
#
# lib/cfgadm_plugins/pci/Makefile.com

LIBRARY= pci.a
VERS= .1
OBJECTS= cfga.o

# include library definitions
include ../../../Makefile.lib

INS.dir.root.sys=       $(INS) -s -d -m $(DIRMODE) $@
$(CH)INS.dir.root.sys=  $(INS) -s -d -m $(DIRMODE) -u root -g sys $@
INS.dir.bin.bin=        $(INS) -s -d -m $(DIRMODE) $@
$(CH)INS.dir.bin.bin=   $(INS) -s -d -m $(DIRMODE) -u bin -g bin $@

USR_LIB_DIR		= $(ROOT)/usr/lib
USR_LIB_DIR_CFGADM	= $(USR_LIB_DIR)/cfgadm
USR_LIB_DIR_CFGADM_64	= $(USR_LIB_DIR_CFGADM)/$(MACH64)

ROOTLIBDIR= $(USR_LIB_DIR_CFGADM)
ROOTLIBDIR64= $(USR_LIB_DIR_CFGADM_64)

MAPFILE=	$(MAPDIR)/mapfile
CLOBBERFILES += $(MAPFILE)

SRCS=		$(OBJECTS:%.o=../common/%.c)

LIBS = $(DYNLIB)

# definitions for lint

LINTFLAGS=	-u
LINTFLAGS64=	-u
LINTOUT=	lint.out
CLEANFILES=	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	-M $(MAPFILE)
LDLIBS +=	-lc -ldevice -ldevinfo

.KEEP_STATE:

all: $(LIBS)

lint:	$(LINTLIB)

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile

# Create target directories
$(USR_LIB_DIR):
	-$(INS.dir.root.sys)

$(USR_LIB_DIR_CFGADM): $(USR_LIB_DIR)
	-$(INS.dir.bin.bin)

$(USR_LIB_DIR_CFGADM_64): $(USR_LIB_DIR_CFGADM)
	-$(INS.dir.bin.bin)

$(USR_LIB_DIR_CFGADM)/%: % $(USR_LIB_DIR_CFGADM)
	-$(INS.file)

$(USR_LIB_DIR_CFGADM_64)/%: % $(USR_LIB_DIR_CFGADM_64)
	-$(INS.file)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
