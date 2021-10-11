#
#ident	"@(#)Makefile.com	1.7	98/10/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
# cmd/sgs/dump/Makefile.com

PROG=		dump

include 	../../../Makefile.cmd
include 	../../Makefile.com

COMOBJS=	dump.o		fcns.o		util.o

SRCS=		$(COMOBJS:%.o=../common/%.c)

OBJS =		$(COMOBJS)
.PARALLEL:	$(OBJS)

CPPFLAGS=	-I../common -I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
LLDFLAGS =	'-R$$ORIGIN/../../lib'
LLDFLAGS64 =	'-R$$ORIGIN/../../../lib/$(MACH64)'
LDFLAGS +=	$(LLDFLAGS)
CONVFLAG =	-L../../libconv/$(MACH)
CONVFLAG64 =	-L../../libconv/$(MACH64)
DEMLIB=		-L../../sgsdemangler/$(MACH)
DEMLIB64=	-L../../sgsdemangler/$(MACH64)
LDLIBS +=	$(DEMLIB) -ldemangle $(CONVFLAG) -lconv -lelf -ldl
LINTFLAGS =	-mx -erroff=E_SUPPRESSION_DIRECTIVE_UNUSED $(LDLIBS)
LINTFLAGS64 +=	-mx -errchk=longptr64 $(LDLIBS)
CLEANFILES +=	$(LINTOUT)
