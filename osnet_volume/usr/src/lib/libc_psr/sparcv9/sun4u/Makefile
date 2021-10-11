#
#pragma ident	"@(#)Makefile	1.8	99/09/12 SMI"
#
# Copyright (c) 1994,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libc_psr/sparcv9/sun4u/Makefile
#
# Platform specific Makefile for libc_psr.
#
# PLATFORM is the target for the binary installation.
#
SRCDIR		= ../..
PLATFORM	= sun4u

#OBJECTS		= memcpy.o memset.o memcmp.o
OBJECTS		= memcpy.o memset.o memcmp.o

include		$(SRCDIR)/Makefile.com

MAPDIR=		../../spec/$(MACH64)/$(PLATFORM)
MAPFILE=	$(MAPDIR)/mapfile-$(PLATFORM)
MAPFILES=	$(MAPFILE)  $(MAPFILE-FLTR)
MAPOPTS=	$(MAPFILES:%=-M %)
DYNFLAGS +=	$(MAPOPTS)
CLOBBERFILES +=	$(MAPFILE)

ASDEFS +=	-D__sparcv9
ASFLAGS	+=	-xarch=v9a

include ../../../../Makefile.master.64

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.
 
BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) $(LDLIBS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

$(DYNLIB):	$(MAPFILES)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) `basename $(MAPFILE)`

.KEEP_STATE:

USR_PSM_LIB_SPARCV9_LINKS	= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/sparcv9)
INS.slink7=	$(RM) -r $@; $(SYMLINK) ../../$(PLATFORM)/lib/sparcv9 $@ $(CHOWNLINK) $(CHGRPLINK)

$(USR_PSM_LIB_SPARCV9_LINKS):
	-$(INS.slink7)

all:		$(LIBS) $(USR_PSM_LIB_SPARCV9_LINKS)

install:	all $(USR_PSM_LIB64_DIR)/$(LIBS)

include		$(SRCDIR)/Makefile.targ
