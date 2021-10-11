#
#ident	"@(#)Makefile.com	1.12	99/06/23 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.

PROG=		elfdump

include		$(SRC)/cmd/Makefile.cmd
include		$(SRC)/cmd/sgs/Makefile.com

COMOBJ=		elfdump.o
BLTOBJ=		msg.o

OBJS=		$(BLTOBJ) $(COMOBJ)

MAPFILE=	../common/mapfile-vers

CPPFLAGS=	-I. -I../../include -I../../include/$(MACH) \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		$(CPPFLAGS.master)
LLDFLAGS =	'-R$$ORIGIN/../../lib'
LLDFLAGS64 =	'-R$$ORIGIN/../../../lib/$(MACH64)'
LDFLAGS +=	-Yl,$(SGSPROTO) -M $(MAPFILE) $(LLDFLAGS)
LDDBGFLAG =	-L../../liblddbg/$(MACH)
LDDBGFLAG64 =	-L../../liblddbg/$(MACH64)
CONVFLAG =	-L../../libconv/$(MACH)
CONVFLAG64 =	-L../../libconv/$(MACH64)
LDLIBS +=	-lelf $(LDDBGFLAG) $(LDDBG_LIB) $(CONVFLAG) -lconv $(INTLLIB)

LINTFLAGS =	-mx -erroff=E_SUPPRESSION_DIRECTIVE_UNUSED
LINTFLAGS64 =	-mx -errchk=longptr64

BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/elfdump

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/elfdump.msg
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGALL=	$(SGSMSGCOM)
SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n elfdump_msg

SRCS=		$(COMOBJ:%.o=../common/%.c) $(BLTDATA)
LINTSRCS=	$(SRCS) ../common/lintsup.c

CLEANFILES +=	$(LINTOUTS) $(BLTFILES)


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	INTLLIB = /usr/lib/libintl.so.1
