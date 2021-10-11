#
#ident	"@(#)Makefile	1.2	98/10/16 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
# cmd/sgs/size/sparcv9/Makefile
#

PROG=		size

include 	../../../Makefile.cmd
include 	../../Makefile.com

OBJS=		main.o process.o fcns.o

SRCS=		$(OBJS:%.o=../common/%.c)

LDFLAGS +=	'-R$$ORIGIN/../../../lib/$(MACH64)'
INCLIST=	-I../common -I../../include -I../../include/sparc \
		-I$(SRCBASE)/uts/$(ARCH)/sys
DEFLIST=	-DSPARC -DELF
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS +=	-L../../libconv/sparcv9 -lconv -lelf
LINTFLAGS64=	-mx -D__sparcv9 -errchk=longptr64 $(LDLIBS)

CLEANFILES +=	$(LINTOUT)

%.o:		../common/%.c
		$(COMPILE.c) $<

$(ROOTCCSBIN64)/%: %
	$(INS.file)

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
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
