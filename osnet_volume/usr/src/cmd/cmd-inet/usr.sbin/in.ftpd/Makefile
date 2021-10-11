#
#ident	"@(#)Makefile	1.28	99/11/12 SMI"
#
# Copyright (c) 1986-1999 by Sun Microsystems, Inc.
# All rights reserved.
#

PROG=		in.ftpd
OBJS1=		ftpd.o glob.o logwtmp.o popen.o
OTHEROBJ=	ftpcmd.o
ETCFILES=	ftpusers
OBJS=		$(OBJS1) $(OTHEROBJ)
SRCS=		$(OBJS1:%.o=%.c)
ROOTETCFILES=	$(ETCFILES:%=$(ROOTETC)/%)

include		../../../Makefile.cmd

$(ROOTETC)/ftpusers :=	FILEMODE=	0644
$(ROOTETC)/ftpusers :=	GROUP=		sys

CPPFLAGS +=	-DBSD_COMP -D_FILE_OFFSET_BITS=64 -I../../../../lib/libcmd/inc
LDLIBS +=	-lcmd -lsocket -lnsl -lbsm -lpam -ldl
YFLAGS +=	-d

.KEEP_STATE:

all:		$(PROG) $(ETCFILES)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install:	all $(ROOTUSRSBINPROG) $(ROOTETCFILES)

clean:
		$(RM) $(OBJS) y.tab.h y.tab.c ftpcmd.c

lint:		lint_SRCS

include		../../../Makefile.targ
