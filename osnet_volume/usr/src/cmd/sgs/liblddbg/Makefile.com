#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.32	99/06/23 SMI"
#

LIBRARY=	liblddbg.a
VERS=		.4

COMOBJS=	args.o		bindings.o	debug.o	\
		dynamic.o	entry.o		elf.o		files.o \
		libs.o		map.o		note.o		phdr.o \
		relocate.o	sections.o	segments.o	shdr.o \
		support.o	syms.o		audit.o		util.o \
		version.o	got.o		move.o
COMOBJS64=	files64.o	map64.o		relocate64.o	sections64.o \
		segments64.o	syms64.o	audit64.o	got64.o \
		move64.o	version64.o
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ)  $(COMOBJS)  $(COMOBJS64)

include		$(SRC)/lib/Makefile.lib
include		$(SRC)/cmd/sgs/Makefile.com

ROOTLIBDIR=	$(ROOT)/usr/lib
MAPFILES=	../common/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
CONVLIB=	-L../../libconv/$(MACH)
DYNFLAGS +=	$(CONVLIB)
LDLIBS +=	-lconv
LINTFLAGS =	-mu -errtags=yes $(ELFFLAG) -L ../../libconv/$(MACH) \
		-erroff=E_SUPPRESSION_DIRECTIVE_UNUSED
LINTFLAGS64 =	-mu -errtags=yes $(ELFFLAG) -errchk=longptr64 \
		-L ../../libconv/$(MACH64) \
		-erroff=E_CAST_INT_TO_SMALL_INT \
		-erroff=E_SUPPRESSION_DIRECTIVE_UNUSED


# A bug in pmake causes redundancy when '+=' is conditionally assigned, so
# '=' is used with extra variables.
# $(DYNLIB) :=  DYNFLAGS += -Yl,$(SGSPROTO)
#
XXXFLAGS=
$(DYNLIB) :=    XXXFLAGS= -Yl,$(SGSPROTO) $(MAPOPTS) -znow
DYNFLAGS +=     $(XXXFLAGS)

native :=	MAPOPTS=
native :=	DYNFLAGS	+= $(CONVLIB)

BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/liblddbg

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/liblddbg.msg
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGALL=	$(SGSMSGCOM)

SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n liblddbg_msg

SRCS=		../common/llib-llddbg
LIBSRCS=	$(COMOBJS:%.o=../common/%.c)  $(BLTDATA)
LIBSRCS64=	$(COMOBJS64:%64.o=%.c)
LINTSRCS=	$(LIBSRCS) ../common/lintsup.c

CLEANFILES +=	$(LINTOUTS) $(LINTLIBS) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB)  $(LINTLIBS) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
