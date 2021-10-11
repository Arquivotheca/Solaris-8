#
#ident	"@(#)Makefile	1.6	96/03/19 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/m4/i386/Makefile
#

PROG=		m4
XPG4PROG=	m4

include 	../../../Makefile.cmd

COBJS=		m4.o  m4ext.o  m4macs.o

YOBJS=		m4objs/m4y.o
XPG4YOBJS=	m4objs.xpg4/m4y_xpg4.o

OBJS=		$(COBJS:%=m4objs/%) $(YOBJS)
XPG4OBJS=	$(COBJS:%=m4objs.xpg4/%) $(XPG4YOBJS)
ALLOBJS=	$(OBJS) $(XPG4OBJS)

BLTSRCS=	../common/m4y.c ../common/m4y_xpg4.c

SRCS=		$(COBJS:%.o=../common/%.c) $(BLTSRCS)

INCLIST=	-I../common
DEFLIST=	-DELF
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)

CLEANFILES +=	$(LINTOUT) $(BLTSRCS)

m4objs.xpg4/%.o := CPPFLAGS += -DXPG4

m4objs/%.o m4objs.xpg4/%.o: ../common/%.c
		$(COMPILE.c) -o $@ $<

.KEEP_STATE:

all:		$(PROG) $(XPG4)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

$(XPG4):	$(XPG4OBJS)
		$(LINK.c) $(XPG4OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

$(OBJS):	m4objs

$(XPG4OBJS):	m4objs.xpg4

m4objs m4objs.xpg4:
		-@mkdir -p $@

install:	all $(ROOTCCSBINPROG) $(ROOTXPG4PROG)

clean:
		$(RM) $(ALLOBJS) $(CLEANFILES)

lint:		$(LINTOUT)

$(LINTOUT):	$(SRCS)
		$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1

include		../../../Makefile.targ
