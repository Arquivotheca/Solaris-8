#
# @(#)Makefile 1.8 96/03/18 SMI; SunOS 5.X
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# cmd/dminfo/Makefile

PROG= dminfo

OBJS=	dminfo.o
SRCS=	$(OBJS:%.o=%.c)

include ../Makefile.cmd

TEXT_DOMAIN=SUNW_BSM_DMINFO
POS=dminfo.po
include ../Makefile.cmd.bsm

FILEMODE=	755

LDLIBS += -lnsl -lbsm

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

clean:
	rm -rf $(OBJS) $(POS)

lint:	lint_PROG

include ../Makefile.targ
