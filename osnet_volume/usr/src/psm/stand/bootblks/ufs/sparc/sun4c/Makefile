#
#ident	"@(#)Makefile	1.7	99/10/04 SMI"
#
# Copyright (c) 1994, 1999, by Sun Microsystems, Inc.
# All rights reserved.
#
# psm/stand/bootblks/ufs/sparc/sun4c/Makefile
#

#
# Platform specific Makefile for the boot block.
#
# PLATFORM is the target for the binary installation.
#
# PLATLINKS is a list of platforms which are to be linked to this binary
#	at the file level.
#
# PROM_TYPE is the type of prom OBP, IEEE1275, etc...
#
BASEDIR		= ../../..
PLATFORM	= sun4c
PLATLINKS	=
PROM_TYPE	= OBPDEP

include $(BASEDIR)/ufs/Makefile.ufs
include $(BASEDIR)/obp-c/Makefile.rules

CPPINCS		+= -I$(SRC)/uts/common
CPPINCS		+= -I$(SRC)/uts/sun
CPPINCS		+= -I$(SRC)/uts/sparc

.KEEP_STATE:

all:	$(MKBOOT) $(PROG).elf
	./$(MKBOOT) $(PROG).elf $(PROG); chmod -x $(PROG)

include $(BASEDIR)/obp-c/Makefile.targ
include $(BASEDIR)/Makefile.targ
