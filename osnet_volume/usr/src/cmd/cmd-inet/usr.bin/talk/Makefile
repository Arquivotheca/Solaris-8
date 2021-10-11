#
#ident	"@(#)Makefile	1.12	97/06/06 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG=	talk
OBJS=	talk.o get_names.o display.o io.o ctl.o init_disp.o \
	msgs.o get_addrs.o ctl_transact.o invite.o look_up.o
SRCS= $(OBJS:%.o=%.c)

include ../../../Makefile.cmd

#
# Message catalog
#
POFILE= talk.po
#

CFLAGS +=	-v
CPPFLAGS +=	-DSYSV -DSTRNET -DBSD_COMP
LDLIBS +=	-lcurses -lsocket -lnsl

.KEEP_STATE:

.PARALLEL: $(OBJS)

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

#
# message catalog
#
_msg: $(POFILE)

$(POFILE): $(SRCS)
	$(RM) talk.po
	$(COMPILE.cpp) 	$(SRCS)	> $(POFILE).i
	$(XGETTEXT) $(XGETFLAGS)	$(POFILE).i	
	sed "/^domain/d" messages.po > $@
	$(RM) messages.po $(POFILE).i

#
install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../Makefile.targ
