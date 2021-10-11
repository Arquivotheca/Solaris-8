#
#ident	"@(#)Makefile.com	1.35	99/06/23 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.

PROG=		ld

include 	$(SRC)/cmd/Makefile.cmd
include 	$(SRC)/cmd/sgs/Makefile.com

COMOBJS=	ld.o
BLTOBJ=		msg.o

OBJS =		$(BLTOBJ) $(MACHOBJS) $(COMOBJS)
.PARALLEL:	$(OBJS)

MAPFILE=	../common/mapfile-vers


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6
INTLLIB=
package :=	INTLLIB = /usr/lib/libintl.so.1


CPPFLAGS=	-I. -I../common -I../../include \
		-I$(SRCBASE)/uts/$(ARCH)/sys \
		-I../../include/$(MACH) \
		$(CPPFLAGS.master)
LLDFLAGS =	'-R$$ORIGIN/../../lib'
LLDFLAGS64 =	'-R$$ORIGIN/../../../lib/$(MACH64)'
LDFLAGS +=	-zlazyload -Wl,-Bdirect -Yl,$(SGSPROTO) -M $(MAPFILE)
LLDLIBS=	-lelf -ldl $(INTLLIB) \
		-L ../../libconv/$(MACH) -lconv
LLDLIBS64=	-lelf -ldl \
		-L ../../libconv/$(MACH64) -lconv

LDFLAGS +=	$(LLDFLAGS)
LDLIBS +=	$(LLDLIBS)
LINTFLAGS =	-mx
LINTFLAGS64 =	-mx -errchk=longptr64
CLEANFILES +=	$(LINTOUTS)

native :=	LDFLAGS = -R$(SGSPROTO) -znoversion
native :=	LLDLIBS = -L$(SGSPROTO) -lelf -ldl -L ../../libconv/$(MACH) \
		    -lconv /usr/lib/libintl.so.1

BLTDEFS=	msg.h
BLTDATA=	msg.c
BLTMESG=	$(SGSMSGDIR)/ld

BLTFILES=	$(BLTDEFS) $(BLTDATA) $(BLTMESG)

SGSMSGCOM=	../common/ld.msg
SGSMSGTARG=	$(SGSMSGCOM)
SGSMSGALL=	$(SGSMSGCOM)
SGSMSGFLAGS +=	-h $(BLTDEFS) -d $(BLTDATA) -m $(BLTMESG) -n ld_msg

SRCS=		$(MACHOBJS:%.o=%.c)  $(COMOBJS:%.o=../common/%.c)  $(BLTDATA)
LINTSRCS=	$(SRCS) ../common/lintsup.c

ROOTCCSBIN=	$(ROOT)/usr/ccs/bin
ROOTCCSBINPROG=	$(PROG:%=$(ROOTCCSBIN)/%)

CLEANFILES +=	$(BLTFILES)

FILEMODE=	0755
