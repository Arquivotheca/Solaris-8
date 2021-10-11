#
#ident	"@(#)Makefile	1.2	98/10/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All Rights Reserved
#
# cmd/sgs/nm/sparcv9/Makefile
#

PROG=		nm

ARFORMAT=	PORTAR

include 	../../../Makefile.cmd
include 	../../Makefile.com

COMOBJS=	nm.o

OBJS=		$(COMOBJS)

SRCS=		$(COMOBJS:%.o=../common/%.c)

LDFLAGS +=	'-R$$ORIGIN/../../../lib/$(MACH64)'
INCLIST=	-I../../include -I../../include/sparc \
		-I$(SRCBASE)/uts/$(ARCH)/sys
DEFLIST=	-DTARGET=SPARC -DSPARC=1 -D$(ARFORMAT) -DELF
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS +=	-L ../../sgsdemangler/sparcv9 -ldemangle \
		-L ../../libconv/sparcv9 -lconv -lelf -ldl
LINTFLAGS64=	-mx $(LDLIBS) -D__sparcv9 -errchk=longptr64
CLEANFILES +=	$(LINTOUT)


%.o:		../common/%.c
		$(COMPILE.c) $<

$(ROOTCCSBIN64)/%: %
	$(INS.file)

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS) ../../sgsdemangler/sparcv9/libdemangle.a
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

package \
install:	all $(ROOTCCSBINPROG64)

clean:
		$(RM) $(OBJS) $(CLEANFILES)

lint:		$(LINTOUT)

$(LINTOUT):	$(SRCS)
		$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1

include		../../../Makefile.targ
include		../../Makefile.sub.64
