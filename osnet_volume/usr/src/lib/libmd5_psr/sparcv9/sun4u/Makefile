#
#pragma ident	"@(#)Makefile	1.2	99/09/10 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5_psr/sparcv9/sun4u/Makefile
#
# Platform specific Makefile for libmd5_psr.
#
# PLATFORM is the target for the binary installation.
#
SRCDIR		= ../..
PLATFORM	= sun4u

#
# Propagate the target to build the mapfiles and spec libs
#
all :=		TARGET= all
install :=	TARGET= install

MAPDIR=		../../spec/$(MACH64)/$(PLATFORM)

include		$(SRCDIR)/Makefile.com
include $(SRC)/Makefile.master.64

#
# Defining sun4u turns on sun4u-specific optimization
#
CPPFLAGS += -Dsun4u

.KEEP_STATE:

USR_PSM_LIB_SPARCV9_LINKS 	= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/sparcv9)
INS.slink7=	$(RM) -r $@; $(SYMLINK) ../../$(PLATFORM)/lib/sparcv9 $@ $(CHOWNLINK) $(CHGRPLINK)

$(USR_PSM_LIB_SPARCV9_LINKS):
	-$(INS.slink7)

all:		$(LIBS) $(USR_PSM_LIB_SPARCV9_LINKS)

install:	all $(USR_PSM_LIB64_DIR)/$(LIBS) 

include		$(SRCDIR)/Makefile.targ
