#
#ident	"@(#)Makefile 1.9 97/11/14 SMI;"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
# cmd/auditreduce/Makefile

PROG= auditreduce
OBJS= main.o option.o proc.o time.o token.o regex2.o

include ../Makefile.cmd

TEXT_DOMAIN=SUNW_OST_OSCMD
POFILE=auditreduce.po
POFILES=main.po option.po proc.po time.po token.po

SRCS= $(OBJS:.o=.c)

LDLIBS += -lnsl -lbsm

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDFLAGS) $(LDLIBS)

$(POFILE): $(POFILES)
	rm -f $@
	cat $(POFILES) > $@

lint: lint_SRCS

clean:
	rm -f $(OBJS)
	rm -f $(PROG)
	rm -f $(POFILES) $(POFILE)

tags: $(SRCS)
	ctags $(SRCS)

include ../Makefile.targ
