#
#ident	"@(#)Makefile	1.10	98/04/08 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/sgs/prof/sparc/Makefile
#

PROG=		prof

include 	../../../Makefile.cmd

COMOBJS=	prof.o profv.o lookup.o rdelf.o \
		symintOpen.o symintClose.o symintUtil.o symintErr.o symintLoad.o

OBJS=		$(COMOBJS)

SRCS=		$(COMOBJS:%.o=../common/%.c)
		

INCLIST=	-I../common -I../../include -I../../include/sparc
DEFLIST=	-DSPARC -DELF
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS +=	../../sgsdemangler/`mach`/libdemangle.a -lelf -ldl
LINTFLAGS +=	$(LDLIBS)
CLEANFILES +=	$(LINTOUT)


%.o:		../common/%.c
		$(COMPILE.c) $<

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install:	all $(ROOTCCSBINPROG)

clean:
		$(RM) $(OBJS) $(CLEANFILES)

lint:		$(LINTOUT)

$(LINTOUT):	$(SRCS)
		$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1

include		../../../Makefile.targ
