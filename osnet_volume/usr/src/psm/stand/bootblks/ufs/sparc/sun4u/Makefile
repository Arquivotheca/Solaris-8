#
#ident	"@(#)Makefile	1.8	99/10/07 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/ufs/sparc/sun4u/Makefile
#

#
# Platform specific Makefile for the boot block.
#
# PLATFORM is the target for the binary installation.
#
# PLATLINKS is a list of platforms which are to be linked to the ../fs
#	directory.
#
BASEDIR		= ../../..
PLATFORM	= sun4u
PLATLINKS	= SUNW,Ultra-1 SUNW,Ultra-250 SUNW,Ultra-4 SUNW,Ultra-Enterprise SUNW,Ultra-Enterprise-10000

LINKED_DIRS             = $(PLATLINKS:%=$(USR_PLAT_DIR)/%)
LINKED_LIB_DIRS         = $(PLATLINKS:%=$(USR_PLAT_DIR)/%/lib)
LINKED_LIB_FS_DIRS	= $(PLATLINKS:%=$(USR_PLAT_DIR)/%/lib/fs)

include $(BASEDIR)/ufs/Makefile.ufs
include $(BASEDIR)/Makefile.1275

.KEEP_STATE:

all:	$(PROG)

include $(BASEDIR)/Makefile.targ
