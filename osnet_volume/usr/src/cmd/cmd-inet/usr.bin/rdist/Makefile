#
#ident	"@(#)Makefile	1.9	98/02/12 SMI"
#
# Copyright (c) 1990,1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.bin/rdist/Makefile

PROG=	rdist
OBJS=	docmd.o expand.o lookup.o main.o server.o
YOBJS=	gram.o
SRCS=	$(OBJS:.o=.c)
YSRCS=	$(YOBJS:.o=.y)

include ../../../Makefile.cmd

CPPFLAGS +=	-DSYSV -DSTRNET -DBSD_COMP
LDLIBS +=	-lsocket -lnsl

ROOTSYMLINK=	$(ROOT)/usr/ucb/$(PROG)

# conditional assignments
$(ROOTPROG) :=      FILEMODE=       04555
$(ROOTPROG) :=      OWNER=          root

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS) $(YOBJS)
	$(LINK.c) $(OBJS) $(YOBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG) $(ROOTSYMLINK)

$(ROOTSYMLINK):
	$(RM) $@; $(SYMLINK) ../bin/$(PROG) $@

clean:
	$(RM) $(OBJS) $(YOBJS)

lint:
	$(YACC.y) $(YSRCS)
	$(LINT.c) $(SRCS) y.tab.c $(LDLIBS)
	$(RM) y.tab.c


include ../../../Makefile.targ
