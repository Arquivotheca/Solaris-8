#
#ident	"@(#)Makefile.com	1.20	99/06/23 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	libldstab.a
VERS=		.1

COMOBJS=	stab.o
DUPOBJS+=
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ) $(COMOBJS) $(DUPOBJS)

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

SRCBASE=	../../../..

MAPFILES=	../common/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
LDLIBS +=	-lelf -lc
DYNFLAGS +=	$(MAPOPTS)

LINTFLAGS +=	-m -erroff=E_SUPPRESSION_DIRECTIVE_UNUSED
LINTFLAGS64 +=	-m -errchk=longptr64


# A bug in pmake causes redundancy when '+=' is conditionally assigned, so
# '=' is used with extra variables.
# $(DYNLIB) :=        DYNFLAGS += -Yl,$(SGSPROTO)
#
XXXFLAGS=
$(DYNLIB) :=	XXXFLAGS= -Yl,$(SGSPROTO)

DYNFLAGS +=	$(XXXFLAGS)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/libldstab

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/libldstab.msg
SGSMSGALL=	$(SGSMSGCOM)
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n libldstab_msg

SRCS=		$(COMOBJS:%.o=../common/%.c) $(BLTDATA)
LINTSRCS=	$(SRCS)

CLEANFILES +=	$(LINTOUTS) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
