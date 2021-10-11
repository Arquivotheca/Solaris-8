#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile.com	1.3	99/03/26 SMI"
#
# lib/libw/Makefile.com

LIBRARY=	libw.a
VERS=		.1

ROOTARLINK=	$(ROOTLIBDIR)/$(LIBRARY)

# include library definitions
include		../../Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
MAPFILES=	$(MAPFILE) $(MAPFILE-FLTR)
MAPOPTS=	$(MAPFILES:%=-M %)

DYNFLAGS += -F libc.so.1 $(MAPOPTS)

LIBS = $(DYNLIB)

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

CLOBBERFILES += $(LIBS) $(ROOTARLINK) $(ROOTLIBS) $(ROOTLINKS)	\
	$(ROOTLIBS64) $(ROOTLINKS64) $(MAPFILE)

# include library targets
include ../../Makefile.targ

$(ROOTARLINK):
		-$(RM) $@; $(SYMLINK) $(LIBNULL) $@

$(DYNLIB):	$(MAPFILE)

$(MAPFILE):
	@cd $(MAPDIR); $(MAKE) mapfile
