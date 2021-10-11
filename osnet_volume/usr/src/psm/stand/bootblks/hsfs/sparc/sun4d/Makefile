#
#ident	"@(#)Makefile	1.3	95/06/13 SMI"
#
# Copyright (c) 1994, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/hsfs/sparc/sun4d/Makefile
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

include $(BASEDIR)/hsfs/Makefile.hsfs
include $(BASEDIR)/Makefile.obp

.KEEP_STATE:

all:	$(PROG)

include $(BASEDIR)/Makefile.targ
