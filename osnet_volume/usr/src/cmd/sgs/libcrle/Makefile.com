#
#ident	"@(#)Makefile.com	1.1	99/08/13 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	libcrle.a
VERS=		.1

COMOBJS=	audit.o		dump.o		util.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ)  $(COMOBJS)


include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

MAPFILES +=	../common/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

CPPFLAGS=	-I. -I../common -I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys $(CPPFLAGS.master)
LDLIBS +=	-lmapmalloc -ldl -lc

LINTFLAGS +=	-m -erroff=E_SUPPRESSION_DIRECTIVE_UNUSED
LINTFLAGS64 +=	-m -errchk=longptr64

DYNFLAGS +=	$(MAPOPTS) -Yl,$(SGSPROTO)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/libcrle

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/libcrle.msg
SGSMSGALL=	$(SGSMSGCOM)
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n libcrle_msg

LIBSRCS=	$(COMOBJS:%.o=../common/%.c)  $(BLTDATA)
LINTSRCS=	$(LIBSRCS)

CLEANFILES +=	$(LINTOUTS) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIB) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
