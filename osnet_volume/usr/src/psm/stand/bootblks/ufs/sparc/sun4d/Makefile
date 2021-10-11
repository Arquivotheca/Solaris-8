#
#ident	"@(#)Makefile	1.6	96/10/15 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/ufs/sparc/sun4d/Makefile
#

#
# Platform specific Makefile for the boot block.
#
# PLATFORM is the target for the binary installation.
#
# PLATFORM_INCS is a list of directories to look in for platform specific
#	header files to include
#
# PLATLINKS is a list of platforms which are to be linked to this binary
#	at the file level.
#
# PROM_TYPE is the type of prom OBP, IEEE1275, etc...
#
BASEDIR		= ../../..
PLATFORM	= sun4d
PLATFORM_INCS	= $(ROOT)/usr/include/v7
PLATLINKS	=
PROM_TYPE	= OBP

include $(BASEDIR)/ufs/Makefile.ufs
include $(BASEDIR)/obp-c/Makefile.rules

CPPINCS		+= -I$(SRC)/uts/common
CPPINCS		+= -I$(SRC)/uts/sun
CPPINCS		+= -I$(SRC)/uts/sparc
CPPINCS		+= -I$(SRC)/uts/$(PLATFORM)
CPPINCS		+= -I$(PLATFORM_INCS)

.KEEP_STATE:

all:	$(MKBOOT) $(PROG).elf
	./$(MKBOOT) $(PROG).elf $(PROG); chmod -x $(PROG)

include $(BASEDIR)/obp-c/Makefile.targ
include $(BASEDIR)/Makefile.targ
