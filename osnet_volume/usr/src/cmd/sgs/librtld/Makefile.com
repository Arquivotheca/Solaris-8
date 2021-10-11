#
#ident	"@(#)Makefile.com	1.17	99/06/23 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	librtld.a
VERS=		.1

MACHOBJS=	_relocate.o
COMOBJS=	dldump.o	dynamic.o	relocate.o	syms.o \
		util.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ)  $(MACHOBJS)  $(COMOBJS)


include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPFILE=	../common/mapfile-vers

ROOTLIBDIR=	$(ROOT)/usr/lib

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		-I$(SRCBASE)/uts/common/krtld \
		$(CPPFLAGS.master)
DBGLIB =	-L ../../liblddbg/$(MACH)
ELFLIB =	-L ../../libelf/$(MACH)
DYNFLAGS +=	$(DBGLIB) $(ELFLIB) -zlazyload
LDLIBS +=	-lelf -lc

LINTFLAGS =	-mu -erroff=E_SUPPRESSION_DIRECTIVE_UNUSED
LINTFLAGS64 =	-mu -errchk=longptr64

# A bug in pmake causes redundancy when '+=' is conditionally assigned, so
# '=' is used with extra variables.
#
XXXFLAGS=
$(DYNLIB) :=	XXXFLAGS= -Yl,$(SGSPROTO) -M $(MAPFILE)
DYNFLAGS +=	$(XXXFLAGS)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/librtld

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/librtld.msg
SGSMSGALL=	$(SGSMSGCOM)
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n librtld_msg

SRCS=		../common/llib-lrtld
LINTSRCS=	$(MACHOBJS:%.o=%.c)  $(COMOBJS:%.o=../common/%.c) \
		$(BLTDATA) ../common/lintsup.c

CLEANFILES +=	$(BLTFILES) $(LINTOUTS)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
