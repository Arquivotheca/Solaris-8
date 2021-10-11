#
#ident	"@(#)Makefile.com	1.46	99/10/22 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.

LIBRARY=	libld.a
VERS=		.2

G_MACHOBJS=	doreloc.o
L_MACHOBJS= 	machrel.o	machsym.o
L_MACHSRCS= 	machrel.c	machsym.c
COMOBJS=	entry.o		files.o		globals.o	libs.o \
		order.o		outfile.o	place.o		relocate.o \
		resolve.o	sections.o	support.o	sunwmove.o \
		syms.o		update.o	util.o		version.o \
		args.o		debug.o		ldentry.o	ldglobals.o \
		ldlibs.o	ldmain.o	exit.o		map.o
COMOBJS64=	$(COMOBJS:%.o=%64.o)
BLTOBJ=		msg.o

OBJECTS=	$(BLTOBJ)  $(G_MACHOBJS)  $(L_MACHOBJS)  $(COMOBJS)

include 	$(SRC)/lib/Makefile.lib
include 	$(SRC)/cmd/sgs/Makefile.com

MAPFILES=	../common/mapfile-vers
MAPOPTS=	$(MAPFILES:%=-M %)
ROOTLIBDIR=	$(ROOT)/usr/lib

CPPFLAGS=	-I. -I../common -I../../include \
		-I../../include/$(MACH) \
		-I$(SRCBASE)/uts/common/krtld \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master) -D__EXTENSIONS__
DBGLIB =	-L ../../liblddbg/$(MACH)
CONVLIB =	-L ../../libconv/$(MACH)
ELFLIB =	-L ../../libelf/$(MACH)

LLDLIBS=	$(LDDBG_LIB) -lelf -ldl
LDLIBS +=	-lconv $(LLDLIBS) $(INTLLIB) -lc
LINTFLAGS=	-mu -L ../../liblddbg/$(MACH) -L ../../libconv/$(MACH) \
		-erroff=E_SUPPRESSION_DIRECTIVE_UNUSED -errtags=yes
LINTFLAGS64 =	-mu -errchk=longptr64 \
		-L ../../liblddbg/$(MACH64) -L ../../libconv/$(MACH64) \
		-errtags=yes \
		-erroff=E_CAST_INT_TO_SMALL_INT \
		-erroff=E_SUPPRESSION_DIRECTIVE_UNUSED
DYNFLAGS +=	-Wl,-Bdirect -zlazyload $(MAPOPTS) -Yl,$(SGSPROTO) '-R$$ORIGIN'

native:=        DYNFLAGS	+= $(CONVLIB)

BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/libld

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/libld.msg
SGSMSGSPARC=	../common/libld.sparc.msg
SGSMSGINTEL=	../common/libld.intel.msg
SGSMSG32=	../common/libld.32.msg
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGALL=	$(SGSMSGCOM) $(SGSMSGSPARC) $(SGSMSGINTEL) $(SGSMSG32)

SGSMSGFLAGS1=	$(SGSMSGFLAGS) -m $(BLTMESG)
SGSMSGFLAGS2=	$(SGSMSGFLAGS) -h $(BLTDEFS) -d $(BLTDATA) -n libld_msg

SRCS=		../common/llib-lld
LIBSRCS=	$(L_MACHSRCS) $(COMOBJS:%.o=../common/%.c) $(BLTDATA) \
		$(G_MACHOBJS:%.o=$(SRCBASE)/uts/$(PLAT)/krtld/%.c)
LINTSRCS=	$(LIBSRCS)

CLEANFILES +=	$(LINTOUTS) $(BLTFILES)
CLOBBERFILES +=	$(DYNLIB) $(LINTLIBS) $(LIBLINKS)

ROOTDYNLIB=	$(DYNLIB:%=$(ROOTLIBDIR)/%)
