#
# Copyright (c) 1995,1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.6	99/03/26 SMI"
#
# lib/libsys/Makefile.com

LIBRARY=	libsys.a
VERS=		.1

COMOBJ=		libsys.o
OBJECTS=	$(COMOBJ)  $(MACHOBJ)

include 	../../../lib/Makefile.lib

MAPFILE=	$(MAPDIR)/mapfile
MAPFILES=	$(MAPFILE) $(MAPFILE-FLTR)
MAPOPTS=	$(MAPFILES:%=-M %)

# Define libsys to be a filter on libc.  The ABI requires the runtime linker as
# the soname.

DYNFLAGS +=	-F/usr/lib/libc.so.1 $(MAPOPTS)
SONAME=		/usr/lib/ld.so.1

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) $(LDLIBS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

pics/%.o :=	ASFLAGS += -K pic

COMSRC=		$(COMOBJ:%.o=%.c)
MACHSRC=	$(MACHOBJ:%.o=%.s)

CLOBBERFILES +=	$(DYNLIB)  $(LIBLINKS)  $(COMSRC)  $(MACHSRC) $(MAPFILE)
