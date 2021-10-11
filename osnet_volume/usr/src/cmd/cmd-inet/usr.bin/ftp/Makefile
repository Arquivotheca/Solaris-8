#
#ident	"@(#)Makefile	1.13	99/08/17 SMI"
#
# Copyright (c) 1989,1990,1992,1995,1996 by Sun Microsystems, Inc.
# ALl rights reserved.
#

PROG= ftp
OBJS=	cmds.o cmdtab.o ftp.o getpass.o glob.o main.o pclose.o \
	ruserpass.o domacro.o
SRCS= $(OBJS:%.o=%.c)

include ../../../Makefile.cmd

CPPFLAGS +=	-v -D_FILE_OFFSET_BITS=64
LDLIBS +=	-lsocket -lnsl

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG)

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../Makefile.targ
