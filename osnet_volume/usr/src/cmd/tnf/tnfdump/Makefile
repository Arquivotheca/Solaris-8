#
#ident	"@(#)Makefile	1.5	96/03/19 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/tnf/tnfdump/Makefile
#

PROG=		tnfdump

OBJS.c=		main.o cooked.o table.o
OBJS=		$(OBJS.c)

SRCS= $(OBJS.c:%.o=%.c)

include	../../Makefile.cmd

POFILES= $(OBJS.c:%.o=%.po)

CPPFLAGS +=	-I../../../lib/libtnfprobe
LDLIBS +=	-ltnf

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTBIN) $(ROOTPROG)

$(ROOTBIN):
	$(INS.dir)

$(POFILE):      $(POFILES)
	$(RM)	$@
	cat     $(POFILES)      > $@

clean:
	$(RM) $(OBJS)

lint: $(SRCS) lint_SRCS

include	../../Makefile.targ
