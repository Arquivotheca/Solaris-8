#
#ident	"@(#)Makefile.com	1.13	99/06/23 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	librtld_db.a
VERS=		.1

COMOBJS=	rtld_db.o	rd_elf.o
COMOBJS64=	rd_elf64.o
MACHOBJS=	rd_mach.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ) $(COMOBJS) $(MACHOBJS)

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPDIR=		../spec/$(MACH)
MAPFILE=	$(MAPDIR)/mapfile

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
DYNFLAGS +=	-Wl,-M$(MAPFILE)
ZDEFS=

LINTFLAGS =	-mu
LINTFLAGS64 =	-mu -errchk=longptr64


BLTDEFS=	msg.h
BLTDATA=	msg.c

BLTFILES=	$(BLTDEFS) $(BLTDATA)

SGSMSGCOM=	../common/librtld_db.msg
SGSMSGINTEL=	../common/librtld_db.intel.msg
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGALL=	$(SGSMSGCOM)
SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA)

SRCS=		../common/llib-lrtld_db
LINTSRCS=	$(COMOBJS:%.o=../common/%.c) $(MACHOBJS:%.o=%.c) \
		$(BLTDATA) ../common/lintsup.c

CLEANFILES +=	$(BLTFILES) $(LINTOUTS)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB) $(MAPFILE)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
