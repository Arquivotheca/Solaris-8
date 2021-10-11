#
#ident	"@(#)Makefile	1.13	99/09/12 SMI"
#
# Copyright (c) 1994,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libc_psr/sparc/sun4u/Makefile
#
# Platform specific Makefile for libc_psr.
#
# PLATFORM is the target for the binary installation.
#
SRCDIR		= ../..
PLATFORM	= sun4u

#OBJECTS		= memcpy.o memset.o memcmp.o
OBJECTS		= memcpy.o memset.o memcmp.o hwmuldiv.o 

include		$(SRCDIR)/Makefile.com

MAPDIR=		../../spec/$(MACH)/$(PLATFORM)
MAPFILE=	$(MAPDIR)/mapfile-$(PLATFORM)
MAPFILES=	$(MAPFILE)  $(MAPFILE-FLTR)
MAPOPTS=	$(MAPFILES:%=-M %)
DYNFLAGS +=	$(MAPOPTS)
CLOBBERFILES +=	$(MAPFILE)

ASFLAGS	+=	-xarch=v8plusa

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.
 
BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) $(LDLIBS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

$(DYNLIB):	$(MAPFILES)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) `basename $@`

.KEEP_STATE:

#
# Used when building links in /usr/platform/$(PLATFORM)/lib for libmd5_psr.so.1
#
MODULE		= libc_psr.so.1
LIBC_PSR_LINKS	= $(LINKED_PLATFORMS:%=$(USR_PLAT_DIR)/%/lib/$(MODULE))
INS.slink6	= $(RM) -r $@; $(SYMLINK) ../../$(PLATFORM)/lib/$(MODULE) $@ $(CHOWNLINK) $(CHGRPLINK)

$(LIBC_PSR_LINKS):
	-$(INS.slink6)

all:		$(LIBS) $(LIBC_PSR_LINKS)

install:	all $(USR_PSM_LIBS)

include		$(SRCDIR)/Makefile.targ
