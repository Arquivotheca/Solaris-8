#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.15	99/09/14 SMI"
#
# sgs/libdl/Makefile.com

LIBRARY=	libdl.a
VERS=		.1

COMOBJS=	dl.o
OBJECTS=	$(COMOBJS)

include 	$(SRC)/lib/Makefile.lib
include 	$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	$(MAPDIR)/mapfile
MAPFILES=	$(MAPFILE) $(MAPFILE-FLTR)
MAPOPTS=	$(MAPFILES:%=-M %)

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.

BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) $(LDLIBS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

#$(DYNLIB) :=	LD = $(SGSPROTO)/ld
#$(DYNLIB) :=	DYNFLAGS += -znodump

LINTFLAGS =	-mu
LINTFLAGS64 =	-mu -errchk=longptr64
SRCS=		../common/llib-ldl
LINTSRCS=	$(COMOBJS:%.o=../common/%.c)

CLEANFILES +=
CLOBBERFILES +=	$(DYNLIB)  $(LINTLIB)  $(LINTOUTS)  $(LIBLINKS)
CLOBBERFILES += $(MAPFILE)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
ROOTLINTLIB=	$(LINTLIB:%=$(ROOTLIBDIR)/%)

ROOTDYNLIB64=	$(DYNLIB:%=$(ROOTLIBDIR64)/%)
ROOTLINTLIB64=	$(LINTLIB:%=$(ROOTLIBDIR64)/%)

# A version of this library needs to be placed in /etc/lib to allow
# dlopen() functionality while in single-user mode.

ETCLIBDIR=	$(ROOT)/etc/lib
ETCDYNLIB=	$(DYNLIB:%=$(ETCLIBDIR)/%)

$(ETCDYNLIB) :=	FILEMODE= 755
