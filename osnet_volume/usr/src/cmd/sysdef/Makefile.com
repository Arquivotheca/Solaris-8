#
#ident	"@(#)Makefile.com	1.2	97/10/25 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sysdef/Makefile.com

PROG=	sysdef
OBJS=	$(PROG).o sdevinfo.o
SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

LDLIBS	+= -lkvm -ldevinfo -lelf

OWNER= root
GROUP= sys
FILEMODE= 02555

CLEANFILES += $(OBJS)

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

lint:	lint_SRCS

%.o:	../%.c
	$(COMPILE.c) $<

clean:
	$(RM) $(CLEANFILES)

include ../../Makefile.targ

