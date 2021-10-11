#
#ident	"@(#)Makefile.com	1.7	97/07/28 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/sgs/gprof/sparc/Makefile
#

include 	../../../Makefile.cmd

COMOBJS=	gprof.o arcs.o dfn.o lookup.o calls.o \
		printgprof.o printlist.o readelf.o
WHATOBJS=	whatdir.o

OBJS=		$(COMOBJS) $(WHATOBJS)
BLURBS=		gprof.callg.blurb gprof.flat.blurb
SRCS=		$(COMOBJS:%.o=../common/%.c) \
		$(WHATOBJS:%.o=../../whatdir/common/%.c)

INCLIST=	-I../common -I../../include -I../../include/$(MACH)
DEFLIST=	-DELF_OBJ -DELF
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS +=	../../sgsdemangler/`mach`/libdemangle.a -ldl
LINTFLAGS +=	-n $(LDLIBS)
CLEANFILES +=	$(LINTOUT)

ROOTCCSBLURB=	$(BLURBS:%=$(ROOTCCSBIN)/%)

$(ROOTCCSBLURB) :=	FILEMODE=	444

$(ROOTCCSBIN)/%: ../common/%
		$(INS.file)

%.o:		../common/%.c
		$(COMPILE.c) $<

%.o:		../../whatdir/common/%.c
		$(COMPILE.c) $<
.PARALLEL: $(OBJS)
