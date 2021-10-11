#
#ident	"@(#)Makefile	1.6	99/10/14 SMI"
#
# Copyright (c) 1989-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/truss/sparcv9/Makefile
#

PROG=	truss

OBJS=	main.o listopts.o ipc.o actions.o expound.o codes.o print.o \
	ramdata.o systable.o procset.o xstat.o fcall.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd
include ../../Makefile.cmd.64

CFLAGS64	+= -v

LDLIBS		+= -lproc -lrtld_db -lnsl -ldl

lint	:= LINTFLAGS64 = -x -D__sparcv9

CPPFLAGS += -D_LARGEFILE64_SOURCE=1

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG64)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../../Makefile.targ
