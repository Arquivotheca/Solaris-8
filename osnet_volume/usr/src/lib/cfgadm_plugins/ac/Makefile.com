#
# Copyright (c) 1994-1998, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.4	99/08/24 SMI"
#
# lib/ac/Makefile.com

include ../../Makefile.com

PLATFORM=	sun4u
LIBRARY= ac.a
VERS= .1

OBJECTS= mema.o mema_prom.o mema_test.o mema_test_config.o mema_test_subr.o \
	 mema_util.o

# include library definitions
include ../../../Makefile.lib

INS.dir.root.sys=	$(INS) -s -d -m $(DIRMODE) $@
$(CH)INS.dir.root.sys=	$(INS) -s -d -m $(DIRMODE) -u root -g sys $@
INS.dir.root.bin=	$(INS) -s -d -m $(DIRMODE) $@
$(CH)INS.dir.root.bin=	$(INS) -s -d -m $(DIRMODE) -u root -g bin $@

USR_PLAT_DIR		= $(ROOT)/usr/platform
USR_PSM_DIR		= $(USR_PLAT_DIR)/sun4u
USR_PSM_LIB_DIR		= $(USR_PSM_DIR)/lib
USR_PSM_LIB_CFG_DIR	= $(USR_PSM_LIB_DIR)/cfgadm
USR_PSM_LIB_CFG_DIR_64	= $(USR_PSM_LIB_CFG_DIR)/$(MACH64)

ROOTLIBDIR=     $(USR_PSM_LIB_CFG_DIR)
ROOTLIBDIR64=   $(USR_PSM_LIB_CFG_DIR_64)

MAPFILE=	../common/mapfile-vers
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
LDLIBS +=	-lc

CPPFLAGS +=	-I$(ROOT)/usr/platform/$(PLATFORM)/include

.KEEP_STATE:

all: $(LIBS)

lint:	$(LINTLIB)

$(DYNLIB):	$(MAPFILE)

# Create target directories
$(USR_PSM_DIR):		$(LINKED_DIRS)
	-$(INS.dir.root.sys)

$(USR_PSM_LIB_DIR):	$(USR_PSM_DIR) $(LINKED_LIB_DIRS)
	-$(INS.dir.root.bin)

$(USR_PSM_LIB_CFG_DIR):	$(USR_PSM_LIB_DIR) $(LINKED_CFG_DIRS)
	-$(INS.dir.root.bin)

$(USR_PSM_LIB_CFG_DIR_64):	$(USR_PSM_LIB_CFG_DIR)
	-$(INS.dir.root.bin)

$(USR_PSM_LIB_CFG_DIR)/%: % $(USR_PSM_LIB_CFG_DIR)
	-$(INS.file)

$(USR_PSM_LIB_CFG_DIR_64)/%: % $(USR_PSM_LIB_CFG_DIR_64)
	-$(INS.file)

# include library targets
include ../../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
