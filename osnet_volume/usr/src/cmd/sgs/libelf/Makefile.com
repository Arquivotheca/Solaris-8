#
# Copyright (c) 1996-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.40	99/06/23 SMI"
#
# sgs/libelf/Makefile.com


LIBRARY=	libelf.a
VERS=		.1
M4=		m4

MACHOBJS=
COMOBJS=	ar.o		begin.o		cntl.o		cook.o \
		data.o		end.o		fill.o		flag.o \
		getarhdr.o	getarsym.o	getbase.o	getdata.o \
		getehdr.o	getident.o	getphdr.o	getscn.o \
		getshdr.o	hash.o		input.o		kind.o \
		ndxscn.o	newdata.o	newehdr.o	newphdr.o \
		newscn.o	next.o		nextscn.o	output.o \
		rand.o		rawdata.o	rawfile.o	rawput.o \
		strptr.o	update.o	error.o		gelf.o \
		clscook.o	checksum.o
CLASSOBJS=	clscook64.o	newehdr64.o	newphdr64.o	update64.o
BLTOBJS=	msg.o		xlate.o		xlate64.o
MISCOBJS=	String.o	args.o		demangle.o	nlist.o \
		nplist.o
MISCOBJS64=	nlist.o

OBJECTS=	$(BLTOBJS)  $(MACHOBJS)  $(COMOBJS)  $(CLASSOBJS) $(MISCOBJS)

DEMOFILES=	Makefile	README		acom.c		dcom.c \
		pcom.c		tpcom.c

include $(SRC)/lib/Makefile.lib
include $(SRC)/cmd/sgs/Makefile.com

WARLOCKFILES=	$(OBJECTS:%.o=wlocks/%.ll)

MAPFILE=	$(MAPDIR)/mapfile
MAPFILES=	$(MAPFILE)
MAPOPTS=	$(MAPFILES:%=-M %)

CLOBBERFILES +=	$(MAPFILE)

CPPFLAGS=	-I. -I../common $(CPPFLAGS.master) -I../../include \
			-I$(SRCBASE)/uts/$(ARCH)/sys
DYNFLAGS +=	$(MAPOPTS)
LDLIBS +=	$(LIBTHREADFLAG) -lc

LINTFLAGS =	-mu -errtags=yes \
		-erroff=E_BAD_PTR_CAST_ALIGN \
		-erroff=E_SUPPRESSION_DIRECTIVE_UNUSED

LINTFLAGS64 =	-mu -errchk=longptr64 -errtags=yes \
		-erroff=E_CAST_INT_TO_SMALL_INT

BUILD.AR=	$(RM) $@ ; \
		$(AR) q $@ `$(LORDER) $(OBJECTS:%=$(DIR)/%)| $(TSORT)`
		$(POST_PROCESS_A)


BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/libelf

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/libelf.msg
SGSMSG32=	../common/libelf.32.msg
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGALL=	$(SGSMSGCOM) $(SGSMSG32)

SGSMSGFLAGS1=	$(SGSMSGFLAGS) -m $(BLTMESG)
SGSMSGFLAGS2=	$(SGSMSGFLAGS) -h $(BLTDEFS) -d $(BLTDATA) -n libelf_msg

BLTSRCS=	$(BLTOBJS:%.o=%.c)
LIBSRCS=	$(COMOBJS:%.o=../common/%.c)  $(MISCOBJS:%.o=../misc/%.c) \
		$(MACHOBJS:%.o=%.c)  $(BLTSRCS)
SRCS=		../common/llib-lelf
LINTSRCS=	$(LIBSRCS) ../common/lintsup.c

ROOTDEMODIR=	$(ROOT)/usr/demo/ELF
ROOTDEMOFILES=	$(DEMOFILES:%=$(ROOTDEMODIR)/%)

LIBS +=		$(DYNLIB) $(LINTLIB)

CLEANFILES +=	$(LINTOUTS) $(BLTSRCS) $(BLTFILES) $(WARLOCKFILES)

$(ROOTDEMODIR) :=	OWNER =		root
$(ROOTDEMODIR) :=	GROUP =		bin
$(ROOTDEMODIR) :=	DIRMODE =	755

.PARALLEL:	$(LIBS) $(ROOTDEMOFILES)
