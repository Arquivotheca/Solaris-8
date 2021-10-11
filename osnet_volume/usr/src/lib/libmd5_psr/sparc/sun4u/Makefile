#
#ident	"@(#)Makefile	1.2	99/09/10 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libmd5_psr/sparc/sun4u/Makefile
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

MAPDIR=		../../spec/$(MACH)/$(PLATFORM)

include		$(SRCDIR)/Makefile.com

#
# Defining sun4u turns on sun4u-specific optimization
#
CPPFLAGS += -Dsun4u
CFLAGS += -xarch=v8plusa

.KEEP_STATE:

#
# Used when building links in /usr/platform/$(PLATFORM)/lib for libmd5_psr.so.1
#
MODULE	= libmd5_psr.so.1

LIBMD5_LINKS	= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/$(MODULE))
INS.slink6	= $(RM) -r $@; $(SYMLINK) ../../$(PLATFORM)/lib/$(MODULE) $@ $(CHOWNLINK) $(CHGRPLINK)

$(LIBMD5_LINKS):
	-$(INS.slink6)

all:		$(LIBS) $(LIBMD5_LINKS)

install:	all $(USR_PSM_LIBS)

include		$(SRCDIR)/Makefile.targ
